#ifndef TYPES_H
#define TYPES_H

// Includes the `bool` type.
#include <stdbool.h>

// Includes integer types (`uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `int8_t`, `int16_t`, `int32_t`, `int64_t`).
#include <stdint.h>

// The maximum length of a string.
#define MAX_STRING_LENGTH 1024

// The maximum number of arguments within a command line.
#define MAX_COMMANDLINE_ARGUMENTS 32

// The maximum length of a timing when defined as a string.
#define MAX_TIMING_STRING_LENGTH 1024

// The maximum number of tasks that can be handled by a daemon.
#define MAX_TASKS 64

// The maximum size of a run history array.
#define MAX_RUNS_HISTORY 64

// The default pipes directory to use.
#define DEFAULT_PIPES_DIR "/tmp/<USERNAME>/saturnd/pipes"

// The name of the default request pipe.
#define REQUEST_PIPE_NAME "saturnd-request-pipe"

// The name of the default reply pipe.
#define REPLY_PIPE_NAME "saturnd-reply-pipe"

// The name of the currently built executable (used for logging purposes).
#ifdef CASSINI
#define EXECUTABLE_NAME "cassini"
#else
#define EXECUTABLE_NAME "saturnd"
#endif

// Describes a string.
typedef struct string {
    // Length of the string.
    uint32_t length;
    
    // Data of the string.
    uint8_t data[MAX_STRING_LENGTH];
} string;

// Describes multiple time references.
typedef struct timing {
    // Minutes of an hour represented by a bit map (starts with minute 0 at the least significant bit).
    // e.g. 1000000000000000000000000000000000011111110000 -> from minute 4 to minute 10 (included) and minute 45.
    uint64_t minutes;
    
    // Hours of a day represented by a bit map (starts with hour 0 at the least significant bit).
    // e.g. 1110 -> from hour 1 to hour 3 (included).
    uint32_t hours;
    
    // Days of a week represented by a bit map (starts with sunday at the least significant bit).
    // e.g. 1011100 -> from tuesday to thursday (included) and saturday.
    uint8_t daysofweek;
} timing;

// Describes a command line.
typedef struct commandline {
    // Count of arguments.
    uint32_t argc;
    
    // Values for each argument.
    string argv[MAX_COMMANDLINE_ARGUMENTS];
} commandline;

// Describes a scheduled task.
typedef struct task {
    // ID of the task.
    uint64_t taskid;
    
    // Timing references of the task.
    timing timing;
    
    // Command line of the task.
    commandline commandline;
} task;

// Describes a scheduled task run.
typedef struct run {
    // Time of the run in second since EPOCH.
    uint64_t time;
    
    // Exit value of the run.
    uint16_t exitcode;
} run;

#endif // TYPES_H.