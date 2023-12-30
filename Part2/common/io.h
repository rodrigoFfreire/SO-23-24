#ifndef COMMON_IO_H
#define COMMON_IO_H

#include <sys/types.h>

/// Parses an unsigned integer from the given file descriptor.
/// @param fd The file descriptor to read from.
/// @param value Pointer to the variable to store the value in.
/// @param next Pointer to the variable to store the next character in.
/// @return 0 if the integer was read successfully, 1 otherwise.
int parse_uint(int fd, unsigned int *value, char *next);

/// Prints an unsigned integer to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param value The value to write.
/// @return 0 if the integer was written successfully, 1 otherwise.
int print_uint(int fd, unsigned int value);

/// Writes a string to the given file descriptor.
/// @param fd The file descriptor to write to.
/// @param str The string to write.
/// @return 0 if the string was written successfully, 1 otherwise.
int print_str(int fd, const char *str);

/// Safely reads nbytes from the given file descriptor to buf
/// @param fd File descriptor to read from
/// @param buf Buffer to hold read data
/// @param nbytes Amount of data to read
/// @return 0 if data was read successfully, -1 otherwise.
ssize_t safe_read(int fd, void *buf, size_t nbytes);

/// Safely writes nbytes to the given file descriptor from buf
/// @param fd File descriptor to write to
/// @param buf Buffer to hold write data
/// @param nbytes Amount of data to read
/// @return 0 if data was written successfully, -1 otherwise.
ssize_t safe_write(int fd, const void *buf, size_t nbytes);

#endif  // COMMON_IO_H
