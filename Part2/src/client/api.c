#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/constants.h"
#include "common/io.h"
#include "api.h"

static ConnectionPipes_t pipes;

int ems_setup(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path) {
  char request_buff[SETUP_REQUEST_BUFSIZ] = {0};
  unsigned int session_id;

  *request_buff = (char)OP_SETUP;
  strcpy(request_buff + sizeof(char), req_pipe_path);
  strcpy(request_buff + sizeof(char) * (MAX_PIPE_NAME_SIZE + 1), resp_pipe_path);

  strncpy(pipes.req_pipe, req_pipe_path, MAX_PIPE_NAME_SIZE);
  strncpy(pipes.resp_pipe, resp_pipe_path, MAX_PIPE_NAME_SIZE);

  if (mkfifo(req_pipe_path, 0640)) {
    perror("Could not create response pipe");
    return 1;
  }
  if (mkfifo(resp_pipe_path, 0640)) {
    perror("Could not create request pipe");
    return 1;
  }

  int server_pipe = open(server_pipe_path, O_WRONLY);
  if (server_pipe < 0) {
    perror("Could not open server pipe");
    return 1;
  }

  if (safe_write(server_pipe, request_buff, SETUP_REQUEST_BUFSIZ) < 0) {
    perror("Could not write to server pipe");
    close(server_pipe);
    return 1;
  }

  close(server_pipe);

  int resp_pipe = open(resp_pipe_path, O_RDONLY);
  if (resp_pipe < 0) {
    perror("Could not open response pipe");
    return 1;
  }
  pipes.resp_fd = resp_pipe;

  if (safe_read(resp_pipe, &session_id, sizeof(int)) < 0) {
    perror("Could not get the session ID");
    close(resp_pipe);
    return 1;
  }
  printf("Server connection established. **SESSION_ID = %u**\n", session_id);

  int req_pipe = open(req_pipe_path, O_WRONLY);
  if (req_pipe < 0) {
    perror("Could not open request pipe");
    close(resp_pipe);
    return 1;
  }
  pipes.req_fd = req_pipe;

  return 0;
}

int close_pipes(void) {
  close(pipes.req_fd);
  close(pipes.resp_fd);

  if (unlink(pipes.req_pipe) < 0) {
    perror("Failed to unlink request pipe");
    return 1;
  }

  if (unlink(pipes.resp_pipe) < 0) {
    perror("Failed to unlink response pipe");
    return 1;
  }
  return 0;
}

int ems_quit(void) {
  char op = (char)OP_QUIT;

  if (safe_write(pipes.req_fd, &op, sizeof(char)) < 0) {
    perror("Could not send quit request");
    return 1;
  }

  close_pipes();
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  char op = (char)OP_CREATE;
  int return_value = 1;
  size_t num_matrix[] = {num_rows, num_cols};

  if (safe_write(pipes.req_fd, &op, sizeof(char)) < 0) {
    perror("Could not send operation code");
    return 1;
  }
  if (safe_write(pipes.req_fd, &event_id, sizeof(int)) < 0) {
    perror("Could not send creation information");
    return 1;
  }
  if (safe_write(pipes.req_fd, num_matrix, 2 * sizeof(size_t)) < 0) {
    perror("Could not send creation information");
    return 1;
  }

  if (safe_read(pipes.resp_fd, &return_value, sizeof(int)) <= 0) {
    perror("Could not read return value of operation");
    return 1;
  }

  return return_value;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  char op = (char)OP_RESERVE;
  int return_value = 1;

  if (safe_write(pipes.req_fd, &op, sizeof(char)) < 0) {
    perror("Could not send operation code");
    return 1;
  }
  if (safe_write(pipes.req_fd, &event_id, sizeof(int)) < 0) {
    perror("Could not send reservation information");
    return 1;
  }
  if (safe_write(pipes.req_fd, &num_seats, sizeof(size_t)) < 0) {
    perror("Could not send reservation information");
    return 1;
  }
  if (safe_write(pipes.req_fd, xs, sizeof(size_t) * MAX_RESERVATION_SIZE) < 0) {
    perror("Could not send reservation information");
    return 1;
  }
  if (safe_write(pipes.req_fd, ys, sizeof(size_t) * MAX_RESERVATION_SIZE) < 0) {
    perror("Could not send reservation information");
    return 1;
  }

  if (safe_read(pipes.resp_fd, &return_value, sizeof(int)) <= 0) {
    perror("Could not read return value of operation");
    return 1;
  }

  return return_value;
}

int ems_show(int out_fd, unsigned int event_id) {
  char op = (char)OP_SHOW;
  int return_value = 1;
  size_t num_matrix[2];  // Contains num_rows, num_cols

  if (safe_write(pipes.req_fd, &op, sizeof(char)) < 0) {
    perror("Could not send operation code");
    return 1;
  }
  if (safe_write(pipes.req_fd, &event_id, sizeof(int)) < 0) {
    perror("Could not send show operation information");
    return 1;
  }

  if (safe_read(pipes.resp_fd, &num_matrix, 2 * sizeof(size_t)) <= 0) {
    perror("Could not get show operation information");
    return 1;
  }

  size_t num_seats = num_matrix[0] * num_matrix[1];
  unsigned int* seats = (unsigned int*)malloc(sizeof(int) * num_seats);
  if (seats == NULL) {
    fprintf(stderr, "Could not allocate memory for seats.\n");
    return 1;
  }
  if (safe_read(pipes.resp_fd, seats, sizeof(int) * num_seats) < 0) {
    perror("Could not get seats");
    free(seats);
    return 1;
  }

  for (size_t i = 1; i <= num_matrix[0]; i++) {
    for (size_t j = 1; j <= num_matrix[1]; j++) {
      char buffer[16];
      sprintf(buffer, "%u", seats[(i - 1) * num_matrix[1] + (j - 1)]);

      if (print_str(out_fd, buffer)) {
        perror("Error writing to file descriptor");
        free(seats);
        return 1;
      }

      if (j < num_matrix[1]) {
        if (print_str(out_fd, " ")) {
          perror("Error writing to file descriptor");
          free(seats);
          return 1;
        }
      }
    }

    if (print_str(out_fd, "\n")) {
      perror("Error writing to file descriptor");
      free(seats);
      return 1;
    }
  }
  free(seats);

  if (safe_read(pipes.resp_fd, &return_value, sizeof(int)) <= 0) {
    perror("Could not read return value of operation");
    return 1;
  }

  return return_value;
}

int ems_list_events(int out_fd) {
  char op = (char)OP_LIST;
  int return_value = 1;
  size_t num_seats = 0;

  if (safe_write(pipes.req_fd, &op, sizeof(char)) < 0) {
    perror("Could not send operation code");
    return 1;
  }

  if (safe_read(pipes.resp_fd, &num_seats, sizeof(size_t)) < 0) {
    perror("Could not get list operation information");
    return 1;
  }

  unsigned int* ids = (unsigned int*)malloc(sizeof(int) * num_seats);
  if (ids == NULL) {
    fprintf(stderr, "Could not allocate memory for event ids.\n");
    return 1;
  }
  if (safe_read(pipes.resp_fd, ids, sizeof(int) * num_seats) < 0) {
    perror("Could not read list operation information");
    free(ids);
    return 1;
  }

  if (!num_seats) {
    if (print_str(out_fd, "No events\n")) {
      perror("Error writing to file descriptor");
      free(ids);
      return 1;
    }
  } else {
    for (size_t i = 0; i < num_seats; i++) {
      char buff[] = "Event: ";
      if (print_str(out_fd, buff)) {
        perror("Error writing to file descriptor");
        free(ids);
        return 1;
      }

      char id[16];
      sprintf(id, "%u\n", ids[i]);
      if (print_str(out_fd, id)) {
        perror("Error writing to file descriptor");
        free(ids);
        return 1;
      }
    }
  }
  free(ids);

  if (safe_read(pipes.resp_fd, &return_value, sizeof(int)) <= 0) {
    perror("Could not read return value of operation");
    return 1;
  }

  return return_value;
}
