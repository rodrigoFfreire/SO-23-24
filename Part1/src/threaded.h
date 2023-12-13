#ifndef EMS_THREADED_H
#define EMS_THREADED_H

#include <stddef.h>
#include <pthread.h>

#include "operations.h"
#include "constants.h"


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
    // Add operationMutex;

} ThreadManager_t;


/// Processes the job file with `max_threads` threads
/// @param threads The thread array
/// @param ems Event Management System data structure
/// @param job_fd .job file descriptor
/// @param out_fd .out file descriptor
/// @param max_threads The maximum amount of threads allowed to be ran concurrently
/// @param thread_delays Array of delays for each thread when WAITS are injected
/// @param thread_waits Array that contains the WAIT flag for each thread
/// @param parseMutex Parsing Lock
/// @return `0` if ran successfully, `1` if found a BARRIER, `-1` on error
int dispatch_threads(pthread_t *threads, Ems_t *ems, int job_fd, int out_fd, 
    unsigned long max_threads, unsigned int *thread_delays, char *thread_waits, 
    pthread_mutex_t *parseMutex);

void clean_threads(pthread_t *threads, unsigned int *thread_delays, char *thread_waits,
  pthread_mutex_t *parseMutex);

void *process_commands(void *args);


#endif // EMS_THREADED_H