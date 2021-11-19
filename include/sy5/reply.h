#ifndef SERVER_REPLY_H
#define SERVER_REPLY_H

#include <sy5/types.h>

enum sy5_reply_item {
    // The given request was executed successfully.
    SERVER_REPLY_OK = 0x4F4B, // 'OK'

    // The given request failed to execute.
    SERVER_REPLY_ERROR = 0x4552, // 'ER'
    
    SERVER_REPLY_COUNT
};

enum sy5_reply_error_item {
    // The task is not found.
    SERVER_REPLY_ERROR_NOT_FOUND = 0x4E46, // 'NF'

    // The task was never run.
    SERVER_REPLY_ERROR_NEVER_RUN = 0x4E52, // 'NR'
    
    SERVER_REPLY_ERROR_COUNT
};

// TODO: Remove ?
// Describes a reply to be sent from the daemon to a client after a request.
typedef struct sy5_reply {
    // Operation code (reply identifier).
    uint16_t reptype;
    
    // Data format per reply identifier.
    union {
        // -> WHEN REPTYPE IS SERVER_REPLY_OK
        
        // CLIENT_REQUEST_LIST_TASKS
        struct {
            // Number of tasks.
            uint32_t nbtasks;
            
            // Tasks data.
            task tasks[];
        };
        
        // CLIENT_REQUEST_CREATE_TASK
        struct {
            // ID of the created task.
            uint64_t taskid;
        };
    
        // CLIENT_REQUEST_GET_TIMES_AND_EXITCODES
        struct {
            // Number of tasks.
            uint32_t nbruns;
        
            // Tasks data.
            run run[];
        };
    
        // CLIENT_REQUEST_GET_STDOUT
        // CLIENT_REQUEST_GET_STDERR
        struct {
            // Output string.
            string output;
        };
    
        // -> WHEN REPTYPE IS SERVER_REPLY_ERROR
        
        // CLIENT_REQUEST_REMOVE_TASK
        // CLIENT_REQUEST_GET_TIMES_AND_EXITCODES
        // CLIENT_REQUEST_GET_STDOUT
        // CLIENT_REQUEST_GET_STDERR
        struct {
            // Error code.
            uint16_t errcode;
        };
    };
} sy5_reply;

const char **reply_item_names();

const char **reply_error_item_names();

#endif // SERVER_REPLY_H.