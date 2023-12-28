#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "common/constants.h"
#include "connections.h"


typedef struct ConnectionQueue {
    Connection_t *front, *rear;
    char terminate;
    pthread_rwlock_t termination_lock;
    pthread_mutex_t queue_lock;
    pthread_cond_t available_connection;
} ConnectionQueue_t;


int init_queue(ConnectionQueue_t *queue);

int isEmpty(ConnectionQueue_t *queue);

void free_queue(ConnectionQueue_t *queue);

int enqueue_connection(ConnectionQueue_t *queue, const char* setup_buffer);

Connection_t *dequeue_connection(ConnectionQueue_t *queue);

void *connect_clients(void *args);

int check_termination(ConnectionQueue_t *queue);

#endif