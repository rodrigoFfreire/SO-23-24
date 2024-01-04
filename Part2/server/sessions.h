#ifndef CONNECTIONS_H
#define CONNECTIONS_H

#include "queue.h"
#include "common/constants.h"

#define CREATE_BUFSIZE ((size_t) 3 * sizeof(size_t))
#define RESERVE_BUFSIZE ((size_t) sizeof(size_t) * (2 * MAX_RESERVATION_SIZE + 2))
#define SHOW_BUFSIZE ((size_t) 2 * sizeof(int))

#define JOB_SUCCESS 0
#define JOB_FAILED 1 
#define UNRESPONSIVE_CLIENT 2

typedef struct Session {
    ConnectionQueue_t *queue;
    unsigned int session_id;
} Session_t;


void *connect_clients(void *args);


#endif