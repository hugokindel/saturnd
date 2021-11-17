#ifndef TIMING_H
#define TIMING_H

#include <sy5/types.h>

#define TIMING_TEXT_MIN_BUFFERSIZE 1024

// Writes the result in *dest. In case of success, returns the number of characters read (>0). In case of failure,
// returns 0.
int timing_from_strings(sy5_timing *dest, char *minutes_str, char *hours_str, char *daysofweek_str);

// Writes a text representation of timing in the buffer pointed to by dest, and adds a trailing '\0'. The buffer must
// be able to hold at least TIMING_TEXT_MIN_BUFFERSIZE characters. Returns the number of characters written, *excluding*
// the trailing '\0'.
int timing_string_from_timing(char *dest, const sy5_timing *timing);

// Writes the result in *dest.
int timing_field_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max);

// Writes the result in *dest.
int timing_range_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max);

// Writes the result in *dest.
int timing_uint_from_string(unsigned long int *dest, const char *string);

// Writes the result in *dest.
int timing_string_from_field(char *dest, unsigned int min, unsigned int max, uint64_t field);

// Writes the result in *dest.
int timing_string_from_range(char *dest, unsigned int start, unsigned int stop);

#endif // TIMING_H.