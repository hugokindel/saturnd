#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <sy5/utils.h>
#include <sy5/reply.h>
#include <sy5/request.h>
#include <sy5/common.h>
#include "sy5/array.h"
#ifdef __linux__
#include <unistd.h>
#endif

static const char g_help[] =
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
    "\tor: cassini [OPTIONS] -e TASKID -> get the standard error of the last run of a task\n"
    "\tor: cassini -h -> display this message\n"
    "\n"
    "options:\n"
    "\t-p PIPES_DIR -> look for the pipes in PIPES_DIR (default: " DEFAULT_PIPES_DIR ")\n";

int main(int argc, char *argv[]) {
    errno = 0;
    
    int exit_code = EXIT_SUCCESS;
    int used_unexisting_option = 0;
    char *opt_minutes = "*";
    char *opt_hours = "*";
    char *opt_daysofweek = "*";
    uint16_t opt_opcode = 0;
    uint64_t opt_taskid = 0;
    char *strtoull_endp = NULL;
    
    // Parse options.
    int opt;
    while ((opt = getopt(argc, argv, "hp:lcqm:H:d:r:x:o:e:")) != -1) {
        switch (opt) {
        case 'h':
            printf("%s", g_help);
            return exit_code;
        case 'p':
            g_pipes_path = strdup(optarg);
            fatal_assert(g_pipes_path != NULL, "invalid `g_pipes_path`!\n");
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
        case 'm':
            opt_minutes = optarg;
            break;
        case 'H':
            opt_hours = optarg;
            break;
        case 'd':
            opt_daysofweek = optarg;
            break;
        case 'r':
            opt_opcode = CLIENT_REQUEST_REMOVE_TASK;
            opt_taskid = strtoull(optarg, &strtoull_endp, 10);
            fatal_assert(strtoull_endp != optarg && strtoull_endp[0] == '\0', "invalid taskid!\n");
            break;
        case 'x':
            opt_opcode = CLIENT_REQUEST_GET_TIMES_AND_EXITCODES;
            opt_taskid = strtoull(optarg, &strtoull_endp, 10);
            fatal_assert(strtoull_endp != optarg && strtoull_endp[0] == '\0', "invalid taskid!\n");
            break;
        case 'o':
            opt_opcode = CLIENT_REQUEST_GET_STDOUT;
            opt_taskid = strtoull(optarg, &strtoull_endp, 10);
            fatal_assert(strtoull_endp != optarg && strtoull_endp[0] == '\0', "invalid taskid!\n");
            break;
        case 'e':
            opt_opcode = CLIENT_REQUEST_GET_STDERR;
            opt_taskid = strtoull(optarg, &strtoull_endp, 10);
            fatal_assert(strtoull_endp != optarg && strtoull_endp[0] == '\0', "invalid taskid!\n");
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
    
    if (opt_opcode == 0) {
        opt_opcode = CLIENT_REQUEST_LIST_TASKS;
    }
    
    fatal_assert(allocate_paths() != -1, "cannot define pipes!\n");
    
    int request_write_fd;
    int connection_attempts = 0;
    do {
        // Open the request pipe in writing.
        request_write_fd = open(g_request_pipe_path, O_WRONLY | O_NONBLOCK);
        
        if (request_write_fd == -1) {
            if (connection_attempts == 10) {
                fatal_error("cannot open request pipe within 100ms, timing out.\n");
            }
            
            log("cannot open request pipe, waiting 10ms...\n");
            connection_attempts++;
            usleep(10000);
        } else {
            break;
        }
    } while (1);
    
    log2("sending to daemon `%s`.\n", request_item_names()[opt_opcode]);
    
    // Writes a request.
    buffer buf = create_buffer();
    fatal_assert(write_uint16(&buf, &opt_opcode) != -1, "cannot write `opcode` to request!\n");
    
    switch (opt_opcode) {
    case CLIENT_REQUEST_CREATE_TASK: {
        task task;
        fatal_assert(timing_from_strings(&task.timing, opt_minutes, opt_hours, opt_daysofweek) != -1, "cannot parse `timing`!\n");
        fatal_assert(commandline_from_args(&task.commandline, argc - optind, argv + optind) != -1, "cannot parse `commandline`!\n");
        fatal_assert(write_task(&buf, &task, 0) != -1, "cannot write `task` to request!\n");
        free_task(&task);
        break;
    }
    case CLIENT_REQUEST_REMOVE_TASK:
    case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES:
    case CLIENT_REQUEST_GET_STDOUT:
    case CLIENT_REQUEST_GET_STDERR: {
        fatal_assert(write_uint64(&buf, &opt_taskid) != -1, "cannot write `taskid` to request!\n");
        break;
    }
    default:
        break;
    }
    
    fatal_assert(write_buffer(request_write_fd, &buf) != -1, "cannot write request!\n");
    free(buf.data);
    fatal_assert(close(request_write_fd) != -1, "cannot close request pipe!\n");
    
    // Waits for a reply...
    int reply_read_fd = open(g_reply_pipe_path, O_RDONLY);
    fatal_assert(reply_read_fd != -1, "cannot open response pipe!\n");
    
    // Reads a reply.
    uint16_t reptype;
    fatal_assert(read_uint16(reply_read_fd, &reptype) != -1, "cannot read `opcode` from response!\n");
    
    if (reptype == SERVER_REPLY_OK) {
        log2("reply received `%s`.\n", reply_item_names()[reptype]);
        
        switch (opt_opcode) {
        case CLIENT_REQUEST_LIST_TASKS: {
            task *tasks = NULL;
            uint32_t nbtasks = read_task_array(reply_read_fd, &tasks);
            fatal_assert(nbtasks != -1, "cannot read `nbtasks` from response!\n");
            for (uint32_t i = 0; i < nbtasks; i++) {
                char timing_str[TIMING_TEXT_MIN_BUFFERSIZE];
                fatal_assert(timing_string_from_timing(timing_str, &tasks[i].timing) != -1, "cannot read `timing` from response!\n");
#ifdef __APPLE__
                printf("%llu: %s", tasks[i].taskid, timing_str);
#else
                printf("%lu: %s", tasks[i].taskid, timing_str);
#endif
                for (uint32_t j = 0; j < tasks[i].commandline.argc; j++) {
                    char *argv_str = NULL;
                    fatal_assert(cstring_from_string(&argv_str, &tasks[i].commandline.argv[j]) != -1, "cannot read `argv` from response!\n");
                    printf(" %s", argv_str);
                    free(argv_str);
                }
                printf("\n");
            }
            array_free(tasks);
            break;
        }
        case CLIENT_REQUEST_CREATE_TASK: {
            uint64_t taskid;
            fatal_assert(read_uint64(reply_read_fd, &taskid) != -1, "cannot read `taskid` from response!\n");
#ifdef __APPLE__
            printf("%llu\n", taskid);
#else
            printf("%lu\n", taskid);
#endif
            break;
        }
        case CLIENT_REQUEST_GET_TIMES_AND_EXITCODES: {
            run *runs = NULL;
            uint32_t nbruns = read_run_array(reply_read_fd, &runs);
            fatal_assert(nbruns != -1, "cannot read `nbruns` from response!\n");
            for (uint32_t i = 0; i < nbruns; i++) {
                time_t timestamp = (time_t)runs[i].time;
                struct tm *time_info = localtime(&timestamp);
                char time_str[26];
                fatal_assert(strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", time_info) != -1, "cannot format `timing` from response!\n");
                printf("%s %d\n", time_str, runs[i].exitcode);
            }
            array_free(runs);
            break;
        }
        case CLIENT_REQUEST_GET_STDOUT:
        case CLIENT_REQUEST_GET_STDERR: {
            string output;
            fatal_assert(read_string(reply_read_fd, &output) != -1, "cannot read `output` from response!\n");
            char *output_str = NULL;
            fatal_assert(cstring_from_string(&output_str, &output) != -1, "cannot parse `output` from response!\n");
            if (output_str != NULL) {
                printf("%s", output_str);
            }
            free(output_str);
            break;
        }
        default:
            break;
        }
    } else {
        uint16_t errcode;
        fatal_assert(read_uint16(reply_read_fd, &errcode) != -1, "cannot read `errcode` from response!\n");
        log2("reply received `%s` with error `%s`.\n", reply_item_names()[reptype], reply_error_item_names()[errcode]);
        goto error;
    }
    
    fatal_assert(close(reply_read_fd) != -1, "cannot close reply pipe!\n");
    
    goto cleanup;
    
    error:
    exit_code = get_error();
    
    cleanup:
    cleanup_paths();
    
    return exit_code;
}