#include "io.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int parse_uint(int fd, unsigned int *value, char *next) {
  char buf[16];

  int i = 0;
  while (1) {
    ssize_t read_bytes = read(fd, buf + i, 1);
    if (read_bytes == -1) {
      return 1;
    } else if (read_bytes == 0) {
      *next = '\0';
      break;
    }

    *next = buf[i];

    if (buf[i] > '9' || buf[i] < '0') {
      buf[i] = '\0';
      break;
    }

    i++;
  }

  unsigned long ul = strtoul(buf, NULL, 10);

  if (ul > UINT_MAX) {
    return 1;
  }

  *value = (unsigned int)ul;

  return 0;
}

int print_uint(int fd, unsigned int value) {
  char buffer[16];
  size_t i = 16;

  for (; value > 0; value /= 10) {
    buffer[--i] = '0' + (char)(value % 10);
  }

  if (i == 16) {
    buffer[--i] = '0';
  }

  while (i < 16) {
    ssize_t written = write(fd, buffer + i, 16 - i);
    if (written == -1) {
      return 1;
    }

    i += (size_t)written;
  }

  return 0;
}

int print_str(int fd, const char *str) {
  size_t len = strlen(str);
  while (len > 0) {
    ssize_t written = write(fd, str, len);
    if (written == -1) {
      return 1;
    }

    str += (size_t)written;
    len -= (size_t)written;
  }

  return 0;
}

ssize_t safe_read(int fd, void *buf, size_t nbytes) {
  ssize_t completed_bytes = 0;

  while (nbytes > 0) {
    ssize_t rd_bytes = read(fd, buf + completed_bytes, nbytes);

    if (rd_bytes < 0)
      return -1;
    else if (rd_bytes == 0)
      break;

    nbytes -= (size_t)rd_bytes;
    completed_bytes += rd_bytes;
  }

  return completed_bytes;
}

ssize_t safe_write(int fd, const void *buf, size_t nbytes) {
  ssize_t completed_bytes = 0;

  while (nbytes > 0) {
    ssize_t wr_bytes = write(fd, buf + completed_bytes, nbytes);

    if (wr_bytes < 0)
      return -1;
    else if (wr_bytes == 0)
      break;

    nbytes -= (size_t)wr_bytes;
    completed_bytes += wr_bytes;
  }

  return completed_bytes;
}
