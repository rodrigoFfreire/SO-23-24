#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>

#include "constants.h"
#include "operations.h"
#include "eventlist.h"
#include "parser.h"
#include "utils.h"
#include "threaded.h"

int process_job(char *job_filepath, char *out_filepath, unsigned int access_delay, unsigned long int max_threads) {
  int job_fd = open(job_filepath, O_RDONLY);
  if (job_fd < 0) {
    fprintf(stderr, "Failed to open %s file\n", job_filepath);
    close(job_fd);
    return 1;
  }

  int out_fd = open(out_filepath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (out_fd < 0) {
    fprintf(stderr, "Failed to open %s file\n", out_filepath);
    close(job_fd);
    close(out_fd);
    return 1;
  }

  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  unsigned int event_id, delay;
  unsigned long used_threads = 0;
  
  Ems_t ems = {NULL, 0};
  if (ems_init(&ems, access_delay)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    close(job_fd);
    close(out_fd);
    return 1;
  }

  pthread_t *threads; 
  if (NULL == (threads = (pthread_t*) safe_malloc(sizeof(pthread_t) * max_threads))) {
    ems_terminate(&ems);
    close(job_fd);
    close(out_fd);
    return 1;
  }

  char eof = 0;
  char barrier = 0;
  while (!eof) {
    while (used_threads < max_threads && !eof) {
      switch (get_next(job_fd)) {
        case CMD_CREATE:
          if (parse_create(job_fd, &event_id, &num_rows, &num_columns) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }
          CommandArgs_t *args_create = (CommandArgs_t*) malloc(sizeof(CommandArgs_t));
          *args_create = (CommandArgs_t){&ems, event_id, 0, num_rows, num_columns, 0, 0, 0};
          pthread_create(&threads[used_threads], NULL, thread_ems_create, (void*)args_create);
          used_threads++;

          break;

        case CMD_RESERVE:
          if (!(num_coords = parse_reserve(job_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys))) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }
          CommandArgs_t *args_reserve = (CommandArgs_t*) malloc(sizeof(CommandArgs_t));
          *args_reserve = (CommandArgs_t){&ems, event_id, num_coords, 0, 0, NULL, NULL, 0};

          args_reserve->xs_cpy = (size_t*) safe_malloc(sizeof(size_t) * num_coords);
          args_reserve->ys_cpy = (size_t*) safe_malloc(sizeof(size_t) * num_coords);          
          memcpy(args_reserve->xs_cpy, xs, sizeof(size_t) * num_coords);
          memcpy(args_reserve->ys_cpy, ys, sizeof(size_t) * num_coords);

          pthread_create(&threads[used_threads], NULL, thread_ems_reserve, (void*)args_reserve);
          used_threads++;

          break;

        case CMD_SHOW:
          if (parse_show(job_fd, &event_id) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }
          CommandArgs_t *args_show = (CommandArgs_t*) malloc(sizeof(CommandArgs_t));
          *args_show = (CommandArgs_t){&ems, event_id, 0, 0, 0, 0, 0, out_fd};
          pthread_create(&threads[used_threads], NULL, thread_ems_show, (void*)args_show);
          used_threads++;

          break;

        case CMD_LIST_EVENTS: {
          CommandArgs_t *args_list = (CommandArgs_t*) malloc(sizeof(CommandArgs_t));
          *args_list = (CommandArgs_t){&ems, 0, 0, 0, 0, 0, 0, out_fd};
          pthread_create(&threads[used_threads], NULL, thread_ems_list_events, (void*)args_list);
          used_threads++;
          
          break;
        }

        case CMD_WAIT:
          if (parse_wait(job_fd, &delay, NULL) == -1) { // thread_id is not implemented
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          struct timespec delay_ms = delay_to_timespec(delay);
          if (delay > 0) {
            printf("Waiting...\n");
            nanosleep(&delay_ms, NULL);
          }

          break;

        case CMD_INVALID:
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;

        case CMD_HELP:
          printf(
              "Available commands:\n"
              "  CREATE <event_id> <num_rows> <num_columns>\n"
              "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
              "  SHOW <event_id>\n"
              "  LIST\n"
              "  WAIT <delay_ms> [thread_id]\n" // thread_id is not implemented
              "  BARRIER\n"
              "  HELP\n");

          break;

        case CMD_BARRIER:
          barrier = 1;
          break;
        case CMD_EMPTY:
          break;

        case EOC:
          eof = 1;
          break;
      }
      if (barrier) {
        barrier = 0;
        break;
      }
    }
    for (unsigned long i = 0; i < max_threads && i < used_threads; i++) {
      pthread_join(threads[i], NULL);
    }
    used_threads = 0;
  }

  free(threads);

  ems_terminate(&ems);
  
  close(job_fd);
  close(out_fd);

  return 0;
}


int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  struct dirent *dir_entry;

  if (argc < 4) {
    fprintf(stderr, "Not enough arguments were provided\n");
    return 1;
  }

  char *endptr;

  unsigned long int max_proc = strtoul(argv[2], &endptr, 10);
  if (*endptr != '\0' || max_proc > (unsigned long )sysconf(_SC_CHILD_MAX)
      || max_proc <= 0) {
    fprintf(stderr, "Invalid max processes or value too large\n");
    return 1;
  }

  unsigned long int max_threads = strtoul(argv[3], &endptr, 10);
  if (*endptr != '\0' || max_threads <= 0 || max_threads > UINT_MAX) {
    fprintf(stderr, "Invalid max threads or value too large\n");
    return 1;
  }

  if (argc > 4) {
    unsigned long int delay = strtoul(argv[3], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  DIR *dir = opendir(argv[1]);
  if (NULL == dir) {
    fprintf(stderr, "Failed to open jobs directory\n");
    return 1;
  }

  char job_dir[256];
  strcpy(job_dir, argv[1]);
  strcat(job_dir, "/");

  while ((dir_entry = readdir(dir)) != NULL) {
    if (!strcmp(".", dir_entry->d_name) || !strcmp("..", dir_entry->d_name))
      continue;
      
    char *ext = strchr(dir_entry->d_name, '.');
    if (NULL == ext || strcmp(JOBS_FILE_EXTENSION, strchr(dir_entry->d_name, '.')))
      continue;

    char job_filepath[256];
    char out_filepath[256];
    get_job_paths(job_filepath, out_filepath, job_dir, dir_entry->d_name);

    unsigned long int current_processes = 0;
    while (current_processes >= max_proc) {
      int status;
      pid_t pid_child = wait(&status);
      
      if (pid_child > 0) {
        printf("Job %s with PID=%d terminated with exit status %d\n", job_filepath, pid_child, WEXITSTATUS(status));
        current_processes--;
      }
    }

    pid_t pid = fork();
    if (pid < 0) {
      fprintf(stderr, "Failed to create process\n");
      continue;
    }
    // Child process
    if (pid == 0) {
      closedir(dir);
      int exit_status = process_job(job_filepath, out_filepath, state_access_delay_ms, max_threads);
      exit(exit_status);
    }
    // Parent process: keeping creating new processes
    current_processes++;
  }
  closedir(dir);

  int status;
  pid_t pid_child;
  while ((pid_child = wait(&status)) > 0) {
    printf("Job with PID=%d terminated with exit status %d\n", pid_child, WEXITSTATUS(status));
  }

  return 0;
}
