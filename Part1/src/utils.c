#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

void get_job_paths(char *job_file, char *out_file, char *dir, char *filename) {
  strcpy(job_file, dir);
  strcpy(out_file, dir);
  strcat(job_file, filename);
  strcat(out_file, filename);

  char *ext = strchr(out_file, '.');
  if (NULL != ext) {
    strcpy(ext, ".out");
  }
}

int utilwrite(int out_fd, const void *buffer, size_t n_bytes) {
  ssize_t completed_bytes = 0;
  while (n_bytes > 0) {
    ssize_t wbytes = write(out_fd, buffer + completed_bytes, n_bytes);

    if (wbytes < 0) {
      fprintf(stderr, "Could not write to .out file\n");
      return 1;
    }
    n_bytes -= (size_t) wbytes;
    completed_bytes += wbytes;
  }
  return 0;
}

struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}