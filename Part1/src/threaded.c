#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "operations.h"
#include "threaded.h"
#include "parser.h"
#include "utils.h"


void dispatch_threads(pthread_t *threads, Ems_t *ems, int job_fd, int out_fd, 
  unsigned long max_threads, unsigned int *thread_delays, char *thread_waits, 
  pthread_mutex_t *parseMutex)
{
  for (unsigned long i = 0; i < max_threads; i++) {
    ThreadManager_t *th_mgr = (ThreadManager_t*) malloc(sizeof(ThreadManager_t));
    th_mgr->ems = ems;
    th_mgr->job_fd = job_fd;
    th_mgr->out_fd = out_fd;
    th_mgr->max_threads = max_threads;
    th_mgr->thread_id = i;
    th_mgr->thread_waits = thread_waits;
    th_mgr->thread_delays = thread_delays;
    th_mgr->parseMutex = parseMutex;

    pthread_create(&threads[i], NULL, process_commands, (void*) th_mgr);
  }

  for (unsigned long i = 0; i < max_threads; i++) {
    pthread_join(threads[i], NULL); // Change this later for barrier
  }
}

void clean_threads(pthread_t *threads, unsigned int *thread_delays, char *thread_waits,
  pthread_mutex_t *parseMutex)
{
  pthread_mutex_destroy(parseMutex);
  free(thread_delays);
  free(thread_waits);
  free(threads);
}


void *process_commands(void *args) {
  ThreadManager_t *th_mgr = (ThreadManager_t*) args;

  int job_fd = th_mgr->job_fd;
  int out_fd = th_mgr->out_fd;
  Ems_t *ems = th_mgr->ems;

  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  unsigned int event_id, delay;

  char eof = 0;
  while (!eof) {
    pthread_mutex_lock(th_mgr->parseMutex);
    switch (get_next(job_fd)) {
      case CMD_CREATE:
        if (parse_create(job_fd, &event_id, &num_rows, &num_columns) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        pthread_mutex_unlock(th_mgr->parseMutex);

        if (ems_create(ems, event_id, num_rows, num_columns))
          fprintf(stderr, "Failed to create event\n");

        break;

      case CMD_RESERVE:
        if ((num_coords = parse_reserve(job_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys)) == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        pthread_mutex_unlock(th_mgr->parseMutex);

        if (ems_reserve(ems, event_id, num_coords, xs, ys))
          fprintf(stderr, "Failed to reserve seats\n");

        break;

      case CMD_SHOW:
        if (parse_show(job_fd, &event_id) != 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        pthread_mutex_unlock(th_mgr->parseMutex);

        if (ems_show(ems, event_id, out_fd))
          fprintf(stderr, "Failed to show event\n");

        break;

      case CMD_LIST_EVENTS:
        pthread_mutex_unlock(th_mgr->parseMutex);

        if (ems_list_events(ems, out_fd)) {
          fprintf(stderr, "Failed to list events\n");
        }
        
        break;

      case CMD_WAIT:
        if (parse_wait(job_fd, &delay, NULL) == -1) { // thread_id is not implemented
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        pthread_mutex_unlock(th_mgr->parseMutex);

        if (delay > 0) {
          printf("Waiting...\n");
          //wait;
        }

        break;

      case CMD_INVALID:
        pthread_mutex_unlock(th_mgr->parseMutex);
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP:
        pthread_mutex_unlock(th_mgr->parseMutex);
        printf(
            "Available commands:\n"
            "  CREATE <event_id> <num_rows> <num_columns>\n"
            "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
            "  SHOW <event_id>\n"
            "  LIST\n"
            "  WAIT <delay_ms> [thread_id]\n"
            "  BARRIER\n"
            "  HELP\n");

        break;

      case CMD_BARRIER:
        pthread_mutex_unlock(th_mgr->parseMutex);
        break;
      case CMD_EMPTY:
        pthread_mutex_unlock(th_mgr->parseMutex);
        break;
      case EOC:
        pthread_mutex_unlock(th_mgr->parseMutex);
        eof = 1;
        break;
    }
  }
  free(args);
  return NULL;
}