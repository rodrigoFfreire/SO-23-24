#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "queue.h"
#include "common/constants.h"

int init_queue(ConnectionQueue_t *queue) {
  if (queue == NULL || pthread_mutex_init(&queue->lock, NULL) != 0 || 
      pthread_cond_init(&queue->available_connection, NULL) != 0)
    return 1;

  queue->front = queue->rear = NULL;

  return 0;
}

int isEmpty(ConnectionQueue_t *queue) {
  return (queue->front == NULL);
}

void free_queue(ConnectionQueue_t *queue) {
  Connection_t *curr_node = queue->front;
  while (curr_node != NULL && curr_node->next != NULL) {
    free(curr_node->next);
    curr_node = curr_node->next;
  }
  pthread_mutex_destroy(&queue->lock);
}

int enqueue_connection(ConnectionQueue_t *queue, const char* setup_buffer) {
  Connection_t *new_connection = (Connection_t*) malloc(sizeof(Connection_t));
  if (queue == NULL || new_connection == NULL)
    return 1;

  new_connection->next = NULL;
  memcpy(new_connection->req_pipe_path, setup_buffer + 1, MAX_PIPE_NAME_SIZE);
  memcpy(new_connection->resp_pipe_path, setup_buffer + 1 + MAX_PIPE_NAME_SIZE, MAX_PIPE_NAME_SIZE);

  if (pthread_mutex_lock(&queue->lock) != 0) {
    fprintf(stderr, "Failed locking queue\n");
    free(new_connection);
    return 1;
  }

  if (isEmpty(queue))
    queue->rear = queue->front = new_connection;
  else {
    queue->rear->next = new_connection;
    queue->rear = new_connection;
  }
  pthread_cond_signal(&queue->available_connection);
  pthread_mutex_unlock(&queue->lock);

  return 0;
}

Connection_t *dequeue_connection(ConnectionQueue_t *queue) {
  if (queue == NULL)
    return NULL;

  if (pthread_mutex_lock(&queue->lock) != 0) {
    fprintf(stderr, "Failed locking queue\n");
    return NULL;
  }

  while (isEmpty(queue)) {
    pthread_cond_wait(&queue->available_connection, &queue->lock);
  }

  Connection_t *connection = queue->front;

  if (queue->front == queue->rear)
    queue->front = queue->rear = NULL;
  else
    queue->front = queue->front->next;

  pthread_mutex_unlock(&queue->lock);

  return connection;
}

void *connect_clients(void *args) {
  ThreadInfo_t info = *(ThreadInfo_t*)args;
  unsigned int session_id = info.session_id;
  ConnectionQueue_t *queue = info.queue;

  char req_pipe_path[MAX_PIPE_NAME_SIZE];
  char resp_pipe_path[MAX_PIPE_NAME_SIZE];

  while (1) { // Replace with a termination condition later
    Connection_t *connection = dequeue_connection(queue);
    if (connection == NULL) {
      fprintf(stderr, "Failed to parse connection request\n");
      free(connection);
      break;
    }

    memcpy(req_pipe_path, connection->req_pipe_path, MAX_PIPE_NAME_SIZE);
    memcpy(resp_pipe_path, connection->resp_pipe_path, MAX_PIPE_NAME_SIZE);
    free(connection);

    // TODO: Interact with client 
  }
  return NULL;
}