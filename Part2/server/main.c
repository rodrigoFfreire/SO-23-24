#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "queue.h"

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s\n <pipe_path> [delay]\n", argv[0]);
    return 1;
  }

  char* endptr;
  unsigned int state_access_delay_us = STATE_ACCESS_DELAY_US;
  if (argc == 3) {
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_us = (unsigned int)delay;
  }

  char reg_pipe_path[MAX_PIPE_NAME_SIZE] = {0};
  strcpy(reg_pipe_path, argv[1]);
  if (mkfifo(reg_pipe_path, 0640) < 0) {
    fprintf(stderr, "Failed to create register pipe\n");
    return 1;
  }
  
  if (ems_init(state_access_delay_us)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  ConnectionQueue_t connect_queue;
  if (init_queue(&connect_queue) != 0) {
    fprintf(stderr, "Failed to initialize queue\n");
    return 1;
  }

  // pthread_t worker_threads[MAX_SESSION_COUNT];
  // TODO: Dispatch threads with consumer target

  int rx_register = open(reg_pipe_path, O_RDONLY);
  if (rx_register < 0 ) {
    fprintf(stderr, "Failed to open register pipe\n");
    return 1;
  }

  while (1) {
    char setup_buffer[SETUP_REQUEST_BUFSIZ];
    if (safe_read(rx_register, setup_buffer, SETUP_REQUEST_BUFSIZ)) {
      fprintf(stderr, "Failed reading from register pipe\n");
      return 1;
    }
    if (setup_buffer[0] != OP_SETUP)
      continue;

    char req_pipe_path[MAX_PIPE_NAME_SIZE];
    char resp_pipe_path[MAX_PIPE_NAME_SIZE];
    memcpy(req_pipe_path, setup_buffer + 1, MAX_PIPE_NAME_SIZE);
    memcpy(resp_pipe_path, setup_buffer + 1 + MAX_PIPE_NAME_SIZE, MAX_PIPE_NAME_SIZE);

    if (enqueue_connection(&connect_queue, req_pipe_path, resp_pipe_path)) {
      fprintf(stderr, "Failed adding new connection to queue\n");
      return 1;
    }
    fprintf(stdout, "[OUT]: Client connected!\n");
  }

  //TODO: Close Server

  ems_terminate();
}