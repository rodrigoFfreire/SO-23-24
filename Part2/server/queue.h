#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "common/constants.h"


typedef struct Connection {
    char req_pipe_path[MAX_PIPE_NAME_SIZE];
    char resp_pipe_path[MAX_PIPE_NAME_SIZE];
    struct Connection* next; 
} Connection_t;

typedef struct ConnectionQueue {
    Connection_t *front, *rear;
    pthread_mutex_t lock;
    pthread_cond_t available_connection;
} ConnectionQueue_t;

typedef struct ThreadInfo {
    ConnectionQueue_t *queue;
    unsigned int session_id;
} ThreadInfo_t;


int init_queue(ConnectionQueue_t *queue);

int isEmpty(ConnectionQueue_t *queue);

void free_queue(ConnectionQueue_t *queue);

int enqueue_connection(ConnectionQueue_t *queue, const char* setup_buffer);

Connection_t *dequeue_connection(ConnectionQueue_t *queue);

void *connect_clients(void *args);

#endif