#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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

int safe_write(int out_fd, const void *buffer, size_t n_bytes) {
  ssize_t completed_bytes = 0;
  while (n_bytes > 0) {
    ssize_t wbytes = write(out_fd, buffer + completed_bytes, n_bytes);

    if (wbytes < 0) {
      fprintf(stderr, "Could not write to .out file\n");
      return 1;
    }
<<<<<<< HEAD
    n_bytes -= (size_t) wbytes;
=======
    n_bytes -= (size_t)wbytes;
>>>>>>> ex3
    completed_bytes += wbytes;
  }
  return EXIT_SUCCESS;
}

struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}
