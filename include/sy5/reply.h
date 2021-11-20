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
} reply;

// TODO: Add documentation.
const char **reply_item_names();

// TODO: Add documentation.
const char **reply_error_item_names();

#endif // SERVER_REPLY_H.