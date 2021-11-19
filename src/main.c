#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sy5/utils.h>
#include <sy5/reply.h>
#include <sy5/request.h>
#include <sy5/timing.h>
#include <sy5/string.h>
#include <sy5/commandline.h>
#include <sy5/endian.h>
#include <time.h>
#ifdef __linux__
#include <unistd.h>
#endif

#define DEFAULT_PIPES_DIR "/tmp/<USERNAME>/saturnd/pipes"
#define REQUEST_PIPE_NAME "saturnd-request-pipe"
#define REPLY_PIPE_NAME "saturnd-reply-pipe"
#ifdef CASSINI
#define EXECUTABLE_NAME "cassini"
#else
#define EXECUTABLE_NAME "saturnd"
#endif

#define perror_custom(err) fprintf(stderr, EXECUTABLE_NAME ": " err)

// TODO: SATURND
/*#ifdef SATURND
#define MAX_TASKS 256
sy5_task tasks[MAX_TASKS];
#endif*/

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

int main(int argc, char *argv[]) {
    errno = 0;
    int err = EXIT_SUCCESS;
    char *pipes_directory = NULL;
    char *request_pipe_path =  NULL;
    char *reply_pipe_path = NULL;
    int had_illegal_option = 0;
#ifdef CASSINI
    char *minutes_str = "*";
    char *hours_str = "*";
    char *daysofweek_str = "*";
    uint16_t opcode = 0;
    uint64_t taskid;
    char *strtoull_endp;
#endif
    
    // Parse options.
    int opt;
    while ((opt = getopt(argc, argv, "hlcqm:H:d:p:r:x:o:e:")) != -1) {
        switch (opt) {
        case 'p':
            pipes_directory = strdup(optarg);
            if (pipes_directory == NULL) {
                goto error_with_perror;
            }
            break;
        case 'h':
            printf("%s", usage_info);
            return 0;
#ifdef CASSINI
        case 'm':
            minutes_str = optarg;
            break;
        case 'H':
            hours_str = optarg;
            break;
        case 'd':
            daysofweek_str = optarg;
            break;
        case 'l':
            opcode = CLIENT_REQUEST_LIST_TASKS;
            break;
        case 'c':
            opcode = CLIENT_REQUEST_CREATE_TASK;
            break;
        case 'q':
            opcode = CLIENT_REQUEST_TERMINATE;
            break;
        case 'r':
            opcode = CLIENT_REQUEST_REMOVE_TASK;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error_with_perror;
            }
            break;
        case 'x':
            opcode = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error_with_perror;
            }
            break;
        case 'o':
            opcode = CLIENT_REQUEST_GET_STDOUT;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error_with_perror;
            }
            break;
        case 'e':
            opcode = CLIENT_REQUEST_GET_STDERR;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error_with_perror;
            }
            break;
#endif
        case '?':
            had_illegal_option = 1;
            break;
        default:
            fprintf(stderr, "unimplemented option: %s\n", optarg);
        }
    }
    
    if (had_illegal_option) {
        fprintf(stderr, EXECUTABLE_NAME ": use `-h` for more informations\n");
    }

#ifdef CASSINI
    if (opcode == 0) {
        fprintf(stderr, EXECUTABLE_NAME ": you need to specify an opcode\n");
        goto error;
    }
#endif
    // Gets the path for each pipe.
    if (pipes_directory == NULL) {
        pipes_directory = calloc(1, PATH_MAX);
        
        if (pipes_directory == NULL) {
            goto error_with_perror;
        }
        
        if (sprintf(pipes_directory, "/tmp/%s/saturnd/pipes/", getlogin()) == -1) {
            pipes_directory = NULL;
            goto error_with_perror;
        }
    }
    
    request_pipe_path = calloc(1, PATH_MAX);
    
    if (request_pipe_path == NULL) {
        goto error_with_perror;
    }
    
    if (sprintf(request_pipe_path, "%s%s%s", pipes_directory, request_pipe_path[strlen(request_pipe_path) - 1] == '/' ? "" : "/", REQUEST_PIPE_NAME) == -1) {
        request_pipe_path = NULL;
        goto error_with_perror;
    }
    
    reply_pipe_path = calloc(1, PATH_MAX);
    
    if (reply_pipe_path == NULL) {
        goto error_with_perror;
    }
    
    if (sprintf(reply_pipe_path, "%s%s%s", pipes_directory, request_pipe_path[strlen(request_pipe_path) - 1] == '/' ? "" : "/", REPLY_PIPE_NAME) == -1) {
        reply_pipe_path = NULL;
        goto error_with_perror;
    }

#ifdef CASSINI
    int request_write_fd = open(request_pipe_path, O_WRONLY | O_NONBLOCK);
    if (request_write_fd == -1) {
        perror_custom("daemon is not running or pipes cannot be reached\n");
        goto error_with_perror;
    }
    
    syslog(LOG_NOTICE, "sending to daemon `%s`\n", request_item_names()[opcode]);
    int be_opcode = htobe16(opcode);
    if (write(request_write_fd, &be_opcode, sizeof(uint16_t)) == -1) {
        goto error_with_perror;
    }
    switch (opcode) {
    case CLIENT_REQUEST_CREATE_TASK: {
        sy5_timing timing;
        if (timing_from_strings(&timing, minutes_str, hours_str, daysofweek_str) == -1) {
            goto error_with_perror;
        }
        if (write_timing(request_write_fd, &timing) == -1) {
            goto error_with_perror;
        }
        sy5_commandline commandline;
        commandline_from_args(&commandline, argc - optind, argv + optind);
        if (write_commandline(request_write_fd, &commandline) == -1) {
            goto error_with_perror;
        }
        break;
    }
    default:
        break;
    }
    close(request_write_fd);
    
    int reply_read_fd = open(reply_pipe_path, O_RDONLY);
    if (reply_read_fd == -1) {
        goto error_with_perror;
    }
    
    sy5_reply reply;
    if (read(reply_read_fd, &reply, sizeof(sy5_reply)) == 1) {
        goto error_with_perror;
    }
    close(reply_read_fd);
    if (reply.reptype == SERVER_REPLY_OK) {
        /*switch (opcode) {
        case CLIENT_REQUEST_LIST_TASKS:
            for (int i = 0; i < reply.nbtasks; i++) {
                char task_str[MAX_TIMING_STRING_LENGTH];
                timing_string_from_timing(task_str, &reply.tasks[i].timing);
                printf("%llu: %s", reply.tasks[i].taskid, task_str);
                for (int j = 0; j < reply.tasks[i].commandline.argc; j++) {
                    char argv_str[MAX_TIMING_STRING_LENGTH];
                    cstring_from_string(argv_str, reply.tasks[i].commandline.argv[j]);
                    printf(" %s", argv_str);
                }
                printf("\n");
            }
            break;
        case CLIENT_REQUEST_CREATE_TASK:
            printf("%llu", reply.taskid);
            break;
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
            for (int i = 0; i < reply.nbruns; i++) {
                time_t timestamp = reply.run[i].time;
                struct tm* time_info = localtime(&timestamp);
                char time_str[26];
                strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", time_info);
                printf("%s %d", time_str, reply.run[i].exitcode);
            }
            break;
        case CLIENT_REQUEST_GET_STDOUT:
        case CLIENT_REQUEST_GET_STDERR: {
            char output_str[MAX_TIMING_STRING_LENGTH];
            cstring_from_string(output_str, reply.output);
            printf("%s", output_str);
            break;
        }
        default:
            break;
        }*/
        
        syslog(LOG_NOTICE, "reply received `%s`\n", reply_item_names()[reply.reptype]);
    } else {
        syslog(LOG_NOTICE, "reply received `%s` with error `%s`\n", reply_item_names()[reply.reptype], reply_error_item_names()[reply.errcode]);
    }
#else
    DIR* dir = opendir(pipes_directory);
    
    // Creates the pipes' directory (recursively) if it doesn't exist.
    if (!dir) {
        if (ENOENT != errno || mkdir_recursively(pipes_directory, 0777) == -1) {
            goto error_with_perror;
        }
        
        dir = opendir(pipes_directory);
        
        if (!dir) {
            goto error_with_perror;
        }
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
            sy5_request request = {
                .opcode = 0
            };
            write(request_write_fd, &request, sizeof(sy5_request));
            close(request_write_fd);
            perror_custom("daemon is already running or pipes are being used by another process\n");
            goto error;
        }
    // Creates the request pipe file if it doesn't exits.
    } else if (mkfifo(request_pipe_path, 0666) == -1) {
        goto error_with_perror;
    }
    
    // Creates the reply pipe file if it doesn't exist.
    if (!reply_pipe_found && mkfifo(reply_pipe_path, 0666) == -1) {
        goto error_with_perror;
    }
    
    closedir(dir);

    // Attempts a double fork to become a daemon.
    pid_t pid = fork();
    
    if (pid == -1) {
        goto error_with_perror;
    } else if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    pid = fork();
    
    if (pid == -1) {
        goto error_with_perror;
    } else if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    syslog(LOG_NOTICE, "daemon started\n");
    
    // Waiting for requests to handle...
    while (1) {
        int request_read_fd = open(request_pipe_path, O_RDONLY);
        if (request_read_fd == -1) {
            goto error_with_perror;
        }
    
        sy5_request request;
        if (read(request_read_fd, &request, sizeof(sy5_request)) == 1) {
            goto error_with_perror;
        }
        close(request_read_fd);
    
        syslog(LOG_NOTICE, "request received `%s`\n", request_item_names()[request.opcode]);
    
        if (request.opcode == 0) {
            syslog(LOG_NOTICE, "no reply required\n");
            continue;
        }
        
        int reply_write_fd = open(reply_pipe_path, O_WRONLY);
        if (reply_write_fd == -1) {
            goto error_with_perror;
        }
    
        int should_terminate = 0;
        sy5_reply reply;
        switch (request.opcode) {
        case CLIENT_REQUEST_CREATE_TASK:
            reply.reptype = SERVER_REPLY_OK;
            syslog(LOG_NOTICE, "with %d arguments", request.commandline.argc);
            for (int i = 0; i < request.commandline.argc; i++) {
                char argv[MAX_STRING_LENGTH];
                cstring_from_string(argv, request.commandline.argv[i]);
                syslog(LOG_NOTICE, "- %s", argv);
            }
            break;
        case CLIENT_REQUEST_TERMINATE:
            should_terminate = 1;
            reply.reptype = SERVER_REPLY_OK;
            break;
        default:
            syslog(LOG_ERR, "unimplemented request: %x\n", request.opcode);
            reply.reptype = SERVER_REPLY_ERROR;
        }
    
        if (reply.reptype == SERVER_REPLY_ERROR) {
            syslog(LOG_NOTICE, "sending to client `%s` with error `%s`\n", reply_item_names()[reply.reptype], reply_error_item_names()[reply.errcode]);
        } else {
            syslog(LOG_NOTICE, "sending to client `%s`\n", reply_item_names()[reply.reptype]);
        }
        write(reply_write_fd, &reply, sizeof(sy5_reply));
        close(reply_write_fd);
        
        if (should_terminate) {
            break;
        }
    }
#endif
    
    goto cleanup;
    
error_with_perror:
    if (errno != 0) {
        perror(EXECUTABLE_NAME);
    }
    
error:
    err = EXIT_FAILURE;
    
cleanup:
    if (pipes_directory != NULL) {
        free(pipes_directory);
        pipes_directory = NULL;
    }
    
    if (request_pipe_path != NULL) {
        free(request_pipe_path);
        request_pipe_path = NULL;
    }
    
    if (reply_pipe_path != NULL) {
        free(reply_pipe_path);
        reply_pipe_path = NULL;
    }
    
    return err;
}