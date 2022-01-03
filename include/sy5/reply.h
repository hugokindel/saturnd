#ifndef SERVER_REPLY_H
#define SERVER_REPLY_H

#include <sy5/types.h>

enum reply_item {
    // The given request was executed successfully.
    SERVER_REPLY_OK = 0x4F4B, // 'OK'

    // The given request failed to execute.
    SERVER_REPLY_ERROR = 0x4552, // 'ER'
    
    // The count of items in the enum.
    SERVER_REPLY_COUNT
};

enum reply_error_item {
    // The task is not found.
    SERVER_REPLY_ERROR_NOT_FOUND = 0x4E46, // 'NF'

    // The task was never run.
    SERVER_REPLY_ERROR_NEVER_RUN = 0x4E52, // 'NR'
    
    // The count of items in the enum.
    SERVER_REPLY_ERROR_COUNT
};

// Describes a reply to be sent from the daemon to a client after a request.
typedef struct reply {
    // Operation code (reply identifier).
    uint16_t reptype;
    
    // Error code (if any).
    uint16_t errcode;
    
    union {
        // CLIENT_REQUEST_LIST_TASKS
        struct {
            // Array of running tasks.
            task *tasks;
        };
        
        // CLIENT_REQUEST_CREATE_TASK
        struct {
            // Task ID of the new task.
            uint64_t taskid;
        };
    
        // CLIENT_REQUEST_GET_TIMES_AND_EXITCODES
        struct {
            // Array of previous runs.
            run *runs;
        };
        
        // CLIENT_REQUEST_GET_STDOUT
        // CLIENT_REQUEST_GET_STDERR
        struct {
            // Output string.
            string output;
        };
    };
} reply;

// Returns an array of names for each `reply_item`.
const char **reply_item_names();

// Returns an array of names for each `reply_error_item`.
const char **reply_error_item_names();

#endif // SERVER_REPLY_H.