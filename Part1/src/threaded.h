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
    pthread_mutex_t *parseMutex;
    // Add operationMutex;

} ThreadManager_t;

void dispatch_threads(pthread_t *threads, Ems_t *ems, int job_fd, int out_fd, 
    unsigned long max_threads, unsigned int *thread_delays, char *thread_waits, 
    pthread_mutex_t *parseMutex);

void clean_threads(pthread_t *threads, unsigned int *thread_delays, char *thread_waits,
  pthread_mutex_t *parseMutex);

void *process_commands(void *args);


#endif // EMS_THREADED_H