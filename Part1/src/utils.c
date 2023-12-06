#include <string.h>
#include <stdlib.h>

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
