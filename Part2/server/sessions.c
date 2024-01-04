#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

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
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGPIPE);
  sigaddset(&mask, SIGUSR1);
  
  if (pthread_sigmask(SIG_BLOCK, &mask, NULL)) {
    fprintf(stderr, "Failed creating signal mask\n");
    return JOB_FAILED;
  }

  char op = OP_NONE;
  int response_status = 0;
  ssize_t io_status;

  while (op != OP_QUIT) {
    if ((io_status = safe_read(req_fd, &op, sizeof(char))) <= 0) {
      if (!io_status)
        return UNRESPONSIVE_CLIENT;
      return JOB_FAILED;
    }

    switch (op) {
    case OP_QUIT:
      break;

    case OP_CREATE:
      char c_buf[CREATE_BUFSIZE];
      if ((io_status = safe_read(req_fd, c_buf + sizeof(char), CREATE_BUFSIZE - 1)) <= 0) {
        if (!io_status)
          return UNRESPONSIVE_CLIENT;
        return JOB_FAILED;
      }

      response_status = ems_create(*(unsigned int*)(c_buf + sizeof(int)),
                                *(size_t*)(c_buf + sizeof(size_t)), 
                                *(size_t*)(c_buf + 2 * sizeof(size_t)));
      
      if (safe_write(resp_fd, &response_status, sizeof(int)) < 0) {
        if (errno == EPIPE)
          return UNRESPONSIVE_CLIENT;
        return JOB_FAILED;
      }
      
      break;

    case OP_RESERVE:
      char r_buf[RESERVE_BUFSIZE];
      if ((io_status = safe_read(req_fd, r_buf + sizeof(char), 2 * sizeof(int) + sizeof(size_t) - 1)) <= 0) {
        if (!io_status)
          return UNRESPONSIVE_CLIENT;
        return JOB_FAILED;
      }

      size_t num_seats = *(size_t*)(r_buf + sizeof(size_t));

      if ((io_status = safe_read(req_fd, r_buf + 2 * sizeof(size_t), 2 * num_seats * sizeof(size_t))) <= 0) {
        if (!io_status)
          return UNRESPONSIVE_CLIENT;
        return JOB_FAILED;
      }

      response_status = ems_reserve(*(unsigned int*)(r_buf + sizeof(int)),
                                num_seats,
                                (size_t*)(r_buf + 2 * sizeof(size_t)),
                                (size_t*)(r_buf + sizeof(size_t) * (num_seats + 2)));

      if (safe_write(resp_fd, &response_status, sizeof(int)) < 0) {
        if (errno == EPIPE)
          return UNRESPONSIVE_CLIENT;
        return JOB_FAILED;
      }

      break;

    case OP_SHOW:
      char s_buf[SHOW_BUFSIZE];
      if ((io_status = safe_read(req_fd, s_buf + sizeof(char), SHOW_BUFSIZE - 1)) < 0) {
        if (!io_status)
          return UNRESPONSIVE_CLIENT;
        return JOB_FAILED;
      }

      response_status = ems_show(resp_fd, *(unsigned int*)(s_buf + sizeof(int)));

      if (safe_write(resp_fd, &response_status, sizeof(int)) < 0) {
        if (errno == EPIPE)
          return UNRESPONSIVE_CLIENT;
        return JOB_FAILED;
      }

      break;

    case OP_LIST:
      response_status = ems_list_events(resp_fd);

      if (safe_write(resp_fd, &response_status, sizeof(int)) < 0) {
        if (errno == EPIPE)
          return UNRESPONSIVE_CLIENT;
        return JOB_FAILED;
      }

      break;
    
    default:
      break;
    }
  }
  
  return JOB_SUCCESS;
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

    fprintf(stdout, "\x1b[1;94m[WORKER %.2u] Connected to Client!\x1b[0m\n", session_id);

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

    int job_status = read_requests(req_pipe, resp_pipe);
    if (job_status) {
      if (job_status == JOB_FAILED)
        fprintf(stderr, "Failed reading client requests\n");
      else if (job_status == UNRESPONSIVE_CLIENT)
        fprintf(stderr, "\x1b[1;91m[WORKER %.2u] Client is unresponsive. Terminating Session...\x1b[0m\n", session_id);

      close(resp_pipe);
      close(req_pipe);
      continue;
    }

    close(req_pipe);
    close(resp_pipe);
    fprintf(stdout, "\x1b[1;94m[WORKER %.2u] Job successfully completed. Terminating Session...\x1b[0m\n", session_id);
  }

  return NULL;
}