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
#include <sy5/common.h>
#include <pthread.h>
#ifdef __linux__
#include <unistd.h>
#endif

typedef struct worker {
    task task;
    pthread_t thread;
    run *runs;
    string last_stdout;
    string last_stderr;
} worker;

static const char g_help[] =
    "usage: saturnd [OPTIONS]\n"
    "\n"
    "options:\n"
    "\t-p PIPES_DIR -> look for the pipes (or creates them if not existing) in PIPES_DIR (default: " DEFAULT_PIPES_DIR ")\n";

static worker **g_workers = NULL;
static uint64_t *g_running_taskids = NULL;
static uint64_t g_taskid = 0;

static int create_worker(worker **dest, task task) {
    worker *tmp = malloc(sizeof(worker));
    assert(tmp);
    
    tmp->task = task;
    tmp->thread = 0;
    tmp->runs = NULL;
    tmp->last_stdout.length = 0;
    tmp->last_stdout.data = NULL;
    tmp->last_stderr.length = 0;
    tmp->last_stderr.data = NULL;
    
    *dest = tmp;
    
    return 0;
}

static int free_worker(worker **worker) {
    free_task(&(*worker)->task);
    array_free((*worker)->runs);
    free_string(&(*worker)->last_stdout);
    free_string(&(*worker)->last_stderr);
    free(*worker);
    *worker = NULL;
    
    return 0;
}

static bool is_task_alive(uint64_t taskid) {
    bool alive = false;
    
    for (uint64_t i = 0; i < array_size(g_running_taskids); i++) {
        if (g_running_taskids[i] == taskid) {
            alive = true;
            break;
        }
    }
    
    return alive;
}

static int remove_task(uint64_t taskid) {
    for (uint64_t i = 0; i < array_size(g_running_taskids); i++) {
        if (g_running_taskids[i] == taskid) {
            array_remove(g_running_taskids, i);
            break;
        }
    }
    
    return 0;
}

static worker *get_worker(uint64_t taskid) {
    for (uint64_t i = 0; i < array_size(g_workers); i++) {
        if (g_workers[i]->task.taskid == taskid) {
            return g_workers[i];
        }
    }
    
    return NULL;
}

static void *worker_thread(void *worker_arg) {
    worker *worker_to_handle = (worker *)worker_arg;
    timing timing = worker_to_handle->task.timing;
    
    uint64_t execution_time;
    time_t timestamp;
    struct tm *time_info;
    
    while (true) {
        execution_time = time(NULL);
        timestamp = (time_t)execution_time;
        time_info = localtime(&timestamp);
        
        if (((timing.daysofweek >> time_info->tm_wday) % 2 == 0) ||
            ((timing.hours >> time_info->tm_hour) % 2 == 0) ||
            ((timing.minutes >> time_info->tm_min) % 2 == 0)) {
            usleep(1000000 * (60 - time_info->tm_sec));
            continue;
        }
        
        if (!is_task_alive(worker_to_handle->task.taskid)) {
            goto cleanup;
        }
        
        int stdout_pipe[2];
        if (pipe(stdout_pipe) == -1) {
            log("cannot create stdout pipe!\n");
            goto cleanup;
        }
        
        int stderr_pipe[2];
        if (pipe(stderr_pipe) == -1) {
            log("cannot create stderr pipe!\n");
            goto cleanup;
        }
        
        pid_t fork_pid = fork();
        
        if (fork_pid == -1) {
            log("cannot create task fork!\n");
            goto cleanup;
        } else if (fork_pid == 0) {
            if (close(stdout_pipe[0]) == -1) {
                log("cannot close stdout pipe!\n");
                goto cleanup;
            }
            
            if (close(stderr_pipe[0]) == -1) {
                log("cannot close stdout pipe!\n");
                goto cleanup;
            }
            
            if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
                log("cannot duplicate stdout!\n");
                goto cleanup;
            }
            
            if (close(stdout_pipe[1]) == -1) {
                log("cannot close stdout pipe!\n");
                goto cleanup;
            }
            
            if (dup2(stderr_pipe[1], STDERR_FILENO) == -1) {
                log("cannot duplicate stdout!\n");
                goto cleanup;
            }
            
            if (close(stderr_pipe[1]) == -1) {
                log("cannot close stderr pipe!\n");
                goto cleanup;
            }
            
            char **argv = NULL;
            
            for (uint32_t i = 0; i < worker_to_handle->task.commandline.argc; i++) {
                char *arg = NULL;
                if (cstring_from_string(&arg, worker_to_handle->task.commandline.argv) == -1) {
                    log("cannot convert cstring from string!\n");
                    goto cleanup;
                }
                
                if (array_push(argv, arg) == -1) {
                    log("cannot push arg to argv!\n");
                    goto cleanup;
                }
            }
            
            char *null_arg = NULL;
            if (array_push(argv, null_arg) == -1) {
                log("cannot push arg to argv!\n");
                goto cleanup;
            }
            
            execvp(argv[0], argv);
            perror("execve");
            exit(EXIT_FAILURE);
        }
        
        if (close(stdout_pipe[1]) == -1) {
            log("cannot close stdout pipe!\n");
            goto cleanup;
        }
        
        if (close(stderr_pipe[1]) == -1) {
            log("cannot close stderr pipe!\n");
            goto cleanup;
        }
        
        char buf[PIPE_BUF] = { 0 };
        
        char *stdout_buf = NULL;
        while (read(stdout_pipe[0], buf, sizeof(buf)) != 0) {
            for (uint32_t i = 0; i < PIPE_BUF; i++) {
                array_push(stdout_buf, buf[i]);
                
                if (buf[i] == 0) {
                    break;
                }
            }
            
            memset (buf, 0, sizeof(buf));
        }
        
        char *stderr_buf = NULL;
        while (read(stderr_pipe[0], buf, sizeof(buf)) != 0) {
            for (uint32_t i = 0; i < PIPE_BUF; i++) {
                array_push(stderr_buf, buf[i]);
                
                if (buf[i] == 0) {
                    break;
                }
            }
            
            char null_char = 0;
            array_push(stderr_buf, null_char);
            
            memset (buf, 0, sizeof(buf));
        }
        
        if (close(stdout_pipe[0]) == -1) {
            log("cannot close stdout pipe!\n");
            goto cleanup;
        }
        
        if (close(stderr_pipe[0]) == -1) {
            log("cannot close stderr pipe!\n");
            goto cleanup;
        }
        
        int status;
        if (waitpid(fork_pid, &status, 0) == -1) {
            log("cannot waitpid()!\n");
            goto cleanup;
        }
        
        if (stdout_buf != NULL) {
            string_from_cstring(&worker_to_handle->last_stdout, stdout_buf);
        }
        
        if (stderr_buf != NULL) {
            string_from_cstring(&worker_to_handle->last_stderr, stderr_buf);
        }
        
        array_free(stdout_buf);
        array_free(stderr_buf);
        
        run cur_run = {
            .exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : 0xFFFF,
            .time = execution_time
        };
        
        array_push(worker_to_handle->runs, cur_run);
    
        execution_time = time(NULL);
        timestamp = (time_t)execution_time;
        time_info = localtime(&timestamp);
        usleep(1000000 * (60 - time_info->tm_sec));
    }
    
    cleanup:
    free_worker(&worker_to_handle);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    errno = 0;
    
    int exit_code = EXIT_SUCCESS;
    bool used_unexisting_option = false;
    char *tasks_directory_path = NULL;
    
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
    
    fatal_assert(allocate_paths() != -1, "cannot define pipes!\n");
    
    tasks_directory_path = calloc(1, PATH_MAX);
    assert(tasks_directory_path);
    assert(sprintf(tasks_directory_path, "%s../tasks/", pipes_directory_path) != -1);
    
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
            free(buf.data);
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
        case CLIENT_REQUEST_LIST_TASKS: {
            task *tasks = NULL;
    
            for (uint64_t i = 0; i < array_size(g_workers); i++) {
                worker *worker = g_workers[i];
                
                if (is_task_alive(worker->task.taskid)) {
                    array_push(tasks, worker->task);
                }
            }
    
            reply.tasks = tasks;
            reply.reptype = SERVER_REPLY_OK;
            
            break;
        }
        case CLIENT_REQUEST_CREATE_TASK: {
            request.task.taskid = g_taskid++;
            
            worker *new_worker = NULL;
            fatal_assert(create_worker(&new_worker, request.task) != -1, "cannot create worker!\n");
            fatal_assert(array_push(g_workers, new_worker) != -1, "cannot push to `g_workers`!\n");
            fatal_assert(array_push(g_running_taskids, request.task.taskid) != -1, "cannot push to `g_running_taskids`!\n");
            fatal_assert(pthread_create(&(array_last(g_workers)->thread), NULL, worker_thread, (void *) array_last(g_workers)) == 0, "cannot create task thread!\n");
    
            reply.taskid = request.task.taskid;
            reply.reptype = SERVER_REPLY_OK;
            
            break;
        }
        case CLIENT_REQUEST_REMOVE_TASK: {
            if (!is_task_alive(request.taskid)) {
                reply.reptype = SERVER_REPLY_ERROR;
                reply.errcode = SERVER_REPLY_ERROR_NOT_FOUND;
                break;
            }
    
            fatal_assert(remove_task(request.taskid) != -1, "cannot remove task!\n");
            
            reply.reptype = SERVER_REPLY_OK;
            
            break;
        }
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES: {
            if (!is_task_alive(request.taskid)) {
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
            if (!is_task_alive(request.taskid)) {
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
    for (uint64_t i = 0; i < array_size(g_workers); i++) {
        pthread_join(g_workers[i]->thread, NULL);
    }
    array_free(g_workers);
    free(tasks_directory_path);
    cleanup_paths();
    
    return exit_code;
}