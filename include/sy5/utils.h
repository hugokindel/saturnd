#ifndef UTILS_H
#define UTILS_H

#include <sy5/types.h>

// Creates a directory by calling `mkdir` recursively on a path for every missing parts.
int mkdir_recursively(const char *path, uint16_t mode);

void string_from_cstring(string *dest, const char *cstring);

void cstring_from_string(char *dest, const string *string);

int timing_from_strings(timing *dest, const char *minutes_str, const char *hours_str, const char *daysofweek_str);

int timing_string_from_timing(char *dest, const timing *timing);

int timing_field_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max);

int timing_range_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max);

int timing_uint_from_string(unsigned long int *dest, const char *string);

int timing_string_from_field(char *dest, unsigned int min, unsigned int max, uint64_t field);

int timing_string_from_range(char *dest, unsigned int start, unsigned int stop);

void commandline_from_args(commandline *dest, unsigned int argc, char *argv[]);

int write_uint8(int fd, const uint8_t *n);

int write_uint16(int fd, const uint16_t *n);

int write_uint32(int fd, const uint32_t *n);

int write_uint64(int fd, const uint64_t *n);

int write_string(int fd, const string *string);

int write_timing(int fd, const timing *timing);

int write_commandline(int fd, const commandline *commandline);

int read_uint8(int fd, uint8_t *n);

int read_uint16(int fd, uint16_t *n);

int read_uint32(int fd, uint32_t *n);

int read_uint64(int fd, uint64_t *n);

int read_string(int fd, string *string);

int read_timing(int fd, timing *timing);

int read_commandline(int fd, commandline *commandline);

#endif // UTILS_H.