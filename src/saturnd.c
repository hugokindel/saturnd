#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sy5/utils.h>
#include <sy5/reply.h>
#include <sy5/array.h>
#include <sy5/request.h>
#include <sy5/common.h>
#include <sy5/worker.h>
#ifdef __linux__
#include <unistd.h>
#endif

static const char g_help[] =
    "usage: saturnd [OPTIONS]\n"
    "\n"
    "options:\n"
    "\t-p PIPES_DIR -> look for the pipes (or creates them if not existing) in PIPES_DIR (default: " DEFAULT_PIPES_DIR ")\n";

typedef struct thread_handle {
    pthread_t pthread;
    uint64_t taskid;
} thread_handle;

static uint64_t g_last_taskid = 0;
static thread_handle *g_threads = NULL;

int main(int argc, char *argv[]) {
    errno = 0;
    
    int exit_code = EXIT_SUCCESS;
    int used_unexisting_option = 0;
    char *tasks_directory_path = NULL;
    
    // Parse options.
    int opt;
    while ((opt = getopt(argc, argv, "hp:")) != -1) {
        switch (opt) {
        case 'h':
            printf("%s", g_help);
            return exit_code;
        case 'p':
            g_pipes_path = strdup(optarg);
            fatal_assert(g_pipes_path != NULL, "invalid `g_pipes_path`!\n");
            break;
        case '?':
            used_unexisting_option = 1;
            break;
        default:
            fprintf(stderr, "unimplemented option: %s\n", optarg);
        }
    }
    
    if (used_unexisting_option) {
        error("use `-h` for more informations\n");
    }
    
    fatal_assert(allocate_paths() != -1, "cannot define pipes!\n");
    
    tasks_directory_path = calloc(1, PATH_MAX);
    assert(tasks_directory_path);
    assert(sprintf(tasks_directory_path, "%s../tasks/", g_pipes_path) != -1);
    
    DIR *pipes_dir = opendir(g_pipes_path);
    
    // Creates the pipe's directory (recursively) if it doesn't exist.
    if (!pipes_dir) {
        fatal_assert(errno == ENOENT && mkdir_recursively(g_pipes_path, 0777) != -1, "cannot find or create the pipes directory!\n");
        pipes_dir = opendir(g_pipes_path);
        fatal_assert(pipes_dir, "cannot open the pipes directory!\n");
    }
    
    struct dirent *entry;
    int request_pipe_found = 0;
    int reply_pipe_found = 0;
    
    // Searches for the pipes files.
    while ((entry = readdir(pipes_dir)) != NULL) {
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
        int request_write_fd = open(g_request_pipe_path, O_WRONLY | O_NONBLOCK);
        if (request_write_fd != -1) {
            uint16_t opcode = 0;
            buffer buf = create_buffer();
            fatal_assert(write_uint16(&buf, &opcode) != -1, "cannot write `opcode` to request");
            fatal_assert(write_buffer(request_write_fd, &buf) != -1, "cannot write reply!\n");
            free(buf.data);
            fatal_assert(close(request_write_fd) != -1, "cannot close request pipe!\n");
            fatal_error("daemon is already running or pipes are being used by another process\n");
        }
    } else {
        // Creates the request pipe file if it doesn't exits.
        fatal_assert(mkfifo(g_request_pipe_path, 0666) != -1, "cannot create the request pipe!\n");
    }
    
    // Creates the reply pipe file if it doesn't exist.
    fatal_assert(reply_pipe_found || mkfifo(g_reply_pipe_path, 0666) != -1, "cannot create the reply pipe!\n");
    
    fatal_assert(closedir(pipes_dir) != -1, "cannot close pipes directory!\n");
    
    DIR *tasks_dir = opendir(tasks_directory_path);
    
    // Creates the tasks directory if it doesn't exists.
    if (!tasks_dir) {
        fatal_assert(errno == ENOENT && mkdir_recursively(tasks_directory_path, 0777) != -1, "cannot find or create the tasks directory!\n");
        tasks_dir = opendir(tasks_directory_path);
        fatal_assert(tasks_dir, "cannot open the tasks directory!\n");
    }
    
    // Searches for the last taskid used.
    while ((entry = readdir(tasks_dir)) != NULL) {
        char* unparsed = NULL;
        uint64_t taskid = strtoull(entry->d_name, &unparsed, 10);
    
        if (errno || (!taskid && entry->d_name == unparsed)) {
            errno = 0;
            continue;
        }
        
        // Calculate the last taskid used based on directories names.
        if (taskid >= g_last_taskid) {
            g_last_taskid = taskid + 1;
        }
    
        // Loads any existing task and create its thread.
        thread_handle thread_handle = { .taskid = taskid };
        array_push(g_threads, thread_handle);
        worker *new_worker = NULL;
        fatal_assert(create_worker(&new_worker, NULL, tasks_directory_path, taskid) != -1, "cannot create worker!\n");
        fatal_assert(array_push(g_workers, new_worker) != -1, "cannot push to `g_workers`!\n");
        fatal_assert(array_push(g_running_taskids, taskid) != -1, "cannot push to `g_running_taskids`!\n");
        fatal_assert(pthread_create(&(array_last(g_threads).pthread), NULL, worker_main, (void *) array_last(g_workers)) == 0, "cannot create task thread!\n");
    }
    
    fatal_assert(closedir(tasks_dir) != -1, "cannot close tasks directory!\n");

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
    
    while (1) {
        // Waits for requests to handle...
        int request_read_fd = open(g_request_pipe_path, O_RDONLY);
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
            fatal_assert(read_task(request_read_fd, &request.task, 0) != -1, "cannot read `task` from request!\n");
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
        case CLIENT_REQUEST_LIST_TASKS: {
            task *tasks = NULL;
    
            for (uint64_t i = 0; i < array_size(g_workers); i++) {
                worker *worker = g_workers[i];
                
                if (is_worker_running(worker->task.taskid)) {
                    array_push(tasks, worker->task);
                }
            }
    
            reply.tasks = tasks;
            reply.reptype = SERVER_REPLY_OK;
            break;
        }
        case CLIENT_REQUEST_CREATE_TASK: {
            request.task.taskid = g_last_taskid++;
    
            // Creates the task thread and save it.
            thread_handle thread_handle = { .taskid = request.task.taskid };
            array_push(g_threads, thread_handle);
            worker *new_worker = NULL;
            fatal_assert(create_worker(&new_worker, &request.task, tasks_directory_path, request.task.taskid) != -1, "cannot create worker!\n");
            fatal_assert(array_push(g_workers, new_worker) != -1, "cannot push to `g_workers`!\n");
            fatal_assert(array_push(g_running_taskids, request.task.taskid) != -1, "cannot push to `g_running_taskids`!\n");
            fatal_assert(pthread_create(&(array_last(g_threads).pthread), NULL, worker_main, (void *) array_last(g_workers)) == 0, "cannot create task thread!\n");
    
            reply.taskid = request.task.taskid;
            reply.reptype = SERVER_REPLY_OK;
            break;
        }
        case CLIENT_REQUEST_REMOVE_TASK: {
            if (!is_worker_running(request.taskid)) {
                reply.reptype = SERVER_REPLY_ERROR;
                reply.errcode = SERVER_REPLY_ERROR_NOT_FOUND;
                break;
            }
    
            fatal_assert(remove_worker(request.taskid) != -1, "cannot remove task!\n");
            
            for (uint64_t i = 0; i < array_size(g_workers); i++) {
                if (g_threads[i].taskid == request.taskid) {
                    pthread_cancel(g_threads[i].pthread);
                }
            }
            
            reply.reptype = SERVER_REPLY_OK;
            break;
        }
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES: {
            if (!is_worker_running(request.taskid)) {
                reply.reptype = SERVER_REPLY_ERROR;
                reply.errcode = SERVER_REPLY_ERROR_NOT_FOUND;
                break;
            }
    
            reply.runs = get_worker(request.taskid)->runs;
            reply.reptype = SERVER_REPLY_OK;
            break;
        }
        case CLIENT_REQUEST_GET_STDOUT:
        case CLIENT_REQUEST_GET_STDERR: {
            if (!is_worker_running(request.taskid)) {
                reply.reptype = SERVER_REPLY_ERROR;
                reply.errcode = SERVER_REPLY_ERROR_NOT_FOUND;
                break;
            }
            
            worker *task_worker = get_worker(request.taskid);
            fatal_assert(task_worker, "task worker is `NULL`!\n");
            
            if (array_empty(task_worker->runs)) {
                reply.reptype = SERVER_REPLY_ERROR;
                reply.errcode = SERVER_REPLY_ERROR_NEVER_RUN;
                break;
            }
            
            if (request.opcode == CLIENT_REQUEST_GET_STDOUT) {
                reply.output = task_worker->last_stdout;
            } else {
                reply.output = task_worker->last_stderr;
            }
            
            reply.reptype = SERVER_REPLY_OK;
            break;
        }
        case CLIENT_REQUEST_TERMINATE:
            reply.reptype = SERVER_REPLY_OK;
            break;
        default:
            reply.reptype = SERVER_REPLY_ERROR;
            reply.errcode = 0;
            break;
        }
        
        int reply_write_fd = open(g_reply_pipe_path, O_WRONLY);
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
            case CLIENT_REQUEST_LIST_TASKS: {
                fatal_assert(write_task_array(&buf, reply.tasks) != -1, "cannot write `task` to reply!\n");
                array_free(reply.tasks);
                break;
            }
            case CLIENT_REQUEST_CREATE_TASK:
                fatal_assert(write_uint64(&buf, &reply.taskid) != -1, "cannot write `taskid` to reply!\n");
                break;
            case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES: {
                fatal_assert(write_run_array(&buf, reply.runs) != -1, "cannot write `run_array` to reply!\n");
                break;
            }
            case CLIENT_REQUEST_GET_STDOUT:
            case CLIENT_REQUEST_GET_STDERR:
                fatal_assert(write_string(&buf, &reply.output) != -1, "cannot write `output` to reply!\n");
                break;
            default:
                break;
            }
        } else {
            fatal_assert(write_uint16(&buf, &reply.errcode) != -1, "cannot write `errcode` to reply!\n");
        }
    
        fatal_assert(write_buffer(reply_write_fd, &buf) != -1, "cannot write reply!\n");
        free(buf.data);
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
    array_free(g_running_taskids);
    for (uint64_t i = 0; i < array_size(g_threads); i++) {
        pthread_cancel(g_threads[i].pthread);
        pthread_join(g_threads[i].pthread, NULL);
    }
    array_free(g_workers);
    free(tasks_directory_path);
    cleanup_paths();
    
    return exit_code;
}