#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>

#include "constants.h"
#include "operations.h"
#include "parser.h"

int main(int argc, char *argv[]) {
  unsigned int state_access_delay_ms = STATE_ACCESS_DELAY_MS;
  struct dirent *dir_entry;

  if (argc < 2) {
    fprintf(stderr, "Jobs directory was not specified\n");
    return 1;
  }

  if (argc > 2) {
    char *endptr;
    unsigned long int delay = strtoul(argv[2], &endptr, 10);

    if (*endptr != '\0' || delay > UINT_MAX) {
      fprintf(stderr, "Invalid delay value or value too large\n");
      return 1;
    }

    state_access_delay_ms = (unsigned int)delay;
  }

  if (ems_init(state_access_delay_ms)) {
    fprintf(stderr, "Failed to initialize EMS\n");
    return 1;
  }

  DIR *dir = opendir(argv[1]);
  if (NULL == dir) {
    fprintf(stderr, "Failed to open jobs directory\n");
    closedir(dir);
    return 1;
  }
  char job_dir[256];
  strcpy(job_dir, argv[1]);
  strcat(job_dir, '/');

  while ((dir_entry = readdir(dir)) != NULL) {
    if (strcmp(".", dir_entry->d_name) && strcmp("..", dir_entry->d_name) &&
        strstr(dir_entry->d_name, ".jobs")) {
      size_t num_rows, num_columns, num_coords;
      size_t xs[MAX_RESERVATION_SIZE], ys[MAX_RESERVATION_SIZE];
      unsigned int event_id, delay;

      char job_filepath[256];
      char out_filepath[256];
      parse_jobpaths(job_filepath, out_filepath, job_dir, dir_entry->d_name);

      int job_fd = open(job_filepath, O_RDONLY);
      if (job_fd < 0) {
        fprintf(stderr, "Failed to open %s file", job_filepath);
        close(job_fd);
        continue;
      }

      int out_fd = open(out_filepath, O_WRONLY | O_CREAT | O_TRUNC);
      if (out_fd < 0) {
        fprintf(stderr, "Failed to open %s file", out_filepath);
        close(job_fd);
        close(out_fd);
        continue;
      } 

      printf("> ");
      fflush(stdout);

      switch (get_next(job_fd)) {
        case CMD_CREATE:
          if (parse_create(job_fd, &event_id, &num_rows, &num_columns) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (ems_create(event_id, num_rows, num_columns)) {
            fprintf(stderr, "Failed to create event\n");
          }

          break;

        case CMD_RESERVE:
          num_coords = parse_reserve(job_fd, MAX_RESERVATION_SIZE, &event_id, xs, ys);

          if (num_coords == 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (ems_reserve(event_id, num_coords, xs, ys)) {
            fprintf(stderr, "Failed to reserve seats\n");
          }

          break;

        case CMD_SHOW:
          if (parse_show(job_fd, &event_id) != 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (ems_show(event_id, out_fd)) {
            fprintf(stderr, "Failed to show event\n");
          }

          break;

        case CMD_LIST_EVENTS:
          if (ems_list_events()) {
            fprintf(stderr, "Failed to list events\n");
          }

          break;

        case CMD_WAIT:
          if (parse_wait(job_fd, &delay, NULL) == -1) {  // thread_id is not implemented
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (delay > 0) {
            printf("Waiting...\n");
            ems_wait(delay);
          }

          break;

        case CMD_INVALID:
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;

        case CMD_HELP:
          printf(
              "Available commands:\n"
              "  CREATE <event_id> <num_rows> <num_columns>\n"
              "  RESERVE <event_id> [(<x1>,<y1>) (<x2>,<y2>) ...]\n"
              "  SHOW <event_id>\n"
              "  LIST\n"
              "  WAIT <delay_ms> [thread_id]\n"  // thread_id is not implemented
              "  BARRIER\n"                      // Not implemented
              "  HELP\n");

          break;

        case CMD_BARRIER:  // Not implemented
        case CMD_EMPTY:
          break;

        case EOC:
          break;
      }
      close(job_fd);
      close(out_fd);
    }
  }
  ems_terminate();
  closedir(dir);
  return 0;
}
