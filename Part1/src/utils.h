#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <time.h>

/// Parses the name of the file and modifies the extension to .out.
/// @param job_filepath Path for the .job file.
/// @param out_filepath Path for the .out file.
/// @param dir Jobs directory name.
/// @param filename File named read from readdir().
void get_job_paths(char *job_filepath, char *out_filepath, char *dir, char *filename);


/// Writes N_BYTES from BUFFER to OUT_FD. Checks for partial writes
/// @param out_fd Output file descriptor
/// @param buffer Buffer
/// @param n_bytes N bytes to write.
int utilwrite(int out_fd, const void *buffer, size_t n_bytes);


/// Simple abstraction to malloc with error msg
/// @param size size of memory to allocate
void *safe_malloc(size_t size);


/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
struct timespec delay_to_timespec(unsigned int delay_ms);

#endif
