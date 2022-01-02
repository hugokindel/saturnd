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
#include <sy5/array.h>
#include <pthread.h>

#ifdef __linux__
#include <unistd.h>
#endif

typedef struct worker {
    task task;
    pthread_t thread;
} worker;

static const char g_help[] =
    "usage: saturnd [OPTIONS]\n"
    "\n"
    "options:\n"
    "\t-p PIPES_DIR -> look for the pipes (or creates them if not existing) in PIPES_DIR (default: " DEFAULT_PIPES_DIR ")\n";

static worker **g_workers = NULL;
static uint64_t *g_running_taskids = NULL;
static pthread_mutex_t g_running_taskids_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint64_t g_taskid = 0;

static int create_worker(worker **dest, task task) {
    worker *tmp = malloc(sizeof(worker));
    assert(tmp);
    
    tmp->task = task;
    *dest = tmp;
    
    return 0;
}

static int free_worker(worker **worker) {
    free_task(&(*worker)->task);
    free(*worker);
    *worker = NULL;
    
    return 0;
}

static int is_task_alive(bool *is_still_alive, uint64_t taskid) {
    assert(g_running_taskids != NULL);
    
    for (uint64_t i = 0; i < array_size(g_running_taskids); i++) {
        if (g_running_taskids[i] == taskid) {
            *is_still_alive = true;
            break;
        }
    }
    
    return 0;
}

static int remove_task(uint64_t taskid) {
    assert(g_running_taskids != NULL);
    assert(pthread_mutex_lock(&g_running_taskids_mutex) == 0);
    
    for (uint64_t i = 0; i < array_size(g_running_taskids); i++) {
        if (g_running_taskids[i] == taskid) {
            array_remove(g_running_taskids, i);
            break;
        }
    }
    
    assert(pthread_mutex_unlock(&g_running_taskids_mutex) == 0);
    
    return 0;
}

static void *worker_thread(void *worker_arg) {
    worker *worker_to_handle = (worker *)worker_arg;
    
    pthread_mutex_lock(&g_running_taskids_mutex);
    array_push(g_running_taskids, worker_to_handle->task.taskid);
    pthread_mutex_unlock(&g_running_taskids_mutex);
    
    {
        while (true) {
            bool alive = false;
            
            if (is_task_alive(&alive, worker_to_handle->task.taskid) == -1) {
                log("cannot check if task is alive!\n");
                break;
            }
            
            if (!alive) {
                break;
            }
            
            log2("welcome from task %llu", worker_to_handle->task.taskid);
            
            usleep(1000000);
        }
    }
    
    if (remove_task(worker_to_handle->task.taskid) == -1) {
        log("cannot remove task!\n");
    }
    
    free_worker(&worker_to_handle);
    
    return NULL;
}

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
            printf("%s", g_help);
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
            buffer buf = create_buffer();
            fatal_assert(write_uint16(&buf, &opcode) != -1, "cannot write `opcode` to request");
            fatal_assert(write_buffer(request_write_fd, &buf) != -1, "cannot write reply!\n");
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

#ifdef DAEMONIZE
    // Attempts a double fork to become a daemon.
    pid_t daemon_pid = fork();
    
    fatal_assert(daemon_pid != -1, "cannot create the daemon process (failed initial fork)!\n");
    if (daemon_pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    daemon_pid = fork();
    
    fatal_assert(daemon_pid != -1, "cannot create the daemon process (failed second fork)!\n");
    if (daemon_pid != 0) {
        exit(EXIT_SUCCESS);
    }
#endif
    
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
        case CLIENT_REQUEST_CREATE_TASK: {
            request.task.taskid = g_taskid++;
            
            worker *new_worker = NULL;
            fatal_assert(create_worker(&new_worker, request.task) != -1, "cannot create worker!\n");
            fatal_assert(array_push(g_workers, new_worker) != -1, "cannot push to `g_workers`");
            fatal_assert(pthread_create(&(array_last(g_workers)->thread), NULL, worker_thread, (void *) array_last(g_workers)) == 0, "cannot create task thread!\n");
    
            reply.reptype = SERVER_REPLY_OK;
            
            break;
        }
        case CLIENT_REQUEST_REMOVE_TASK: {
            bool alive = false;
            fatal_assert(is_task_alive(&alive, request.taskid) != -1, "cannot check if task is alive!\n");
            if (!alive) {
                reply.reptype = SERVER_REPLY_ERROR;
                reply.errcode = SERVER_REPLY_ERROR_NOT_FOUND;
                break;
            }
    
            fatal_assert(remove_task(request.taskid) != -1, "cannot remove task!\n");
            
            reply.reptype = SERVER_REPLY_OK;
            
            break;
        }
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
            reply.reptype = SERVER_REPLY_ERROR;
            reply.errcode = 0;
            break;
        }
        
        int reply_write_fd = open(reply_pipe_path, O_WRONLY);
        fatal_assert(reply_write_fd != -1, "cannot open reply pipe!\n");
        
        if (reply.reptype == SERVER_REPLY_OK) {
            log2("sending to client `%s`.\n", reply_item_names()[reply.reptype]);
        } else {
            log2("sending to client `%s` with error `%s`.\n", reply_item_names()[reply.reptype], reply_error_item_names()[reply.errcode]);
        }
    
        buffer buf = create_buffer();
        fatal_assert(write_uint16(&buf, &reply.reptype) != -1, "cannot write `reptype` to reply!\n");
        
        if (reply.reptype == SERVER_REPLY_OK) {
            switch (request.opcode) {
            case CLIENT_REQUEST_LIST_TASKS:
                //fatal_assert(write_task_array(&buf, &g_nbtasks, g_tasks, true) != -1, "cannot write `task` to reply!\n");
                break;
            case CLIENT_REQUEST_CREATE_TASK:
                fatal_assert(write_uint64(&buf, &request.task.taskid) != -1, "cannot write `taskid` to reply!\n");
                break;
            case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
                //fatal_assert(write_run_array(&buf, &g_nbruns[request.taskid], g_runs[request.taskid]) != -1, "cannot write `run_array` to reply!\n");
                break;
            case CLIENT_REQUEST_GET_STDOUT:
                //fatal_assert(write_string(&buf, &g_stdouts[request.taskid]) != -1, "cannot write `output` to reply!\n");
                break;
            case CLIENT_REQUEST_GET_STDERR:
                //fatal_assert(write_string(&buf, &g_stderrs[request.taskid]) != -1, "cannot write `output` to reply!\n");
                break;
            default:
                break;
            }
        } else {
            fatal_assert(write_uint16(&buf, &reply.errcode) != -1, "cannot write `errcode` to reply!\n");
        }
    
        fatal_assert(write_buffer(reply_write_fd, &buf) != -1, "cannot write reply!\n");
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
    array_free(g_workers);
    cleanup_paths(&pipes_directory_path, &request_pipe_path, &reply_pipe_path);
    
    return exit_code;
}