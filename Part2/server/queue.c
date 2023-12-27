#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "queue.h"

int init_queue(ConnectionQueue_t *queue) {
    if (pthread_mutex_init(&queue->lock, NULL) != 0)
        return 1;

    queue->front = queue->rear = NULL;
    return 0;
}

int isEmpty(ConnectionQueue_t *queue) {
    return (queue->front == NULL);
}

int enqueue_connection(ConnectionQueue_t *queue, const char* req_pipe_path, const char* resp_pipe_path) {
    if (queue == NULL)
        return 1;

    Connection_t *new_con = (Connection_t*) malloc(sizeof(Connection_t));
    if (new_con == NULL)
        return 1;

    new_con->req_pipe_path = req_pipe_path;
    new_con->resp_pipe_path = resp_pipe_path;
    new_con->next = NULL;

    if (isEmpty(queue))
        queue->rear = queue->front = new_con;
    else {
        queue->rear->next = new_con;
        queue->rear = new_con;
    }
    return 0;
}
