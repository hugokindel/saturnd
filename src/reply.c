#include <sy5/reply.h>

static const char *reply_item_names_array[] = {
    [SERVER_REPLY_OK] = "SERVER_REPLY_OK",
    [SERVER_REPLY_ERROR] = "SERVER_REPLY_ERROR",
    
    [SERVER_REPLY_COUNT] = 0,
};

static const char *reply_error_item_names_array[] = {
    [SERVER_REPLY_ERROR_NOT_FOUND] = "SERVER_REPLY_ERROR_NOT_FOUND",
    [SERVER_REPLY_ERROR_NEVER_RUN] = "SERVER_REPLY_ERROR_NEVER_RUN",
    
    [SERVER_REPLY_ERROR_COUNT] = 0,
};

const char **reply_item_names() {
    return reply_item_names_array;
}

const char **reply_error_item_names() {
    return reply_error_item_names_array;
}