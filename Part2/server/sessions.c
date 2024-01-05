#include "sessions.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/io.h"
#include "operations.h"
#include "queue.h"


/// Listens for the client's requests, executes the appropriate commands and responds back
/// @param req_fd File descriptor of the request pipe
/// @param resp_fd File descriptor of the response pipe
/// @return `CLIENT_JOB_SUCCESSS` if successful, `CLIENT_FAILED` on error,
/// `CLIENT_UNRESPONSIVE` if the client has closed its pipes
int handle_requests(int req_fd, int resp_fd) {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGPIPE);
  sigaddset(&mask, SIGUSR1);

  if (pthread_sigmask(SIG_BLOCK, &mask, NULL)) {
    fprintf(stderr, "Failed creating signal mask\n");
    return CLIENT_FAILED;
  }

  char op = OP_NONE;
  int response_status = 0;
  ssize_t io_status;

  while (op != OP_QUIT) {
    if ((io_status = safe_read(req_fd, &op, sizeof(char))) <= 0) {
      if (!io_status) return CLIENT_UNRESPONSIVE;
      return CLIENT_FAILED;
    }

    unsigned int event_id;
    size_t num_matrix[2], xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
    size_t num_seats;

    switch (op) {
      case OP_QUIT:
        break;

      case OP_CREATE:
        if ((io_status = safe_read(req_fd, &event_id, sizeof(int))) <= 0) {
          if (!io_status) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        if ((io_status = safe_read(req_fd, num_matrix, 2 * sizeof(size_t))) <= 0) {
          if (!io_status) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        response_status = ems_create(event_id, num_matrix[0], num_matrix[1]);

        if (safe_write(resp_fd, &response_status, sizeof(int)) < 0) {
          if (errno == EPIPE) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        break;

      case OP_RESERVE:
        if ((io_status = safe_read(req_fd, &event_id, sizeof(int))) <= 0) {
          if (!io_status) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        if ((io_status = safe_read(req_fd, &num_seats, sizeof(size_t))) <= 0) {
          if (!io_status) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        if ((io_status = safe_read(req_fd, xs, sizeof(size_t) * MAX_RESERVATION_SIZE)) <= 0) {
          if (!io_status) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        if ((io_status = safe_read(req_fd, ys, sizeof(size_t) * MAX_RESERVATION_SIZE)) <= 0) {
          if (!io_status) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        response_status = ems_reserve(event_id, num_seats, xs, ys);

        if (safe_write(resp_fd, &response_status, sizeof(int)) < 0) {
          if (errno == EPIPE) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        break;

      case OP_SHOW:
        if ((io_status = safe_read(req_fd, &event_id, sizeof(int))) <= 0) {
          if (!io_status) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        response_status = ems_show(resp_fd, event_id);

        if (safe_write(resp_fd, &response_status, sizeof(int)) < 0) {
          if (errno == EPIPE) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        break;

      case OP_LIST:
        response_status = ems_list_events(resp_fd);

        if (safe_write(resp_fd, &response_status, sizeof(int)) < 0) {
          if (errno == EPIPE) return CLIENT_UNRESPONSIVE;
          return CLIENT_FAILED;
        }

        break;

      default:
        break;
    }
  }

  return CLIENT_SUCCESS;
}

void *connect_clients(void *args) {
  Session_t *session_info = (Session_t *)args;
  unsigned int session_id = session_info->session_id;
  ConnectionQueue_t *queue = session_info->queue;

  char req_pipe_path[MAX_PIPE_NAME_SIZE];
  char resp_pipe_path[MAX_PIPE_NAME_SIZE];

  while (1) {
    pthread_mutex_lock(&queue->queue_lock);
    while (isEmpty(queue))
      pthread_cond_wait(&queue->available_connection, &queue->queue_lock);

    Connection_t *connection = dequeue_connection(queue);
    pthread_mutex_unlock(&queue->queue_lock);

    if (connection == NULL) {
      fprintf(stderr, "Invalid connection\n");
      continue;
    }

    memcpy(req_pipe_path, connection->req_pipe_path, MAX_PIPE_NAME_SIZE);
    memcpy(resp_pipe_path, connection->resp_pipe_path, MAX_PIPE_NAME_SIZE);
    free(connection);

    int resp_pipe = open(resp_pipe_path, O_WRONLY);
    if (resp_pipe < 0) {
      perror("Failed opening response pipe");
      continue;
    }

    if (safe_write(resp_pipe, &session_id, sizeof(int)) < 0) {
      perror("Failed to send session id to client");
      close(resp_pipe);
      continue;
    }

    int req_pipe = open(req_pipe_path, O_RDONLY);
    if (req_pipe < 0) {
      perror("Failed opening request pipe");
      close(resp_pipe);
      continue;
    }

    int job_status = handle_requests(req_pipe, resp_pipe);
    if (job_status) {
      if (job_status == CLIENT_FAILED)
        fprintf(stderr, "Failed communicating with client\n");
      else if (job_status == CLIENT_UNRESPONSIVE)
        fprintf(stderr, "Client is unresponsive. Terminating Session...\n", session_id);

      close(resp_pipe);
      close(req_pipe);
      continue;
    }

    close(req_pipe);
    close(resp_pipe);
  }

  return NULL;
}
