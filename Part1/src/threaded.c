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
      thread_delays[i] += delay;
    }
  }
  return 0;
}


int dispatch_threads(pthread_t *threads, Ems_t *ems, int job_fd, int out_fd, 
  unsigned long max_threads, unsigned int *thread_delays, char *thread_waits,
  pthread_mutex_t *parseMutex)
{
  char barrier = 0;
  ThreadManager_t *th_mgr = (ThreadManager_t*) malloc(sizeof(ThreadManager_t) * max_threads);
  if (th_mgr == NULL) {
    fprintf(stderr, "Could not allocate memory\n");
    return THREAD_ERROR;
  }

  for (unsigned long i = 0; i < max_threads; i++) {
    th_mgr[i].ems = ems;
    th_mgr[i].job_fd = job_fd;
    th_mgr[i].out_fd = out_fd;
    th_mgr[i].max_threads = max_threads;
    th_mgr[i].tid = i;
    th_mgr[i].thread_waits = thread_waits;
    th_mgr[i].thread_delays = thread_delays;
    th_mgr[i].barrier = &barrier;
    th_mgr[i].parseMutex = parseMutex;

    if (pthread_create(&threads[i], NULL, process_commands, (void*) &th_mgr[i]) != 0) {
      fprintf(stderr, "Could not create thread\n");
      free(th_mgr);
      return THREAD_ERROR;
    }
  }

  for (unsigned long i = 0; i < max_threads; i++) {
    void *return_value = NULL;
    if (pthread_join(threads[i], &return_value) != 0) {
      fprintf(stderr, "Could not join thread\n");
      free(th_mgr);
      free(return_value);
      return THREAD_ERROR;
    }

    if (return_value == NULL) {
      fprintf(stderr, "Thread could not process file\n");
      free(th_mgr);
      free(return_value);
      return THREAD_ERROR;
    }

    free(return_value);
  }
  free(th_mgr);

  if (barrier) {
    fprintf(stderr, "BARRIER Found. Restarting...\n");
    return THREAD_FOUND_BARRIER;
  }
  return THREAD_SUCCESS;
}

void clean_threads(pthread_t *threads, unsigned int *thread_delays, char *thread_waits,
  pthread_mutex_t *parseMutex) 
{
  free(threads);
  free(thread_delays);
  free(thread_waits);
  pthread_mutex_destroy(parseMutex);
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

  int *return_value = (int*) malloc(sizeof(int));
  if (return_value == NULL) {
    fprintf(stderr, "Could not allocate memory\n");
    pthread_exit(NULL);
  }

  char eof = 0;
  while (!eof) {
    pthread_mutex_lock(th_mgr->parseMutex);

    if (*(th_mgr->barrier)) {   // check if barrrier flag is true
      pthread_mutex_unlock(th_mgr->parseMutex);
      *return_value = THREAD_FOUND_BARRIER;

      pthread_exit((void*) return_value);
    }

    if (th_mgr->thread_waits[tid]) {
      printf("Waiting...\n"); // Remove this later
      th_mgr->thread_waits[tid] = 0;
      delay = th_mgr->thread_delays[tid];
      th_mgr->thread_delays[tid] = 0;  // Consume delay

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

      case CMD_WAIT: {
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
      }
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
        *(th_mgr->barrier) = 1;
        pthread_mutex_unlock(th_mgr->parseMutex);

        *return_value = THREAD_FOUND_BARRIER;
        pthread_exit((void*) return_value);

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

  *return_value = THREAD_SUCCESS;
  pthread_exit((void*) return_value);
}
