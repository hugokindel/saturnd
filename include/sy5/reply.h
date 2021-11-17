#ifndef SERVER_REPLY_H
#define SERVER_REPLY_H

// The given request was executed successfully.
#define SERVER_REPLY_OK 0x4f4b     // 'OK'

// The given request failed to execute.
#define SERVER_REPLY_ERROR 0x4552  // 'ER'

// The task is not found.
#define SERVER_REPLY_ERROR_NOT_FOUND 0x4e46  // 'NF'

// The task was never run.
#define SERVER_REPLY_ERROR_NEVER_RUN 0x4e52  // 'NR'

#endif // SERVER_REPLY_H.