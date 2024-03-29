#ifndef EMS_THREADED_H
#define EMS_THREADED_H

#include <pthread.h>
#include <stddef.h>

#include "constants.h"
#include "operations.h"

#define THREAD_ERROR -1
#define THREAD_SUCCESS 0
#define THREAD_FOUND_BARRIER 1

typedef struct ThreadManager {
  Ems_t *ems;
  int job_fd;
  int out_fd;
  unsigned long max_threads;
  unsigned long tid;
  unsigned int *thread_delays;
  char *thread_waits;
  char *barrier;
  pthread_mutex_t *parseMutex;
} ThreadManager_t;

/// Creates threads to process a job file running commands in parallel
/// @param threads The thread array
/// @param ems Event Management System data structure
/// @param job_fd .job file descriptor
/// @param out_fd .out file descriptor
/// @param max_threads The maximum amount of threads allowed to be ran
/// concurrently
/// @param thread_delays Array of delays for each thread when WAITS are injected
/// @param thread_waits Array that contains the WAIT flag for each thread
/// @param parseMutex Mutex for locking the parsing operation
/// @return `0` if ran successfully, `1` if found a BARRIER, `-1` on error
int dispatch_threads(pthread_t *threads, Ems_t *ems, int job_fd, int out_fd,
                     unsigned long max_threads, unsigned int *thread_delays,
                     char *thread_waits, pthread_mutex_t *parseMutex);

/// Processes a job file. Reads commands and executes them
/// @param args A ThreadManager struct which contains information for managing
/// threads
/// @return `NULL` if operation failed, `0` if was operation was successful or
/// `1` if a barrier was found
void *process_commands(void *args);

/// Cleans thread related allocated memory
/// @param threads Thread array
/// @param thread_delays Array of delays for each thread when WAITS are injected
/// @param thread_waits Array that contains the WAIT flag for each thread
/// @param parseMutex Mutex for locking the parsing operation
void clean_threads(pthread_t *threads, unsigned int *thread_delays,
                   char *thread_waits, pthread_mutex_t *parseMutex);

#endif // EMS_THREADED_H
