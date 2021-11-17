#ifndef SERVER_REPLY_H
#define SERVER_REPLY_H

#include <sy5/types.h>

// The given request was executed successfully.
#define SERVER_REPLY_OK 0x4f4b     // 'OK'

// The given request failed to execute.
#define SERVER_REPLY_ERROR 0x4552  // 'ER'

// The task is not found.
#define SERVER_REPLY_ERROR_NOT_FOUND 0x4e46  // 'NF'

// The task was never run.
#define SERVER_REPLY_ERROR_NEVER_RUN 0x4e52  // 'NR'

// Describes a reply to be sent from the daemon to a client after a request.
struct sy5_reply {
    // Operation code (reply identifier).
    uint16_t reptype;
    
    // Data format per reply identifier.
    union {
        // CLIENT_REQUEST_LIST_TASKS
        // -> OK
        struct {
            // Number of tasks.
            uint32_t nbtasks;
            
            // Tasks data.
            sy5_task tasks[];
        };
        
        // CLIENT_REQUEST_CREATE_TASK
        // -> OK
        struct {
            // ID of the created task.
            uint64_t taskid;
        };
        
        // CLIENT_REQUEST_REMOVE_TASK
        // -> ERROR
        struct {
            // Error code.
            uint16_t errcode;
        };
        
        // CLIENT_REQUEST_GET_TIMES_AND_EXITCODES
        // -> OK
        struct {
            // Number of tasks.
            uint32_t nbruns;
    
            // Tasks data.
            sy5_run run[];
        };
        // -> ERROR
        struct {
            // Error code.
            uint16_t errcode;
        };
    
        // CLIENT_REQUEST_GET_STDOUT
        // CLIENT_REQUEST_GET_STDERR
        // -> OK
        struct {
            // Output string.
            sy5_string output;
        };
        // -> ERROR
        struct {
            // Error code.
            uint16_t errcode;
        };
    };
};

#endif // SERVER_REPLY_H.