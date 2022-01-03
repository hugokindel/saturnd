#ifndef UTILS_H
#define UTILS_H

#include <sy5/types.h>

// Logs a syslog message.
#define log(message) syslog(LOG_NOTICE, message)

// Logs a syslog message with variadic arguments.
#define log2(message, ...) syslog(LOG_NOTICE, message, __VA_ARGS__)

// Prints an error message.
#define error(message) fprintf(stderr, EXECUTABLE_NAME ": " message); log(message)

// Prints an error message and terminate the program.
#define fatal_error(message) error(message); goto error

// Asserts a condition that returns `-1` in case of error.
#define assert(condition) if (!(condition)) { return -1; } (void)0

// Asserts a condition that terminate the program in case of error.
#define fatal_assert(condition, message) if (!(condition)) { fatal_error(message); } (void)0

// Creates a data (for sending data to a pipe).
buffer create_buffer();

// Allocate and defines every needed paths (for the pipes).
int allocate_paths();

// Cleanup and set to NULL every needed paths (for the pipes).
void cleanup_paths();

// Checks `errno` to call `perror` if needed and returns `EXIT_FAILURE`.
int get_error();

// Creates a directory by calling `mkdir` recursively on a path for every missing parts.
// Returns `-1` in case of failure, else 0.
int mkdir_recursively(const char *path, uint16_t mode);

// Writes the result in `*dest`.
// Returns `-1` in case of failure, else 0.
int string_from_cstring(string *dest, const char *cstring);

// Writes the result in `*dest`.
// Returns `-1` in case of failure, else 0.
int cstring_from_string(char **dest, const string *string);

// Writes the result in `*dest`.
// Returns `-1` in case of failure, else the number of characters read.
int timing_from_strings(timing *dest, const char *minutes_str, const char *hours_str, const char *daysofweek_str);

// Writes a text representation of timing in `*dest`, and adds a trailing `\0`.
// The data must be able to hold at least `TIMING_TEXT_MIN_BUFFERSIZE` characters.
// Returns `-1` in case of failure, else the number of characters written.
int timing_string_from_timing(char *dest, const timing *timing);

// Writes the result in `*dest`.
// Returns `-1` in case of failure, else the number of characters read.
int timing_field_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max);

// Writes the result in `*dest`.
// Returns `-1` in case of failure, else the number of characters read.
int timing_range_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max);

// Writes the result in `*dest`.
// Returns `-1` in case of failure, else the number of characters read.
int timing_uint_from_string(unsigned long int *dest, const char *string);

// Writes the result in `*dest`.
// Returns `-1` in case of failure, else the number of characters written.
int timing_string_from_field(char *dest, unsigned int min, unsigned int max, uint64_t field);

// Writes the result in `*dest`.
// Returns `-1` in case of failure, else the number of characters written.
int timing_string_from_range(char *dest, unsigned int start, unsigned int stop);

// Parses a commandline struct in `*dest` from `argc` and `argv`.
// Returns `-1` in case of failure, else 0.
int commandline_from_args(commandline *dest, unsigned int argc, char *argv[]);

// Writes a `data` to a file descriptor.
int write_buffer(int fd, const buffer *buf);

// Writes an `uint_8` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_uint8(buffer *buf, const uint8_t *n);

// Writes an `uint_16` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_uint16(buffer *buf, const uint16_t *n);

// Writes an `uint_32` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_uint32(buffer *buf, const uint32_t *n);

// Writes an `uint_64` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_uint64(buffer *buf, const uint64_t *n);

// Writes an `string` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_string(buffer *buf, const string *string);

// Writes an `timing` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_timing(buffer *buf, const timing *timing);

// Writes an `commandline` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_commandline(buffer *buf, const commandline *commandline);

// Writes an `task` in big endian to a `data`.
// Returns `-1` in case of failure, else 0.
int write_task(buffer *buf, const task *task, bool write_taskid);

// Writes an `task[]` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_task_array(buffer *buf, const task *tasks);

// Writes an `run` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_run(buffer *buf, const run *run);

// Writes an `run[]` (from host byte order to big endian order) to a `data`.
// Returns `-1` in case of failure, else 0.
int write_run_array(buffer *buf, const run *runs);

// Reads an `uint_8` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_uint8(int fd, uint8_t *n);

// Reads an `uint_16` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_uint16(int fd, uint16_t *n);

// Reads an `uint_32` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_uint32(int fd, uint32_t *n);

// Reads an `uint_64` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_uint64(int fd, uint64_t *n);

// Reads an `string` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_string(int fd, string *string);

// Reads an `timing` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_timing(int fd, timing *timing);

// Reads an `commandline` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_commandline(int fd, commandline *commandline);

// Reads an `task` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_task(int fd, task *task, bool read_taskid);

// Reads an `task[]` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_task_array(int fd, task **tasks);

// Reads an `run` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_run(int fd, run *run);

// Reads an `run[]` (from big endian order to host byte order) to a file descriptor.
// Returns `-1` in case of failure, else 0.
int read_run_array(int fd, run **runs);

int copy_timing(timing *dest, const timing *src);

int copy_string(string *dest, const string *src);

int copy_commandline(commandline *dest, const commandline *src);

int copy_task(task *dest, const task *src);

int free_string(string *string);

int free_commandline(commandline *commandline);

int free_task(task *task);

#endif // UTILS_H.