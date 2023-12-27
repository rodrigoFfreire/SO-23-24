#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>


typedef struct Connection {
    const char* req_pipe_path;
    const char* resp_pipe_path;
    struct Connection* next; 
} Connection_t;

typedef struct ConnectionQueue {
    Connection_t *front, *rear;
    pthread_mutex_t lock;
} ConnectionQueue_t;


int init_queue(ConnectionQueue_t *queue);

int isEmpty(ConnectionQueue_t *queue);

int enqueue_connection(ConnectionQueue_t *queue, const char* req_pipe_path, const char* resp_pipe_path);

#endif