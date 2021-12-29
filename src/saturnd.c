#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sy5/utils.h>
#include <sy5/reply.h>
#include <sy5/request.h>

#ifdef __linux__
#include <unistd.h>
#endif

uint64_t g_last_taskid;
uint32_t g_nbtasks;
task g_tasks[MAX_TASKS];
uint32_t g_nbruns[MAX_TASKS];
run g_runs[MAX_TASKS][MAX_RUNS_HISTORY];
string g_stdouts[MAX_TASKS];
string g_stderrs[MAX_TASKS];

const char usage_info[] =
    "usage: saturnd [OPTIONS]\n"
    "\n"
    "options:\n"
    "\t-p PIPES_DIR -> look for the pipes (or creates them if not existing) in PIPES_DIR (default: " DEFAULT_PIPES_DIR ")\n";

int main(int argc, char *argv[]) {
    errno = 0;
    
    int exit_code = EXIT_SUCCESS;
    char *pipes_directory_path = NULL;
    char *request_pipe_path =  NULL;
    char *reply_pipe_path = NULL;
    bool used_unexisting_option = false;
    
    // Parse options.
    int opt;
    while ((opt = getopt(argc, argv, "hp:")) != -1) {
        switch (opt) {
        case 'h':
            printf("%s", usage_info);
            return exit_code;
        case 'p':
            pipes_directory_path = strdup(optarg);
            fatal_assert(pipes_directory_path != NULL, "invalid `pipes_directory_path`!\n");
            break;
        case '?':
            used_unexisting_option = true;
            break;
        default:
            fprintf(stderr, "unimplemented option: %s\n", optarg);
        }
    }
    
    if (used_unexisting_option) {
        error("use `-h` for more informations\n");
    }
    
    fatal_assert(allocate_paths(&pipes_directory_path, &request_pipe_path, &reply_pipe_path) != -1, "cannot define pipes!\n");
    
    DIR *dir = opendir(pipes_directory_path);
    
    // Creates the pipes' directory (recursively) if it doesn't exist.
    if (!dir) {
        fatal_assert(errno == ENOENT && mkdir_recursively(pipes_directory_path, 0777) != -1, "cannot find or create the pipes directory!\n");
        dir = opendir(pipes_directory_path);
        fatal_assert(dir, "cannot open the pipes directory!\n");
    }
    
    struct dirent *entry;
    int request_pipe_found = 0;
    int reply_pipe_found = 0;
    
    // Searches for the pipes files.
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, REQUEST_PIPE_NAME) == 0) {
            request_pipe_found = 1;
        } else if (strcmp(entry->d_name, REPLY_PIPE_NAME) == 0) {
            reply_pipe_found = 1;
        }
        
        if (request_pipe_found && reply_pipe_found) {
            break;
        }
    }
    
    // If we find and can open the request pipe file for writing successfully, it means it is already being read by
    // another process, in which case we can assume a daemon is already running.
    if (request_pipe_found) {
        int request_write_fd = open(request_pipe_path, O_WRONLY | O_NONBLOCK);
        if (request_write_fd != -1) {
            uint16_t opcode = 0;
            fatal_assert(write_uint16(request_write_fd, &opcode) != -1, "cannot send `opcode` to request");
            fatal_assert(close(request_write_fd) != -1, "cannot close request pipe!\n");
            fatal_error("daemon is already running or pipes are being used by another process\n");
        }
    } else {
        // Creates the request pipe file if it doesn't exits.
        fatal_assert(mkfifo(request_pipe_path, 0666) != -1, "cannot create the request pipe!\n");
    }
    
    // Creates the reply pipe file if it doesn't exist.
    fatal_assert(reply_pipe_found || mkfifo(reply_pipe_path, 0666) != -1, "cannot create the reply pipe!\n");
    
    closedir(dir);
    
    // Attempts a double fork to become a daemon.
    pid_t pid = fork();
    
    fatal_assert(pid != -1, "cannot create the  daemon through double fork (failed initial fork)!\n");
    if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    pid = fork();
    
    fatal_assert(pid != -1, "cannot create the  daemon through double fork (failed second fork)!\n");
    if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    log("daemon started.\n");
    
    while (true) {
        // Waits for requests to handle...
        int request_read_fd = open(request_pipe_path, O_RDONLY);
        fatal_assert(request_read_fd != -1, "cannot open request pipe!\n");
        
        // Reads a request.
        request request;
        fatal_assert(read_uint16(request_read_fd, &request.opcode) != -1, "cannot read `opcode` from request!\n");
    
        log2("request received `%s`.\n", request_item_names()[request.opcode]);
        
        if (request.opcode == 0) {
            log("no reply required.\n");
            close(request_read_fd);
            continue;
        }
        
        switch (request.opcode) {
        case CLIENT_REQUEST_CREATE_TASK:
            fatal_assert(read_task(request_read_fd, &request.task, false) != -1, "cannot read `task` from request!\n");
            break;
        case CLIENT_REQUEST_REMOVE_TASK:
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
        case CLIENT_REQUEST_GET_STDOUT:
        case CLIENT_REQUEST_GET_STDERR:
            fatal_assert(read_uint64(request_read_fd, &request.taskid) != -1, "cannot read `taskid` from request!\n");
            break;
        default:
            break;
        }
    
        fatal_assert(close(request_read_fd) != -1, "cannot close request pipe!\n");
        
        // Writes a reply.
        reply reply;
        switch (request.opcode) {
        case CLIENT_REQUEST_CREATE_TASK:
            // TODO: Create the task and its data in a pthread.
            reply.reptype = SERVER_REPLY_OK;
            break;
        case CLIENT_REQUEST_REMOVE_TASK:
            // TODO: Stop the task's pthread and remove its data if `taskid` does exists.
            // TODO: Set `reply.errcode` to 'SERVER_REPLY_ERROR_NOT_FOUND' if `taskid` does not exists.
            reply.reptype = SERVER_REPLY_OK;
            break;
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
            // TODO: Set `reply.errcode` to 'SERVER_REPLY_ERROR_NOT_FOUND' if `taskid` does not exists.
            reply.reptype = SERVER_REPLY_OK;
            break;
        case CLIENT_REQUEST_GET_STDOUT:
        case CLIENT_REQUEST_GET_STDERR:
            // TODO: Set `reply.errcode` to 'SERVER_REPLY_ERROR_NOT_FOUND' if `taskid` does not exists.
            // TODO: Set `reply.errcode` to 'SERVER_REPLY_ERROR_NEVER_RUN' if `taskid` exists but has not yet been run.
            reply.reptype = SERVER_REPLY_OK;
            break;
        default:
            reply.reptype = SERVER_REPLY_OK;
            break;
        }
        
        int reply_write_fd = open(reply_pipe_path, O_WRONLY);
        fatal_assert(reply_write_fd != -1, "cannot open reply pipe!\n");
        
        if (reply.reptype == SERVER_REPLY_OK) {
            log2("sending to client `%s`.\n", reply_item_names()[reply.reptype]);
        } else {
            log2("sending to client `%s` with error `%s`.\n", reply_item_names()[reply.reptype], reply_error_item_names()[reply.errcode]);
        }
    
        fatal_assert(write_uint16(reply_write_fd, &reply.reptype) != -1, "cannot write `reptype` to reply!\n");
        
        if (reply.reptype == SERVER_REPLY_OK) {
            switch (request.opcode) {
            case CLIENT_REQUEST_LIST_TASKS:
                fatal_assert(write_task_array(reply_write_fd, &g_nbtasks, g_tasks, true) != -1, "cannot write `task` to reply!\n");
                break;
            case CLIENT_REQUEST_CREATE_TASK:
                fatal_assert(write_uint64(reply_write_fd, &g_last_taskid) != -1, "cannot write `taskid` to reply!\n");
                break;
            case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
                fatal_assert(write_run_array(reply_write_fd, &g_nbruns[request.taskid], g_runs[request.taskid]) != -1, "cannot write `run_array` to reply!\n");
                break;
            case CLIENT_REQUEST_GET_STDOUT:
                fatal_assert(write_string(reply_write_fd, &g_stdouts[request.taskid]) != -1, "cannot write `output` to reply!\n");
                break;
            case CLIENT_REQUEST_GET_STDERR:
                fatal_assert(write_string(reply_write_fd, &g_stderrs[request.taskid]) != -1, "cannot write `output` to reply!\n");
                break;
            default:
                break;
            }
        } else {
            fatal_assert(write_uint16(reply_write_fd, &reply.errcode) != -1, "cannot write `errcode` to reply!\n");
        }
    
        fatal_assert(close(reply_write_fd) != -1, "cannot close reply pipe!\n");
        
        if (request.opcode == CLIENT_REQUEST_TERMINATE) {
            break;
        }
    }
    
    log("daemon shutting down...");
    
    goto cleanup;
    
    error:
    exit_code = get_error();
    
    cleanup:
    cleanup_paths(&pipes_directory_path, &request_pipe_path, &reply_pipe_path);
    
    return exit_code;
}