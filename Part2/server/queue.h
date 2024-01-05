#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

#include "common/constants.h"

typedef struct Connection {
  char req_pipe_path[MAX_PIPE_NAME_SIZE];
  char resp_pipe_path[MAX_PIPE_NAME_SIZE];
  struct Connection *next;
} Connection_t;

typedef struct ConnectionQueue {
  Connection_t *front, *rear;
  char terminate;
  pthread_rwlock_t termination_lock;
  pthread_mutex_t queue_lock;
  pthread_cond_t available_connection;
} ConnectionQueue_t;

/// Initializes the connection queue.
/// @param queue Pointer to the connection queue.
/// @return 0 if successfull, 1 otherwise.
int init_queue(ConnectionQueue_t *queue);

/// Checks if the connection queue is empty.
/// @param queue Pointer to the connection queue.
/// @return 0 if not empty, 1 otherwise.
int isEmpty(ConnectionQueue_t *queue);

/// Frees all allocated memory by the queue.
/// @param queue Pointer to the connection queue.
void free_queue(ConnectionQueue_t *queue);

/// Enqueues a connection request.
/// @param queue Pointer to the connection queue.
/// @param setup_buffer Buffer that contains the paths of the client's named pipes
/// @return 0 if successfull, 1 otherwise.
int enqueue_connection(ConnectionQueue_t *queue, const char *setup_buffer);

/// Dequeues a connection request.
/// @param queue Pointer to the connection queue.
/// @return `Connection_t` struct that contains the paths of the client's named pipes
/// @return `NULL` if the queue is empty
Connection_t *dequeue_connection(ConnectionQueue_t *queue);

#endif
