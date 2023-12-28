#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "queue.h"
#include "sessions.h"


int check_termination(ConnectionQueue_t *queue) {
  pthread_rwlock_rdlock(&queue->termination_lock);
    if (queue->terminate) {
      pthread_rwlock_unlock(&queue->termination_lock);
      return 1;
    }
  pthread_rwlock_unlock(&queue->termination_lock);
  return 0;
}


void *connect_clients(void *args) {
  Session_t session_info = *(Session_t*)args;
  // unsigned int session_id = session_info.session_id; shutup compiler unused var
  ConnectionQueue_t *queue = session_info.queue;

  char req_pipe_path[MAX_PIPE_NAME_SIZE];
  char resp_pipe_path[MAX_PIPE_NAME_SIZE];

  while (1) {
    if (check_termination(queue))
      break;

    pthread_mutex_lock(&queue->queue_lock);
    while (isEmpty(queue)) {
      pthread_cond_wait(&queue->available_connection, &queue->queue_lock);
      
      if (check_termination(queue)) {
        pthread_mutex_unlock(&queue->queue_lock);
        return NULL;
      }
    }

    Connection_t *connection = dequeue_connection(queue);
    pthread_mutex_unlock(&queue->queue_lock);

    if (connection == NULL) {
      fprintf(stderr, "Failed to accept client connection\n");
      continue;
    }

    memcpy(req_pipe_path, connection->req_pipe_path, MAX_PIPE_NAME_SIZE);
    memcpy(resp_pipe_path, connection->resp_pipe_path, MAX_PIPE_NAME_SIZE);
    free(connection);

    // TODO: Interact with client
  }

  return NULL;
}