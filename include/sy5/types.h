#ifndef TYPES_H
#define TYPES_H

#include <sys/types.h>
#include <limits.h>

// Includes integer types (`uint8`, `uint16`, `uint32`, `uint64`, `int8`, `int16`, `int32`, `int64`).
#include <stdint.h>

// Describes a string.
typedef struct sy5_string {
    // Length of the string.
    uint32_t length;
    
    // Data of the string.
    uint8_t data[];
} sy5_string;

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
    sy5_string argv[];
} sy5_commandline;

// Describes a scheduled task.
typedef struct sy5_task {
    // ID of the task.
    uint64_t taskid;
    
    // Timing references of the task.
    sy5_timing timing;
    
    // Command line of the task.
    sy5_string commandline;
} sy5_task;

// Describes a scheduled task.
typedef struct sy5_run {
    // Time of the run in second since EPOCH.
    uint64_t time;
    
    // Exit value of the run.
    uint16_t exitcode;
} sy5_run;

#endif // TYPES_H.