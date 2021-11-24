#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <limits.h>
#include <sys/fcntl.h>
#include <sy5/utils.h>
#include <sy5/reply.h>
#include <sy5/request.h>
#ifdef __linux__
#include <unistd.h>
#endif
#ifdef SATURND
#include <dirent.h>
#include <sys/stat.h>
#endif

#define DEFAULT_PIPES_DIR "/tmp/<USERNAME>/saturnd/pipes"
#define REQUEST_PIPE_NAME "saturnd-request-pipe"
#define REPLY_PIPE_NAME "saturnd-reply-pipe"
#ifdef CASSINI
#define EXECUTABLE_NAME "cassini"
#else
#define EXECUTABLE_NAME "saturnd"
#endif

#define printf_error(err) fprintf(stderr, EXECUTABLE_NAME ": " err); goto error
#define printf_perror(err) fprintf(stderr, EXECUTABLE_NAME ": " err); goto error_with_perror
#define assert_perror(condition) if (!(condition)) { goto error_with_perror; } (void)0

const char usage_info[] =
#ifdef CASSINI
    "usage: cassini [OPTIONS] -l -> list all tasks\n"
    "\tor: cassini [OPTIONS]    -> same\n"
    "\tor: cassini [OPTIONS] -q -> terminate the daemon\n"
    "\tor: cassini [OPTIONS] -c [-m MINUTES] [-H HOURS] [-d DAYSOFWEEK] COMMAND_NAME [ARG_1] ... [ARG_N]\n"
    "\t\t-> add a new task and print its TASKID\n"
    "\t\t\tformat & semantics of the \"timing\" fields defined here:\n"
    "\t\t\thttps://pubs.opengroup.org/onlinepubs/9699919799/utilities/crontab.html\n"
    "\t\t\tdefault value for each field is \"*\"\n"
    "\tor: cassini [OPTIONS] -r TASKID -> remove a task\n"
    "\tor: cassini [OPTIONS] -x TASKID -> get info (time + exit code) on all the past runs of a task\n"
    "\tor: cassini [OPTIONS] -o TASKID -> get the standard output of the last run of a task\n"
    "\tor: cassini [OPTIONS] -e TASKID -> get the standard error\n"
    "\tor: cassini -h -> display this message\n"
#else
    "usage: saturnd [OPTIONS]\n"
#endif
    "\n"
    "options:\n"
#ifdef CASSINI
    "\t-p PIPES_DIR -> look for the pipes in PIPES_DIR (default: " DEFAULT_PIPES_DIR ")\n";
#else
    "\t-p PIPES_DIR -> look for the pipes (or creates them if not existing) in PIPES_DIR (default: " DEFAULT_PIPES_DIR ")\n";
#endif

#ifdef SATURND
uint64_t g_last_taskid;
uint32_t g_nbtasks;
task g_tasks[MAX_TASKS];
uint32_t g_nbruns[MAX_TASKS];
run g_runs[MAX_TASKS][MAX_RUNS_HISTORY];
string g_stdouts[MAX_TASKS];
string g_stderrs[MAX_TASKS];
#endif

int main(int argc, char *argv[]) {
    errno = 0;
    
    int exit_code = EXIT_SUCCESS;
    char *pipes_directory_path = NULL;
    char *request_pipe_path =  NULL;
    char *reply_pipe_path = NULL;
    bool used_unexisting_option = false;
#ifdef CASSINI
    char *opt_minutes = "*";
    char *opt_hours = "*";
    char *opt_daysofweek = "*";
    uint16_t opt_opcode = 0;
    uint64_t opt_taskid = 0;
    char *strtoull_endp = NULL;
#endif
    
    // Parse options.
    int opt;
    while ((opt = getopt(argc, argv, "hlcqm:H:d:p:r:x:o:e:")) != -1) {
        switch (opt) {
        case 'p':
            pipes_directory_path = strdup(optarg);
            assert_perror(pipes_directory_path != NULL);
            break;
        case 'h':
            printf("%s", usage_info);
            return exit_code;
#ifdef CASSINI
        case 'm':
            opt_minutes = optarg;
            break;
        case 'H':
            opt_hours = optarg;
            break;
        case 'd':
            opt_daysofweek = optarg;
            break;
        case 'l':
            opt_opcode = CLIENT_REQUEST_LIST_TASKS;
            break;
        case 'c':
            opt_opcode = CLIENT_REQUEST_CREATE_TASK;
            break;
        case 'q':
            opt_opcode = CLIENT_REQUEST_TERMINATE;
            break;
        case 'r':
            opt_opcode = CLIENT_REQUEST_REMOVE_TASK;
            opt_taskid = strtoull(optarg, &strtoull_endp, 10);
            assert(strtoull_endp != optarg && strtoull_endp[0] == '\0');
            break;
        case 'x':
            opt_opcode = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
            opt_taskid = strtoull(optarg, &strtoull_endp, 10);
            assert(strtoull_endp != optarg && strtoull_endp[0] == '\0');
            break;
        case 'o':
            opt_opcode = CLIENT_REQUEST_GET_STDOUT;
            opt_taskid = strtoull(optarg, &strtoull_endp, 10);
            assert(strtoull_endp != optarg && strtoull_endp[0] == '\0');
            break;
        case 'e':
            opt_opcode = CLIENT_REQUEST_GET_STDERR;
            opt_taskid = strtoull(optarg, &strtoull_endp, 10);
            assert(strtoull_endp != optarg && strtoull_endp[0] == '\0');
            break;
#endif
        case '?':
            used_unexisting_option = true;
            break;
        default:
            fprintf(stderr, "unimplemented option: %s\n", optarg);
        }
    }
    
    if (used_unexisting_option) {
        fprintf(stderr, EXECUTABLE_NAME ": use `-h` for more informations\n");
    }

#ifdef CASSINI
    if (opt_opcode == 0) {
        fprintf(stderr, EXECUTABLE_NAME ": you need to specify an opcode\n");
        goto error;
    }
#endif
    
    // Gets the path for each pipe.
    if (pipes_directory_path == NULL) {
        pipes_directory_path = calloc(1, PATH_MAX);
        assert_perror(pipes_directory_path != NULL);
        
        if (sprintf(pipes_directory_path, "/tmp/%s/saturnd/pipes/", getlogin()) == -1) {
            pipes_directory_path = NULL;
            goto error_with_perror;
        }
    }
    
    request_pipe_path = calloc(1, PATH_MAX);
    assert_perror(request_pipe_path != NULL);
    
    if (sprintf(request_pipe_path, "%s%s%s", pipes_directory_path, request_pipe_path[strlen(request_pipe_path) - 1] == '/' ? "" : "/", REQUEST_PIPE_NAME) == -1) {
        request_pipe_path = NULL;
        goto error_with_perror;
    }
    
    reply_pipe_path = calloc(1, PATH_MAX);
    assert_perror(reply_pipe_path != NULL);
    
    if (sprintf(reply_pipe_path, "%s%s%s", pipes_directory_path, request_pipe_path[strlen(request_pipe_path) - 1] == '/' ? "" : "/", REPLY_PIPE_NAME) == -1) {
        reply_pipe_path = NULL;
        goto error_with_perror;
    }

#ifdef CASSINI
    int request_write_fd;
    int connection_attempts = 0;
    do {
        // Open the request pipe in writing.
        request_write_fd = open(request_pipe_path, O_WRONLY | O_NONBLOCK);

        if (request_write_fd == -1) {
            if (connection_attempts == 10) {
                syslog(LOG_NOTICE, "daemon unavailable within 100ms, timeout reached\n");
                printf_perror("daemon is not running or pipes cannot be reached\n");
            }

            syslog(LOG_NOTICE, "daemon unavailable, waiting 10ms...\n");
            connection_attempts++;
            usleep(10000);
        } else {
            break;
        }
    } while (true);
    
    syslog(LOG_NOTICE, "sending to daemon `%s`\n", request_item_names()[opt_opcode]);
    
    // Writes a request.
    assert_perror(write_uint16(request_write_fd, &opt_opcode) != -1);
    
    switch (opt_opcode) {
    case CLIENT_REQUEST_CREATE_TASK: {
        task task;
        assert_perror(timing_from_strings(&task.timing, opt_minutes, opt_hours, opt_daysofweek) != -1);
        assert_perror(commandline_from_args(&task.commandline, argc - optind, argv + optind) != -1);
        assert_perror(write_task(request_write_fd, &task, false) != -1);
        break;
    }
    case CLIENT_REQUEST_REMOVE_TASK:
    case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
    case CLIENT_REQUEST_GET_STDOUT:
    case CLIENT_REQUEST_GET_STDERR: {
        assert_perror(write_uint64(request_write_fd, &opt_taskid) != -1);
        break;
    }
    default:
        break;
    }
    
    close(request_write_fd);
    
    // Waits for a reply...
    int reply_read_fd = open(reply_pipe_path, O_RDONLY);
    assert_perror(reply_read_fd != -1);
    
    // Reads a reply.
    uint16_t reptype;
    assert_perror(read_uint16(reply_read_fd, &reptype) != -1);
    
    if (reptype == SERVER_REPLY_OK) {
        syslog(LOG_NOTICE, "reply received `%s`\n", reply_item_names()[reptype]);
        
        switch (opt_opcode) {
        case CLIENT_REQUEST_LIST_TASKS: {
            task tasks[MAX_TASKS];
            uint32_t nbtasks = read_task_array(reply_read_fd, tasks, true);
            assert_perror(nbtasks != -1);
            for (uint32_t i = 0; i < nbtasks; i++) {
                char timing_str[MAX_TIMING_STRING_LENGTH];
                assert_perror(timing_string_from_timing(timing_str, &tasks[i].timing) != -1);
#ifdef __APPLE__
                printf("%llu: %s", tasks[i].taskid, timing_str);
#else
                printf("%lu: %s", tasks[i].taskid, timing_str);
#endif
                for (uint32_t j = 0; j < tasks[i].commandline.argc; j++) {
                    char argv_str[MAX_STRING_LENGTH];
                    assert_perror(cstring_from_string(argv_str, &tasks[i].commandline.argv[j]) != -1);
                    printf(" %s", argv_str);
                }
                printf("\n");
            }
            break;
        }
        case CLIENT_REQUEST_CREATE_TASK: {
            uint64_t taskid;
            assert_perror(read_uint64(reply_read_fd, &taskid) != -1);
#ifdef __APPLE__
            printf("%llu\n", taskid);
#else
            printf("%lu\n", taskid);
#endif
            break;
        }
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES: {
            run runs[MAX_RUNS_HISTORY];
            uint32_t nbruns = read_run_array(reply_read_fd, runs);
            assert_perror(nbruns != -1);
            for (uint32_t i = 0; i < nbruns; i++) {
                time_t timestamp = (time_t)runs[i].time;
                struct tm *time_info = localtime(&timestamp);
                char time_str[26];
                assert_perror(strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", time_info) != -1);
                printf("%s %d\n", time_str, runs[i].exitcode);
            }
            break;
        }
        case CLIENT_REQUEST_GET_STDOUT:
        case CLIENT_REQUEST_GET_STDERR: {
            string output;
            assert_perror(read_string(reply_read_fd, &output) != -1);
            char output_str[MAX_STRING_LENGTH];
            assert_perror(cstring_from_string(output_str, &output) != -1);
            printf("%s\n", output_str);
            break;
        }
        default:
            break;
        }
    } else {
        uint16_t errcode;
        assert_perror(read_uint16(reply_read_fd, &errcode) != -1);
        syslog(LOG_NOTICE, "reply received `%s` with error `%s`\n", reply_item_names()[reptype], reply_error_item_names()[errcode]);
        goto error;
    }
    
    close(reply_read_fd);
#else
    DIR *dir = opendir(pipes_directory_path);
    
    // Creates the pipes' directory (recursively) if it doesn't exist.
    if (!dir) {
        assert_perror(errno == ENOENT && mkdir_recursively(pipes_directory_path, 0777) != -1);
        dir = opendir(pipes_directory_path);
        assert_perror(dir);
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
            write_uint16(request_write_fd, &opcode);
            close(request_write_fd);
            printf_error("daemon is already running or pipes are being used by another process\n");
        }
    } else {
        // Creates the request pipe file if it doesn't exits.
        assert_perror(mkfifo(request_pipe_path, 0666) != -1);
    }
    
    // Creates the reply pipe file if it doesn't exist.
    assert_perror(reply_pipe_found || mkfifo(reply_pipe_path, 0666) != -1);
    
    closedir(dir);

    // Attempts a double fork to become a daemon.
    pid_t pid = fork();
    
    assert_perror(pid != -1);
    if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    pid = fork();
    
    assert_perror(pid != -1);
    if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    syslog(LOG_NOTICE, "daemon started\n");
    
    while (true) {
        // Waits for requests to handle...
        int request_read_fd = open(request_pipe_path, O_RDONLY);
        assert_perror(request_read_fd != -1);
    
        // Reads a request.
        request request;
        assert_perror(read_uint16(request_read_fd, &request.opcode) != -1);
    
        syslog(LOG_NOTICE, "request received `%s`\n", request_item_names()[request.opcode]);
    
        if (request.opcode == 0) {
            syslog(LOG_NOTICE, "no reply required\n");
            close(request_read_fd);
            continue;
        }
    
        switch (request.opcode) {
        case CLIENT_REQUEST_CREATE_TASK:
        assert_perror(read_task(request_read_fd, &request.task, false) != -1);
            break;
        case CLIENT_REQUEST_REMOVE_TASK:
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
        case CLIENT_REQUEST_GET_STDOUT:
        case CLIENT_REQUEST_GET_STDERR:
            assert_perror(read_uint64(request_read_fd, &request.taskid) != -1);
            break;
        default:
            break;
        }
        
        close(request_read_fd);
    
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
        assert_perror(reply_write_fd != -1);
    
        if (reply.reptype == SERVER_REPLY_OK) {
            syslog(LOG_NOTICE, "sending to client `%s`\n", reply_item_names()[reply.reptype]);
        } else {
            syslog(LOG_NOTICE, "sending to client `%s` with error `%s`\n", reply_item_names()[reply.reptype], reply_error_item_names()[reply.errcode]);
        }
        
        write_uint16(reply_write_fd, &reply.reptype);
        
        if (reply.reptype == SERVER_REPLY_OK) {
            switch (request.opcode) {
            case CLIENT_REQUEST_LIST_TASKS:
                assert_perror(write_task_array(reply_write_fd, &g_nbtasks, g_tasks, true) != -1);
                break;
            case CLIENT_REQUEST_CREATE_TASK:
                assert_perror(write_uint64(reply_write_fd, &g_last_taskid) != -1);
                break;
            case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
                assert_perror(write_run_array(reply_write_fd, &g_nbruns[request.taskid], g_runs[request.taskid]) != -1);
                break;
            case CLIENT_REQUEST_GET_STDOUT:
                assert_perror(write_string(reply_write_fd, &g_stdouts[request.taskid]) != -1);
                break;
            case CLIENT_REQUEST_GET_STDERR:
                assert_perror(write_string(reply_write_fd, &g_stderrs[request.taskid]) != -1);
                break;
            default:
                break;
            }
        } else {
            write_uint16(reply_write_fd, &reply.errcode);
        }
    
        close(reply_write_fd);
        
        if (request.opcode == CLIENT_REQUEST_TERMINATE) {
            break;
        }
    }
    
    syslog(LOG_NOTICE, "daemon shutting down...");
#endif
    
    goto cleanup;
    
error_with_perror:
    if (errno != 0) {
        perror(EXECUTABLE_NAME);
    }
    
error:
    exit_code = EXIT_FAILURE;
    
cleanup:
    if (pipes_directory_path != NULL) {
        free(pipes_directory_path);
        pipes_directory_path = NULL;
    }
    
    if (request_pipe_path != NULL) {
        free(request_pipe_path);
        request_pipe_path = NULL;
    }
    
    if (reply_pipe_path != NULL) {
        free(reply_pipe_path);
        reply_pipe_path = NULL;
    }
    
    return exit_code;
}