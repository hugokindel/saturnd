#ifndef UTILS_H
#define UTILS_H

#include <sy5/types.h>

// Creates a directory by calling `mkdir` recursively on a path for every missing parts.
int mkdir_recursively(const char *path, uint16_t mode);

// TODO: Add documentation.
int string_from_cstring(string *dest, const char *cstring);

// TODO: Add documentation.
int cstring_from_string(char *dest, const string *string);

// TODO: Add documentation.
int timing_from_strings(timing *dest, const char *minutes_str, const char *hours_str, const char *daysofweek_str);

// TODO: Add documentation.
int timing_string_from_timing(char *dest, const timing *timing);

// TODO: Add documentation.
int timing_field_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max);

// TODO: Add documentation.
int timing_range_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max);

// TODO: Add documentation.
int timing_uint_from_string(unsigned long int *dest, const char *string);

// TODO: Add documentation.
int timing_string_from_field(char *dest, unsigned int min, unsigned int max, uint64_t field);

// TODO: Add documentation.
int timing_string_from_range(char *dest, unsigned int start, unsigned int stop);

// TODO: Add documentation.
int commandline_from_args(commandline *dest, unsigned int argc, char *argv[]);

// TODO: Add documentation.
int write_uint8(int fd, const uint8_t *n);

// TODO: Add documentation.
int write_uint16(int fd, const uint16_t *n);

// TODO: Add documentation.
int write_uint32(int fd, const uint32_t *n);

// TODO: Add documentation.
int write_uint64(int fd, const uint64_t *n);

// TODO: Add documentation.
int write_string(int fd, const string *string);

// TODO: Add documentation.
int write_timing(int fd, const timing *timing);

// TODO: Add documentation.
int write_commandline(int fd, const commandline *commandline);

// TODO: Add documentation.
int write_task(int fd, const task *task, bool write_taskid);

// TODO: Add documentation.
int write_task_array(int fd, const uint32_t *nbtasks, const task tasks[], bool read_taskid);

// TODO: Add documentation.
int write_run(int fd, const run *run);

// TODO: Add documentation.
int write_run_array(int fd, const uint32_t *nbruns, const run runs[]);

// TODO: Add documentation.
int read_uint8(int fd, uint8_t *n);

// TODO: Add documentation.
int read_uint16(int fd, uint16_t *n);

// TODO: Add documentation.
int read_uint32(int fd, uint32_t *n);

// TODO: Add documentation.
int read_uint64(int fd, uint64_t *n);

// TODO: Add documentation.
int read_string(int fd, string *string);

// TODO: Add documentation.
int read_timing(int fd, timing *timing);

// TODO: Add documentation.
int read_commandline(int fd, commandline *commandline);

// TODO: Add documentation.
int read_task(int fd, task *task, bool read_taskid);

// TODO: Add documentation.
int read_task_array(int fd, task task[], bool read_taskid);

// TODO: Add documentation.
int read_run(int fd, run *run);

// TODO: Add documentation.
int read_run_array(int fd, run run[]);

#endif // UTILS_H.