#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "common/constants.h"
#include "common/io.h"
#include "operations.h"
#include "queue.h"
#include "sessions.h"

volatile sig_atomic_t terminate = 0;
volatile sig_atomic_t usr1_sig = 0;

void sig_handler(int sig) {
  if (sig == SIGINT)
    terminate = 1;

  else if (sig == SIGUSR1)
    usr1_sig = 1;
}

int main(int argc, char* argv[]) {
  struct sigaction sa;
  sa.sa_handler = &sig_handler;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGUSR1, &sa, NULL);

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
  if (mkfifo(reg_pipe_path, 0666) < 0) {
    perror("Failed to create register pipe");
    return 1;
  }

  if (ems_init(state_access_delay_us)) {
    if (unlink(reg_pipe_path) < 0) perror("Failed to unlink register pipe");

    return 1;
  }

  ConnectionQueue_t connect_queue;
  if (init_queue(&connect_queue) != 0) {
    if (unlink(reg_pipe_path) < 0) perror("Failed to unlink register pipe");

    ems_terminate();
    return 1;
  }

  pthread_t worker_threads[MAX_SESSION_COUNT];
  Session_t sessions[MAX_SESSION_COUNT];

  for (unsigned int i = 0; i < MAX_SESSION_COUNT; i++) {
    sessions[i].session_id = i;
    sessions[i].queue = &connect_queue;

    if (pthread_create(&worker_threads[i], NULL, connect_clients, (void*)&sessions[i]) != 0) {
      if (unlink(reg_pipe_path) < 0) perror("Failed to unlink register pipe");

      fprintf(stderr, "Failed to dispatch worker thread\n");
      ems_terminate();
      free_queue(&connect_queue);
      return 1;
    }
  }

  int register_pipe = open(reg_pipe_path, O_RDWR);
  if (register_pipe < 0) {
    if (unlink(reg_pipe_path) < 0) perror("Failed to unlink register pipe");

    perror("Failed to open register pipe\n");
    ems_terminate();
    free_queue(&connect_queue);
    return 1;
  }

  ssize_t read_status = 0;
  while (1) {
    if (terminate) break;

    char setup_buffer[SETUP_REQUEST_BUFSIZ] = {0};
    if ((read_status = safe_read(register_pipe, setup_buffer, SETUP_REQUEST_BUFSIZ)) < 0) {
      if (errno == EINTR && usr1_sig) {
        if (ems_sigusr1_action()) fprintf(stderr, "Failed executing USR1 action\n");
        usr1_sig = 0;
      } else if (errno == EINTR && terminate) {
        break;
      } else {
        fprintf(stderr, "Failed reading from register pipe\n");
      }
      continue;
    }

    if (setup_buffer[0] != OP_SETUP) continue;

    if (enqueue_connection(&connect_queue, setup_buffer)) {
      fprintf(stderr, "Failed adding new connection request to queue\n");
      continue;
    }
  }

  pthread_rwlock_wrlock(&connect_queue.termination_lock);
  connect_queue.terminate = 1;
  pthread_rwlock_unlock(&connect_queue.termination_lock);
  pthread_cond_broadcast(&connect_queue.available_connection);

  for (unsigned int i = 0; i < MAX_SESSION_COUNT; i++) {
    pthread_join(worker_threads[i], NULL);
    fprintf(stdout, "\x1b[1;94m[WORKER %.2u]: Terminated!\x1b[0m\n", i);
  }

  close(register_pipe);
  unlink(reg_pipe_path);
  free_queue(&connect_queue);
  ems_terminate();
  fprintf(stdout, "\x1b[1;95m[SERVER]: Terminated!\x1b[0m\n");

  return 0;
}