#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "operations.h"
#include "threaded.h"
#include "parser.h"
#include "utils.h"
#include "time.h"

/// The current thread executing this will wait for `delay_ms`
/// @param delay_ms The amount to wait in milliseconds
void thread_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

/// Injects a wait in a specific thread (if specified) or all threads
/// @param wait_option Option flag: (0 | 1) decides if wait is global (0) or thread specific (1)
/// @param target_tid For non global waits this is the target thread for wait injection
/// @param max_threads Maximum amount of concurrent threads
/// @param thread_waits Delay flag: (0 | 1) decides which threads wait
/// @param thread_delays Delay value to wait for each thread
/// @param delay Amount to wait in milliseconds
/// @return 0 if waits were successfully injected in thread(s)
int inject_wait(int wait_option, unsigned int target_tid, unsigned long max_threads, 
  char *thread_waits, unsigned int *thread_delays, unsigned int delay)
{
  // Mutex is assumed to be locked here
  if (wait_option) {
    if (target_tid >= max_threads)
      return 1;

    thread_waits[target_tid] = 1;
    thread_delays[target_tid] = delay;
  } 
  else {
    for (unsigned long i = 0; i < max_threads; i++) {
      thread_waits[i] = 1;
      thread_delays[i] = delay;
    }
  }
  return 0;
}


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
    th_mgr->tid = i;
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

  unsigned long tid = th_mgr->tid;
  int job_fd = th_mgr->job_fd;
  int out_fd = th_mgr->out_fd;
  Ems_t *ems = th_mgr->ems;

  size_t num_rows, num_columns, num_coords;
  size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
  unsigned int event_id, delay, target_tid;

  char eof = 0;
  while (!eof) {
    pthread_mutex_lock(th_mgr->parseMutex);

    if (th_mgr->thread_waits[tid]) {
      printf("Waiting...\n"); // Remove this later
      th_mgr->thread_waits[tid] = 0;
      delay = th_mgr->thread_delays[tid];

      pthread_mutex_unlock(th_mgr->parseMutex);
      thread_wait(delay);
    } else {
      pthread_mutex_unlock(th_mgr->parseMutex);
    }

    pthread_mutex_lock(th_mgr->parseMutex);
    switch (get_next(job_fd)) {
      case CMD_CREATE:
        if (parse_create(job_fd, &event_id, &num_rows, &num_columns) != 0) {
          pthread_mutex_unlock(th_mgr->parseMutex);
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        pthread_mutex_unlock(th_mgr->parseMutex);

        if (ems_create(ems, event_id, num_rows, num_columns))
          fprintf(stderr, "Failed to create event\n");

        break;

      case CMD_RESERVE:
        if ((num_coords = parse_reserve(job_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys)) == 0) {
          pthread_mutex_unlock(th_mgr->parseMutex);
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        pthread_mutex_unlock(th_mgr->parseMutex);

        if (ems_reserve(ems, event_id, num_coords, xs, ys))
          fprintf(stderr, "Failed to reserve seats\n");

        break;

      case CMD_SHOW:
        if (parse_show(job_fd, &event_id) != 0) {
          pthread_mutex_unlock(th_mgr->parseMutex);
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
        int wait_option = parse_wait(job_fd, &delay, &target_tid);
        if (wait_option == -1 || delay == 0) {
          pthread_mutex_unlock(th_mgr->parseMutex);
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (inject_wait(wait_option, target_tid, th_mgr->max_threads, 
                      th_mgr->thread_waits, th_mgr->thread_delays, delay)) 
        {
          fprintf(stderr, "Invalid thread id\n");
        }
        pthread_mutex_unlock(th_mgr->parseMutex);

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
            "  WAIT <delay_ms> [target_tid]\n"
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
