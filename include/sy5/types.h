#ifndef TYPES_H
#define TYPES_H

// Includes integer types (`uint8`, `uint16`, `uint32`, `uint64`, `int8`, `int16`, `int32`, `int64`).
#include <stdint.h>

// Describes multiple time references.
typedef struct sy5_timing {
    // Minutes of an hour represented by a bit map (starts with minute 0 at the least significant bit).
    // e.g. 1000000000000000000000000000000000011111110000 -> from minute 4 to minute 10 (included) and minute 45.
    uint64_t minutes;
    
    // Hours of a day represented by a bit map (starts with hour 0 at the least significant bit).
    // e.g. 1110 -> from hour 1 to hour 3 (included).
    uint32_t hours;
    
    // Days of a week represented by a bit map (starts with sunday at the least significant bit).
    // e.g. 1011100 -> from tuesday to thursday (included) and saturday.
    uint8_t daysofweek;
} sy5_timing;

// Describes a command line.
typedef struct sy5_commandline {
    // Count of arguments.
    uint32_t argc;
    
    // Values for each argument.
    char *argv[];
} sy5_commandline;

#endif // TYPES_H.