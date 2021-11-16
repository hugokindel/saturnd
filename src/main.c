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

#define DEFAULT_PIPES_DIR "/tmp/<USERNAME>/saturnd/pipes"
#define REQUEST_PIPE_NAME "saturnd-request-pipe"
#define REPLY_PIPE_NAME "saturnd-reply-pipe"

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
                goto error;
            }
            break;
        case 'h':
            printf("%s", usage_info);
            return 0;
        case '?':
            fprintf(stderr, "%s", usage_info);
            return 0;
#ifdef CASSINI
            goto error;
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
                goto error;
            }
            break;
        case 'x':
            operation = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error;
            }
            break;
        case 'o':
            operation = CLIENT_REQUEST_GET_STDOUT;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error;
            }
            break;
        case 'e':
            operation = CLIENT_REQUEST_GET_STDERR;
            taskid = strtoull(optarg, &strtoull_endp, 10);
            if (strtoull_endp == optarg || strtoull_endp[0] != '\0') {
                goto error;
            }
            break;
#endif
        }
    }
    
    if (pipes_directory == NULL) {
        pipes_directory = calloc(1, PATH_MAX);
        if (sprintf(pipes_directory, "/tmp/%s/saturnd/pipes/", getlogin()) == -1) {
            goto error;
        }
    }
    
    char *request_pipe_path = calloc(1, PATH_MAX);
    if (sprintf(request_pipe_path, "%s%s", pipes_directory, REQUEST_PIPE_NAME) == -1) {
        goto error;
    }
    
    char *reply_pipe_path = calloc(1, PATH_MAX);
    if (sprintf(reply_pipe_path, "%s%s", pipes_directory, REPLY_PIPE_NAME) == -1) {
        goto error;
    }

#ifdef CASSINI
    // TODO: CASSINI
#else
    DIR* dir = opendir(pipes_directory);
    
    if (!dir) {
        if (ENOENT != errno || mkdir_recursively(pipes_directory, 0777) == -1) {
            goto error;
        }
        
        dir = opendir(pipes_directory);
        
        if (!dir) {
            goto error;
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
    
    if ((!request_pipe_found && mkfifo(request_pipe_path, 0666) == -1) ||
        (!reply_pipe_found && mkfifo(reply_pipe_path, 0666) == -1)) {
        goto error;
    }
    
    closedir(dir);

    pid_t pid = fork();
    
    if (pid == -1) {
        goto error;
    } else if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    if (setsid() == -1) {
        goto error;
    }
    
    pid = fork();
    
    if (pid == -1) {
        goto error;
    } else if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    syslog(LOG_NOTICE, "daemon started");
    
    /*while (1) {
        sleep(5);
        syslog(LOG_NOTICE, "working");
    }*/
#endif
    
    return EXIT_SUCCESS;
    
    error:
    {
        if (errno != 0) {
            perror("main");
        }
        
        free(pipes_directory);
        pipes_directory = NULL;
        
        return EXIT_FAILURE;
    }
}
