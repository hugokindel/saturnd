#include "cassini.h"
#include <syslog.h>

const char usage_info[] =
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
    "\tor: cassini -h -> display this message\n";

int main(int argc, char *argv[]) {
    errno = 0;
    char *minutes_str = "*";
    char *hours_str = "*";
    char *daysofweek_str = "*";
    char *pipes_directory = NULL;
    uint16_t operation = CLIENT_REQUEST_LIST_TASKS;
    uint64_t taskid;
    
    int opt;
    char *strtoull_endp;
    while ((opt = getopt(argc, argv, "hlcqm:H:d:p:r:x:o:e:")) != -1) {
        switch (opt) {
        case 'm':
            minutes_str = optarg;
            break;
        case 'H':
            hours_str = optarg;
            break;
        case 'd':
            daysofweek_str = optarg;
            break;
        case 'p':
            pipes_directory = strdup(optarg);
            if (pipes_directory == NULL) goto error;
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
        case 'h':
            printf("%s", usage_info);
            return 0;
        case '?':
            fprintf(stderr, "%s", usage_info);
            goto error;
        }
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        goto error;
    } else if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    if(setsid() == -1) {
        goto error;
    }
    
    pid = fork();
    
    if (pid == -1) {
        goto error;
    } else if (pid != 0) {
        exit(EXIT_SUCCESS);
    }
    
    syslog(LOG_NOTICE, "saturnd daemon started");
    
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
