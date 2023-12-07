#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

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
  ssize_t wbytes = write(out_fd, buffer, n_bytes);
  if (wbytes < 0) {
    fprintf(stderr, "Could not write to .out file\n");
    return 1;
  }
  return 0;
}