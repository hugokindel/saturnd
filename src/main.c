#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdio.h>
#include <errno.h>
#include <syslog.h>
#include <limits.h>
#include <sy5/utils.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/fcntl.h>
#include <sy5/client-request.h>

#define DEFAULT_PIPES_DIR "/tmp/<USERNAME>/saturnd/pipes"
#define REQUEST_PIPE_NAME "saturnd-request-pipe"
#define REPLY_PIPE_NAME "saturnd-reply-pipe"
#define MAX_MESSAGE_LENGTH 4096 // TODO: Which limit to impose ?

#define print_error(err) fprintf(stderr, "main: " err)

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
    int err = 0;
    char *pipes_directory = NULL;

#ifdef CASSINI
    char *minutes_str = "*";
    char *hours_str = "*";
    char *daysofweek_str = "*";
    uint16_t operation = CLIENT_REQUEST_LIST_TASKS;
    uint64_t taskid;
    char *strtoull_endp;
#endif
    
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
        case '?':
            fprintf(stderr, "%s", usage_info);
            return 0;
#ifdef CASSINI
            goto error_with_perror;
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
            operation = CLIENT_REQUEST_LIST_TASKS;
            break;
        case 'c':
            operation = CLIENT_REQUEST_CREATE_TASK;
            break;
        case 'q':
            operation = CLIENT_REQUEST_TERMINATE;
            break;
        case 'r':
            operation = CLIENT_REQUEST_REMOVE_TASK;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error_with_perror;
            }
            break;
        case 'x':
            operation = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error_with_perror;
            }
            break;
        case 'o':
            operation = CLIENT_REQUEST_GET_STDOUT;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error_with_perror;
            }
            break;
        case 'e':
            operation = CLIENT_REQUEST_GET_STDERR;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error_with_perror;
            }
            break;
#endif
        }
    }
    
    if (pipes_directory == NULL) {
        pipes_directory = calloc(1, PATH_MAX);
        if (sprintf(pipes_directory, "/tmp/%s/saturnd/pipes/", getlogin()) == -1) {
            goto error_with_perror;
        }
    }
    
    char *request_pipe_path = calloc(1, PATH_MAX);
    if (sprintf(request_pipe_path, "%s%s", pipes_directory, REQUEST_PIPE_NAME) == -1) {
        goto error_with_perror;
    }
    
    char *reply_pipe_path = calloc(1, PATH_MAX);
    if (sprintf(reply_pipe_path, "%s%s", pipes_directory, REPLY_PIPE_NAME) == -1) {
        goto error_with_perror;
    }

#ifdef CASSINI
    syslog(LOG_NOTICE, "Client started");
    
    int request_write_fd = open(request_pipe_path, O_WRONLY | O_NONBLOCK);
    if (request_write_fd == -1) {
        print_error("Daemon is not running or pipes cannot be reached\n");
        goto error_with_perror;
    }
    
    syslog(LOG_NOTICE, "Sending to daemon: Ping");
    write(request_write_fd, "Ping", sizeof("Ping"));
    close(request_write_fd);
    
    int reply_read_fd = open(reply_pipe_path, O_RDONLY);
    if (reply_read_fd == -1) {
        goto error_with_perror;
    }
    
    char reply_buffer[MAX_MESSAGE_LENGTH];
    if (read(reply_read_fd, reply_buffer, MAX_MESSAGE_LENGTH) == 1) {
        goto error_with_perror;
    }
    close(reply_read_fd);
    syslog(LOG_NOTICE, "Response received: %s", reply_buffer);
#else
    DIR* dir = opendir(pipes_directory);
    
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
    
    if (request_pipe_found) {
        int request_write_fd = open(request_pipe_path, O_WRONLY | O_NONBLOCK);
        if (request_write_fd != -1) {
            write(request_write_fd, "ALIVE", sizeof("ALIVE"));
            close(request_write_fd);
            print_error("Daemon is already running\n");
            goto error;
        }
    } else if (mkfifo(request_pipe_path, 0666) == -1) {
        goto error_with_perror;
    }
    
    if (!reply_pipe_found && mkfifo(reply_pipe_path, 0666) == -1) {
        goto error_with_perror;
    }
    
    closedir(dir);

    pid_t pid = fork();
    
    if (pid == -1) {
        goto error_with_perror;
    } else if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    if (setsid() == -1) {
        goto error_with_perror;
    }
    
    pid = fork();
    
    if (pid == -1) {
        goto error_with_perror;
    } else if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    syslog(LOG_NOTICE, "Daemon started");
    
    while (1) {
        int request_read_fd = open(request_pipe_path, O_RDONLY);
        if (request_read_fd == -1) {
            goto error_with_perror;
        }
    
        char request_buffer[MAX_MESSAGE_LENGTH];
        if (read(request_read_fd, request_buffer, MAX_MESSAGE_LENGTH) == 1) {
            goto error_with_perror;
        }
        close(request_read_fd);
        
        if (strcmp(request_buffer, "ALIVE") == 0) {
            continue;
        }
    
        syslog(LOG_NOTICE, "Request received: %s", request_buffer);
    
        int reply_write_fd = open(reply_pipe_path, O_WRONLY);
        if (reply_write_fd == -1) {
            goto error_with_perror;
        }
    
        syslog(LOG_NOTICE, "Sending to client: Pong");
        write(reply_write_fd, "Pong", sizeof("Pong"));
        close(reply_write_fd);
    }
#endif
    
    goto cleanup;
    
    error_with_perror:
    {
        if (errno != 0) {
            perror("main");
        }
    }
    
    error:
    {
        err = 1;
    }
    
    cleanup:
    {
        free(pipes_directory);
        pipes_directory = NULL;
        free(request_pipe_path);
        request_pipe_path = NULL;
        free(reply_pipe_path);
        reply_pipe_path = NULL;
    }
    
    return err;
}
