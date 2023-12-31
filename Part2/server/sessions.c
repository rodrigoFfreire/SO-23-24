#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "queue.h"
#include "sessions.h"
#include "common/io.h"
#include "operations.h"


int check_termination(ConnectionQueue_t *queue) {
  pthread_rwlock_rdlock(&queue->termination_lock);
    if (queue->terminate) {
      pthread_rwlock_unlock(&queue->termination_lock);
      return 1;
    }
  pthread_rwlock_unlock(&queue->termination_lock);
  return 0;
}

int read_requests(int req_fd, int resp_fd) {
  char op = OP_NONE;
  int return_value = 0;

  while (op != OP_QUIT) {
    if (safe_read(req_fd, &op, sizeof(char)) < 0) {
      return 1;
    }

    switch (op) {
    case OP_QUIT:
      break;

    case OP_CREATE:
      char c_buf[CREATE_BUFSIZE];
      if (safe_read(req_fd, c_buf + sizeof(char), CREATE_BUFSIZE - 1) < 0)
        return 1;

      return_value = ems_create(*(unsigned int*)(c_buf + sizeof(int)),
                                *(size_t*)(c_buf + sizeof(size_t)), 
                                *(size_t*)(c_buf + 2 * sizeof(size_t)));
      
      if (safe_write(resp_fd, &return_value, sizeof(int)) < 0) {
        return 1;
      }
      
      break;

    case OP_RESERVE:
      char r_buf[RESERVE_BUFSIZE];
      if (safe_read(req_fd, r_buf + sizeof(char), 2 * sizeof(int) + sizeof(size_t) - 1) < 0)
        return 1;

      size_t num_seats = *(size_t*)(r_buf + sizeof(size_t));

      if (safe_read(req_fd, r_buf + 2 * sizeof(size_t), 2 * num_seats * sizeof(size_t)) < 0)
        return 1;

      return_value = ems_reserve(*(unsigned int*)(r_buf + sizeof(int)),
                                num_seats,
                                (size_t*)(r_buf + 2 * sizeof(size_t)),
                                (size_t*)(r_buf + sizeof(size_t) * (num_seats + 2)));

      if (safe_write(resp_fd, &return_value, sizeof(int)) < 0)
        return 1;

      break;

    case OP_SHOW:
      char s_buf[SHOW_BUFSIZE];
      if (safe_read(req_fd, s_buf + sizeof(char), SHOW_BUFSIZE - 1) < 0)
        return 1;

      return_value = ems_show(resp_fd, *(unsigned int*)(s_buf + sizeof(int)));

      if (safe_write(resp_fd, &return_value, sizeof(int)) < 0)
        return 1;

      break;

    case OP_LIST:
      return_value = ems_list_events(resp_fd);

      if (safe_write(resp_fd, &return_value, sizeof(int)) < 0)
        return 1;

      break;
    
    default:
      break;
    }
  }
  
  return 0;
}

void *connect_clients(void *args) {
  Session_t *session_info = (Session_t*)args;
  unsigned int session_id = session_info->session_id;
  ConnectionQueue_t *queue = session_info->queue;

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

    fprintf(stdout, "\x1b[1;94m[WORKER %.2u] Connected to Client!\n", session_id);

    int resp_pipe = open(resp_pipe_path, O_WRONLY);
    if (resp_pipe < 0) {
      fprintf(stderr, "Failed opening response pipe\n");
      continue;
    }

    if (safe_write(resp_pipe, &session_id, sizeof(int)) < 0) {
      fprintf(stderr, "Failed to send response to client\n");
      close(resp_pipe);
      continue;
    }

    int req_pipe = open(req_pipe_path, O_RDONLY);
    if (req_pipe < 0) {
      fprintf(stderr, "Failed opening request pipe\n");
      close(resp_pipe);
      continue;
    }

    if (read_requests(req_pipe, resp_pipe)) {
      fprintf(stderr, "Failed reading client requests\n");
      close(resp_pipe);
      close(req_pipe);
      continue;
    }

    close(req_pipe);
    close(resp_pipe);
    fprintf(stdout, "\x1b[1;94m[WORKER %.2u] Client disconnected [JOB DONE]\n", session_id);
  }

  return NULL;
}