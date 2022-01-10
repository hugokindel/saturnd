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
    "\t-p PIPES_DIR -> look for the pipes (or creates them if not existing) in PIPES_DIR (default: /tmp/<USERNAME>/saturnd/pipes)\n";

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
            fatal_assert(g_pipes_path != NULL);
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
    
    fatal_assert(allocate_paths() != -1);
    
    tasks_directory_path = calloc(1, PATH_MAX);
    assert(tasks_directory_path);
    assert(sprintf(tasks_directory_path, "%s../tasks/", g_pipes_path) != -1);
    
    DIR *pipes_dir = opendir(g_pipes_path);
    
    // Creates the pipe's directory (recursively) if it doesn't exist.
    if (!pipes_dir) {
        fatal_assert(errno == ENOENT && mkdir_recursively(g_pipes_path, 0777) != -1);
        pipes_dir = opendir(g_pipes_path);
        fatal_assert(pipes_dir);
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
            fatal_assert(write_uint16(&buf, &opcode) != -1);
            fatal_assert(write_buffer(request_write_fd, &buf) != -1);
            free(buf.data);
            fatal_assert(close(request_write_fd) != -1);
            fatal_error("daemon is already running or pipes are being used by another process\n");
        }
        errno = 0;
    } else {
        // Creates the request pipe file if it doesn't exits.
        fatal_assert(mkfifo(g_request_pipe_path, 0666) != -1);
    }
    
    // Creates the reply pipe file if it doesn't exist.
    fatal_assert(reply_pipe_found || mkfifo(g_reply_pipe_path, 0666) != -1);
    
    fatal_assert(closedir(pipes_dir) != -1);
    
    DIR *tasks_dir = opendir(tasks_directory_path);
    
    // Creates the tasks directory if it doesn't exists.
    if (!tasks_dir) {
        fatal_assert(errno == ENOENT && mkdir_recursively(tasks_directory_path, 0777) != -1);
        tasks_dir = opendir(tasks_directory_path);
        fatal_assert(tasks_dir);
    }
    
    uint64_t *existing_taskids = NULL;
    // Searches for existing tasks.
    while ((entry = readdir(tasks_dir)) != NULL) {
        char* unparsed = NULL;
        uint64_t taskid = strtoull(entry->d_name, &unparsed, 10);
    
        if (errno || (!taskid && entry->d_name == unparsed)) {
            errno = 0;
            continue;
        }
        
        array_push(existing_taskids, taskid);
        
        // Calculate the last taskid used based on directories names.
        if (taskid >= g_last_taskid) {
            g_last_taskid = taskid + 1;
        }
    }
    
    fatal_assert(closedir(tasks_dir) != -1);
    
    // Sort existing tasks.
    if (!array_empty(existing_taskids)) {
        uint64_t k;
        for (uint64_t i = 0; i < array_size(existing_taskids) - 1; i++) {
            k = i;
        
            for (uint64_t j = i + 1; j < array_size(existing_taskids); j++) {
                if (existing_taskids[j] < existing_taskids[k]) {
                    k = j;
                }
            }
        
            uint64_t tmp = existing_taskids[k];
            existing_taskids[k] = existing_taskids[i];
            existing_taskids[i] = tmp;
        }
    }
    
    // Loads any existing task and create its thread.
    for (uint64_t i = 0; i < array_size(existing_taskids); i++) {
        thread_handle thread_handle = { .taskid = existing_taskids[i] };
        fatal_assert(array_push(g_threads, thread_handle) != -1);
        worker *new_worker = NULL;
        fatal_assert(create_worker(&new_worker, NULL, tasks_directory_path, existing_taskids[i]) != -1);
        fatal_assert(array_push(g_workers, new_worker) != -1); // NOLINT
        fatal_assert(array_push(g_running_taskids, existing_taskids[i]) != -1);
        fatal_assert(pthread_create(&(array_last(g_threads).pthread), NULL, worker_main, (void *) array_last(g_workers)) == 0);
    }
    
    array_free(existing_taskids);

#ifdef DAEMONIZE
    // Attempts a double fork to become a daemon.
    pid_t daemon_pid = fork();
    
    fatal_assert(daemon_pid != -1);
    if (daemon_pid != 0) {
        goto cleanup;
    }
    
    daemon_pid = fork();
    
    fatal_assert(daemon_pid != -1);
    if (daemon_pid != 0) {
        goto cleanup;
    }
#endif
    
    log("daemon started.\n");
    
    while (1) {
        // Waits for requests to handle...
        int request_read_fd = open(g_request_pipe_path, O_RDONLY);
        fatal_assert(request_read_fd != -1);
        
        // Reads a request.
        request request;
        fatal_assert(read_uint16(request_read_fd, &request.opcode) != -1);
    
        log2("request received `%s`.\n", request_item_names()[request.opcode]);
        
        if (request.opcode == 0) {
            log("no reply required.\n");
            fatal_assert(close(request_read_fd) != -1);
            continue;
        }
        
        switch (request.opcode) {
        case CLIENT_REQUEST_CREATE_TASK:
            fatal_assert(read_task(request_read_fd, &request.task, 0) != -1);
            break;
        case CLIENT_REQUEST_REMOVE_TASK:
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
        case CLIENT_REQUEST_GET_STDOUT:
        case CLIENT_REQUEST_GET_STDERR:
            fatal_assert(read_uint64(request_read_fd, &request.taskid) != -1);
            break;
        default:
            break;
        }
    
        fatal_assert(close(request_read_fd) != -1);
        
        // Writes a reply.
        reply reply;
        switch (request.opcode) {
        case CLIENT_REQUEST_LIST_TASKS: {
            task *tasks = NULL;
    
            for (uint64_t i = 0; i < array_size(g_workers); i++) {
                worker *worker = g_workers[i];
                
                if (worker != NULL) {
                    if (is_worker_running(worker->task.taskid)) {
                        fatal_assert(array_push(tasks, worker->task) != -1);
                    }
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
            fatal_assert(array_push(g_threads, thread_handle) != -1);
            worker *new_worker = NULL;
            fatal_assert(create_worker(&new_worker, &request.task, tasks_directory_path, request.task.taskid) != -1);
            fatal_assert(array_push(g_workers, new_worker) != -1); // NOLINT
            fatal_assert(array_push(g_running_taskids, request.task.taskid) != -1);
            fatal_assert(pthread_create(&(array_last(g_threads).pthread), NULL, worker_main, (void *) array_last(g_workers)) == 0);
    
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
    
            char dir_path[PATH_MAX];
            char *tmp = strcpy(dir_path, get_worker(request.taskid)->dir_path);
            fatal_assert(tmp);
            
            for (uint64_t i = 0; i < array_size(g_workers); i++) {
                if (g_threads[i].taskid == request.taskid) {
                    fatal_assert(pthread_cancel(g_threads[i].pthread) == 0);
                    fatal_assert(pthread_join(g_threads[i].pthread, NULL) == 0);
                    g_workers[i] = NULL;
                    break;
                }
            }
    
            fatal_assert(remove_worker(request.taskid) != -1);
    
            printf("%s", dir_path);
            
            char *task_file_path = calloc(1, PATH_MAX);
            fatal_assert(sprintf(task_file_path, "%stask", dir_path) != -1);
            char *runs_file_path = calloc(1, PATH_MAX);
            fatal_assert(sprintf(runs_file_path, "%sruns", dir_path) != -1);
            char *last_stdout_file_path = calloc(1, PATH_MAX);
            fatal_assert(sprintf(last_stdout_file_path, "%slast_stdout", dir_path) != -1);
            char *last_stderr_file_path = calloc(1, PATH_MAX);
            fatal_assert(sprintf(last_stderr_file_path, "%slast_stderr", dir_path) != -1);
            fatal_assert(unlink(task_file_path) != -1);
            fatal_assert(unlink(runs_file_path) != -1);
            fatal_assert(unlink(last_stdout_file_path) != -1);
            fatal_assert(unlink(last_stderr_file_path) != -1);
            fatal_assert(rmdir(dir_path) != -1);
            free(task_file_path);
            free(runs_file_path);
            free(last_stdout_file_path);
            free(last_stderr_file_path);
            
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
            fatal_assert(task_worker);
            
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
        fatal_assert(reply_write_fd != -1);
        
        if (reply.reptype == SERVER_REPLY_OK) {
            log2("sending to client `%s`.\n", reply_item_names()[reply.reptype]);
        } else {
            log2("sending to client `%s` with error `%s`.\n", reply_item_names()[reply.reptype], reply_error_item_names()[reply.errcode]);
        }
    
        buffer buf = create_buffer();
        fatal_assert(write_uint16(&buf, &reply.reptype) != -1);
        
        if (reply.reptype == SERVER_REPLY_OK) {
            switch (request.opcode) {
            case CLIENT_REQUEST_LIST_TASKS: {
                fatal_assert(write_task_array(&buf, reply.tasks) != -1);
                array_free(reply.tasks);
                break;
            }
            case CLIENT_REQUEST_CREATE_TASK:
                fatal_assert(write_uint64(&buf, &reply.taskid) != -1);
                break;
            case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES: {
                fatal_assert(write_run_array(&buf, reply.runs) != -1);
                break;
            }
            case CLIENT_REQUEST_GET_STDOUT:
            case CLIENT_REQUEST_GET_STDERR:
                fatal_assert(write_string(&buf, &reply.output) != -1);
                break;
            default:
                break;
            }
        } else {
            fatal_assert(write_uint16(&buf, &reply.errcode) != -1);
        }
    
        fatal_assert(write_buffer(reply_write_fd, &buf) != -1);
        free(buf.data);
        fatal_assert(close(reply_write_fd) != -1);
        
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
    array_free(g_threads);
    array_free(g_workers);
    free(tasks_directory_path);
    cleanup_paths();
    
    return exit_code;
}