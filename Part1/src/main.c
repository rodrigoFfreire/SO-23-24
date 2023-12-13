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

int process_job(char *job_filepath, char *out_filepath, unsigned int access_delay, unsigned long max_threads) {
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
  
  Ems_t ems = {NULL, 0};
  if (ems_init(&ems, access_delay)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    close(job_fd);
    close(out_fd);
    return 1;
  }

  pthread_t *threads = (pthread_t*) malloc(sizeof(pthread_t) * max_threads);
  if (NULL == threads) {
    fprintf(stderr, "Failed to allocate memory\n");
    ems_terminate(&ems);
    close(job_fd);
    close(out_fd);
    return 1;
  }

  pthread_mutex_t parseMutex = PTHREAD_MUTEX_INITIALIZER;
  char *thread_waits = (char*) malloc(sizeof(char) * max_threads);
  unsigned int *thread_delays = (unsigned int*) malloc(sizeof(char) * max_threads);

  int job_status = 1;
  while (job_status == THREAD_FOUND_BARRIER) {
    memset(thread_waits, 0, max_threads);
    memset(thread_delays, 0, max_threads);

    job_status = dispatch_threads(threads, &ems, job_fd, out_fd, max_threads, thread_delays, thread_waits, &parseMutex);
    if (job_status < 0) {
      fprintf(stderr, "An error has occured processing the job file\n");
      clean_threads(threads, thread_delays, thread_waits, &parseMutex);
      ems_terminate(&ems);
      close(job_fd);
      close(out_fd);
      return 1;
    }
  }

  clean_threads(threads, thread_delays, thread_waits, &parseMutex);

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

  unsigned long max_proc = strtoul(argv[2], &endptr, 10);
  if (*endptr != '\0' || max_proc > (unsigned long )sysconf(_SC_CHILD_MAX)
      || max_proc <= 0) {
    fprintf(stderr, "Invalid max processes or value too large\n");
    return 1;
  }

  unsigned long max_threads = strtoul(argv[3], &endptr, 10);
  if (*endptr != '\0' || max_threads <= 0 || max_threads > UINT_MAX) {
    fprintf(stderr, "Invalid max threads or value too large\n");
    return 1;
  }

  if (argc > 4) {
    unsigned long delay = strtoul(argv[3], &endptr, 10);

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
