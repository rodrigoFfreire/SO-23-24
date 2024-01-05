#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "queue.h"
#include "common/constants.h"

int init_queue(ConnectionQueue_t *queue) {
  if (pthread_mutex_init(&queue->queue_lock, NULL) != 0) {
    fprintf(stderr, "Failed to initialize queue lock");
    return 1;
  }
  
  if (pthread_rwlock_init(&queue->termination_lock, NULL) != 0) {
    fprintf(stderr, "Failed to initialize termination lock");
    pthread_mutex_destroy(&queue->queue_lock);
    return 1;
  }

  if (pthread_cond_init(&queue->available_connection, NULL) != 0) {
    fprintf(stderr, "Failed to initialize condition variable");
    pthread_mutex_destroy(&queue->queue_lock);
    pthread_rwlock_destroy(&queue->termination_lock);
    return 1;
  }

  queue->front = queue->rear = NULL;
  queue->terminate = 0;

  return 0;
}

int isEmpty(ConnectionQueue_t *queue) {
  return (queue->front == NULL);
}

void free_queue(ConnectionQueue_t *queue) {
  Connection_t *curr_node = queue->front;
  Connection_t *next_node;

  while (curr_node != NULL) {
    next_node = curr_node->next;
    free(curr_node);
    fprintf(stderr, "\x1b[1;91m[SERVER]: Rejected Connection [Closing Server]\n");
    curr_node = next_node;
  }
  queue->front = queue->rear = NULL;

  pthread_mutex_destroy(&queue->queue_lock);
  pthread_rwlock_destroy(&queue->termination_lock);
  pthread_cond_destroy(&queue->available_connection);
}

int enqueue_connection(ConnectionQueue_t *queue, const char* setup_buffer) {
  Connection_t *new_connection = (Connection_t*) malloc(sizeof(Connection_t));
  if (new_connection == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    return 1;
  }

  new_connection->next = NULL;
  memcpy(new_connection->req_pipe_path, setup_buffer + 1, MAX_PIPE_NAME_SIZE);
  memcpy(new_connection->resp_pipe_path, setup_buffer + 1 + MAX_PIPE_NAME_SIZE, MAX_PIPE_NAME_SIZE);

  if (pthread_mutex_lock(&queue->queue_lock) != 0) {
    fprintf(stderr, "Failed locking queue");
    free(new_connection);
    return 1;
  }

  if (isEmpty(queue))
    queue->rear = queue->front = new_connection;
  else {
    queue->rear->next = new_connection;
    queue->rear = new_connection;
  }

  pthread_cond_broadcast(&queue->available_connection);
  pthread_mutex_unlock(&queue->queue_lock);

  return 0;
}

Connection_t *dequeue_connection(ConnectionQueue_t *queue) {
  Connection_t *connection = queue->front;

  if (queue->front == queue->rear)
    queue->front = queue->rear = NULL;
  else
    queue->front = queue->front->next;

  return connection;
}
