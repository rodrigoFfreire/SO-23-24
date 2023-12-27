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

  pthread_t worker_threads[MAX_SESSION_COUNT];
  ThreadInfo_t thread_info[MAX_SESSION_COUNT];

  for (unsigned int i = 0; i < MAX_SESSION_COUNT; i++) {
    thread_info[i].session_id = i;
    thread_info[i].queue = &connect_queue;

    if (pthread_create(&worker_threads[i], NULL, connect_clients, (void*)&thread_info[i]) != 0) {
      fprintf(stderr, "Failed to dispatch worker thread\n");
      return 1;
    }
  }

  int rx_register = open(reg_pipe_path, O_RDONLY);
  if (rx_register < 0 ) {
    fprintf(stderr, "Failed to open register pipe\n");
    return 1;
  }

  while (1) {
    char setup_buffer[SETUP_REQUEST_BUFSIZ] = {0};
    if (safe_read(rx_register, setup_buffer, SETUP_REQUEST_BUFSIZ)) {
      fprintf(stderr, "Failed reading from register pipe\n");
      return 1;
    }
    if (setup_buffer[0] != OP_SETUP)
      continue;

    if (enqueue_connection(&connect_queue, setup_buffer)) {
      fprintf(stderr, "Failed adding new connection request to queue\n");
      return 1;
    }
  }

  //TODO: Close Server
  free_queue(&connect_queue);
  close(rx_register);
  ems_terminate();
  unlink(reg_pipe_path);

  return 0;
}