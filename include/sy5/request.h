#ifndef CLIENT_REQUEST_H
#define CLIENT_REQUEST_H

#include <sy5/types.h>

enum sy5_request_item {
    // Lists all tasks.
    CLIENT_REQUEST_LIST_TASKS = 0x4C53, // 'LS'.
    
    // Creates a task to perform at a given point in time.
    CLIENT_REQUEST_CREATE_TASK = 0x4352, // 'CR'.
    
    // Removes a scheduled task.
    CLIENT_REQUEST_REMOVE_TASK = 0x524D, // 'RM'.
    
    // Lists all previous execution times and exit codes of a scheduled task.
    CLIENT_REQUEST_GET_TIMES_AND_EXITCODES = 0x5458, // 'TX'.
    
    // Displays standard output from the latest execution of a scheduled task.
    CLIENT_REQUEST_GET_STDOUT = 0x534F, // 'SO'.
    
    // Displays standard error output from the latest execution of a scheduled task.
    CLIENT_REQUEST_GET_STDERR = 0x5345, // 'SE'.
    
    // Terminates the daemon.
    CLIENT_REQUEST_TERMINATE = 0x544D, // 'TM'.
    
    CLIENT_REQUEST_COUNT
};

// Describes a request to be sent from a client to the daemon.
typedef struct sy5_request {
    // Operation code (request identifier).
    uint16_t opcode;
    
    // Data format per request identifier.
    union {
        // CLIENT_REQUEST_CREATE_TASK
        struct {
            // Precise when to perform a given task.
            sy5_timing timing;
            
            // Command line arguments of a given task.
            sy5_commandline commandline;
        };
        
        // CLIENT_REQUEST_REMOVE_TASK
        // CLIENT_REQUEST_GET_TIMES_AND_EXITCODES
        // CLIENT_REQUEST_GET_STDOUT
        // CLIENT_REQUEST_GET_STDERR
        struct {
            // Task ID on which operate.
            uint64_t taskid;
        };
    };
} sy5_request;

const char **request_item_names();

#endif // CLIENT_REQUEST_H.