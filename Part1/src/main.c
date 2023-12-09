#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "constants.h"
#include "operations.h"
#include "eventlist.h"
#include "parser.h"
#include "utils.h"


int process_job(char *job_filepath, char *out_filepath, unsigned int access_delay) {
  struct Ems ems = {NULL, 0};
  if (ems_init(&ems, access_delay)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  int job_fd = open(job_filepath, O_RDONLY);
  if (job_fd < 0) {
    fprintf(stderr, "Failed to open %s file\n", job_filepath);
    ems_terminate(&ems);
    close(job_fd);
    return 1;
  }

  int out_fd = open(out_filepath, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (out_fd < 0) {
    fprintf(stderr, "Failed to open %s file\n", out_filepath);
    ems_terminate(&ems);
    close(job_fd);
    close(out_fd);
    return 1;
  }

  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  unsigned int event_id, delay;

  char eof = 0;
  while (!eof) {
    switch (get_next(job_fd)) {
    case CMD_CREATE:
      if (parse_create(job_fd, &event_id, &num_rows, &num_columns) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (ems_create(&ems, event_id, num_rows, num_columns)) {
        fprintf(stderr, "Failed to create event\n");
      }

      break;

    case CMD_RESERVE:
      num_coords =
          parse_reserve(job_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

      if (num_coords == 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (ems_reserve(&ems, event_id, num_coords, xs, ys)) {
        fprintf(stderr, "Failed to reserve seats\n");
      }

      break;

    case CMD_SHOW:
      if (parse_show(job_fd, &event_id) != 0) {
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (ems_show(&ems, event_id, out_fd)) {
        fprintf(stderr, "Failed to show event\n");
      }

      break;

    case CMD_LIST_EVENTS:
      if (ems_list_events(&ems, out_fd)) {
        fprintf(stderr, "Failed to list events\n");
      }

      break;

    case CMD_WAIT:
      if (parse_wait(job_fd, &delay, NULL) == -1) { // thread_id is not implemented
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        continue;
      }

      if (delay > 0) {
        printf("Waiting...\n");
        ems_wait(delay);
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
          "  BARRIER\n"                     // Not implemented
          "  HELP\n");

      break;

    case CMD_BARRIER: // Not implemented
    case CMD_EMPTY:
      break;

    case EOC:
      eof = 1;
      break;
    }
  }

  close(job_fd);
  close(out_fd);
  ems_terminate(&ems);

  return 0;
}


int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  struct dirent *dir_entry;

  if (argc < 3) {
    fprintf(stderr, "Not enough arguments were provided\n");
    return 1;
  }

  char *endptr;
  long int max_proc = strtol(argv[2], &endptr, 10);
  long int current_processes = 0;

  if (*endptr != '\0' || max_proc > sysconf(_SC_CHILD_MAX)) {
    fprintf(stderr, "Invalid max processes or value too large\n");
    return 1;
  }

  if (argc > 3) {
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

    while (current_processes >= max_proc) {
      int status;
      pid_t pid_child = wait(&status);
      
      if (pid_child > 0) {
        printf("Job with PID=%d terminated with exit status %d\n", pid_child, WEXITSTATUS(status));
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
      int exit_status = process_job(job_filepath, out_filepath, state_access_delay_ms);
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
