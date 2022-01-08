#include <sy5/worker.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <sy5/utils.h>
#include <sy5/array.h>
#include <pthread.h>
#include <sys/time.h>

worker **g_workers = NULL;
uint64_t *g_running_taskids = NULL;

typedef struct worker_cleanup_handle {
    worker *worker;
    pthread_mutex_t *mutex;
} worker_cleanup_handle;

int create_worker(worker **dest, task *task, const char *tasks_path, uint64_t taskid) {
    worker *tmp = malloc(sizeof(worker));
    assert(tmp);
    
    if (task != NULL) {
        tmp->task = *task;
    }
    tmp->runs = NULL;
    tmp->last_stdout.length = 0;
    tmp->last_stdout.data = NULL;
    tmp->last_stderr.length = 0;
    tmp->last_stderr.data = NULL;
    char *task_path = calloc(1, PATH_MAX);
    assert(task_path != NULL);
#ifdef __APPLE__
    assert(sprintf(task_path, "%s%llu/", tasks_path, taskid));
#else
    assert(sprintf(task_path, "%s%lu/", tasks_path, taskid));
#endif
    assert(task_path != NULL);
    tmp->dir_path = task_path;
    
    // Creates the task's directory if it doesn't exist.
    assert(create_directory(tmp->dir_path) != -1);
    
    // Opens (or create) and read (or write) the `task` file.
    assert(open_file(&tmp->task_file_fd, tmp->dir_path, "task", O_RDWR | O_CREAT) != -1);
    off_t pos = lseek(tmp->task_file_fd, 0L, SEEK_END);
    assert(pos != -1 && (pos != 0 || task != NULL));
    assert(lseek(tmp->task_file_fd, 0L, SEEK_SET) != -1);
    if (task != NULL) {
        assert(ftruncate(tmp->task_file_fd, 0) != -1);
        buffer buf = create_buffer();
        assert(write_task(&buf, task, true) != -1);
        assert(write_buffer(tmp->task_file_fd, &buf) != -1);
    } else {
        assert(read_task(tmp->task_file_fd, &tmp->task, true) != -1);
    }
    
    // Opens and read the `runs` file.
    assert(open_file(&tmp->runs_file_fd, tmp->dir_path, "runs", O_RDWR | O_CREAT) != -1);
    pos = lseek(tmp->runs_file_fd, 0L, SEEK_END);
    assert(pos != -1);
    if (pos > 0) {
        assert(lseek(tmp->runs_file_fd, 0L, SEEK_SET) != -1);
        assert(read_run_array(tmp->runs_file_fd, &tmp->runs) != -1);
    }
    
    // Opens and read the `last_stdout` file.
    assert(open_file(&tmp->last_stdout_file_fd, tmp->dir_path, "last_stdout", O_RDWR | O_CREAT) != -1);
    pos = lseek(tmp->last_stdout_file_fd, 0L, SEEK_END);
    assert(pos != -1);
    if (pos > 0) {
        assert(lseek(tmp->last_stdout_file_fd, 0L, SEEK_SET) != -1);
        assert(read_string(tmp->last_stdout_file_fd, &tmp->last_stdout) != -1);
    }
    
    // Opens and read the `last_stderr` file.
    assert(open_file(&tmp->last_stderr_file_fd, tmp->dir_path, "last_stderr", O_RDWR | O_CREAT) != -1);
    pos = lseek(tmp->last_stderr_file_fd, 0L, SEEK_END);
    assert(pos != -1);
    if (pos > 0) {
        assert(lseek(tmp->last_stderr_file_fd, 0L, SEEK_SET) != -1);
        assert(read_string(tmp->last_stderr_file_fd, &tmp->last_stdout) != -1);
    }
    
    *dest = tmp;
    
    return 0;
}

int free_worker(worker **worker) {
    assert(free_task(&(*worker)->task) != -1);
    array_free((*worker)->runs);
    assert(free_string(&(*worker)->last_stdout) != -1);
    assert(free_string(&(*worker)->last_stderr) != -1);
    free((*worker)->dir_path);
    assert(close((*worker)->task_file_fd) != -1);
    assert(close((*worker)->runs_file_fd) != -1);
    assert(close((*worker)->last_stdout_file_fd) != -1);
    assert(close((*worker)->last_stderr_file_fd) != -1);
    free(*worker);
    *worker = NULL;
    
    return 0;
}

bool is_worker_running(uint64_t taskid) {
    bool alive = false;
    
    for (uint64_t i = 0; i < array_size(g_running_taskids); i++) {
        if (g_running_taskids[i] == taskid) {
            alive = true;
            break;
        }
    }
    
    return alive;
}

int remove_worker(uint64_t taskid) {
    for (uint64_t i = 0; i < array_size(g_running_taskids); i++) {
        if (g_running_taskids[i] == taskid) {
            array_remove(g_running_taskids, i);
            break;
        }
    }
    
    return 0;
}

worker *get_worker(uint64_t taskid) {
    for (uint64_t i = 0; i < array_size(g_workers); i++) {
        if (g_workers[i]->task.taskid == taskid) {
            return g_workers[i];
        }
    }
    
    return NULL;
}

void cleanup_worker(void *cleanup_handle_ptr) {
    worker_cleanup_handle *cleanup_handle = cleanup_handle_ptr;
    pthread_mutex_unlock(cleanup_handle->mutex);
    free_worker(&cleanup_handle->worker);
    
    log("worker thread shutting down...\n");
}

void sleep_worker(pthread_mutex_t *lock, pthread_cond_t *cond) {
    uint64_t execution_time = time(NULL);
    time_t timestamp = (time_t)execution_time;
    struct tm *time_info = localtime(&timestamp);
    struct timeval now;
    gettimeofday(&now, NULL);
    struct timespec max_wait = {now.tv_sec + 1 * (60 - time_info->tm_sec), 0};
    
    // Make the thread sleep until the next minute approximativaly.
    // The sleep can be cancelled by a `pthread_cancel` (can happen if a request to remove this task is received or if
    // saturnd is exiting).
    pthread_mutex_lock(lock);
    pthread_cond_timedwait(cond, lock, &max_wait);
    pthread_mutex_unlock(lock);
}

void *worker_main(void *worker_arg) {
    worker *worker_to_handle = (worker *)worker_arg;
    timing timing = worker_to_handle->task.timing;
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    worker_cleanup_handle cleanup_handle = { .worker = worker_to_handle, .mutex = &lock };
    
    // Push the cleanup handler (that will happen once the thread is going to end).
    pthread_cleanup_push(cleanup_worker, &cleanup_handle)
    
    // If there are already any `runs` (meaning we read this task from file), check if it has already been run this
    // exact minute. If it did, sleep until the next minute.
    if (array_size(worker_to_handle->runs) > 0) {
        uint64_t execution_time_last_run = array_last(worker_to_handle->runs).time;
        time_t timestamp_last_run = (time_t)execution_time_last_run;
        struct tm *time_info_last_run = localtime(&timestamp_last_run);
        uint64_t execution_time_now = time(NULL);
        time_t timestamp_now = (time_t)execution_time_now;
        struct tm *time_info_now = localtime(&timestamp_now);
        
        if (time_info_last_run->tm_year == time_info_now->tm_year &&
            time_info_last_run->tm_mon == time_info_now->tm_mon &&
            time_info_last_run->tm_mday == time_info_now->tm_mday &&
            time_info_last_run->tm_hour == time_info_now->tm_hour &&
            time_info_last_run->tm_min == time_info_now->tm_min) {
            sleep_worker(&lock, &cond);
        }
    }
    
    while (true) {
        // Checks if the task has to run this minute, if not, sleep until the next minute.
        uint64_t execution_time = time(NULL);
        time_t timestamp = (time_t)execution_time;
        struct tm *time_info = localtime(&timestamp);
        if (((timing.daysofweek >> time_info->tm_wday) % 2 == 0) ||
            ((timing.hours >> time_info->tm_hour) % 2 == 0) ||
            ((timing.minutes >> time_info->tm_min) % 2 == 0)) {
            sleep_worker(&lock, &cond);
        }
        
        if (!is_worker_running(worker_to_handle->task.taskid)) {
            break;
        }
        
        // Create self-pipes to extract `stdout` and `stderr` from the upcoming `exec` call.
        int stdout_pipe[2];
        int stderr_pipe[2];
        fatal_assert(pipe(stdout_pipe) != -1, "cannot create stdout pipe!\n");
        fatal_assert(pipe(stderr_pipe) != -1, "cannot create stdout pipe!\n");
        
        pid_t fork_pid = fork();
        fatal_assert(fork_pid != -1, "cannot create task fork!\n");
        
        if (fork_pid == 0) {
            fatal_assert(close(stdout_pipe[0]) != -1, "cannot close stdout pipe!\n");
            fatal_assert(close(stderr_pipe[0]) != -1, "cannot close stderr pipe!\n");
            fatal_assert(dup2(stdout_pipe[1], STDOUT_FILENO) != -1, "cannot duplicate stdout!\n");
            fatal_assert(dup2(stderr_pipe[1], STDERR_FILENO) != -1, "cannot duplicate stderr!\n");
            fatal_assert(close(stdout_pipe[1]) != -1, "cannot close stdout pipe!\n");
            fatal_assert(close(stderr_pipe[1]) != -1, "cannot close stderr pipe!\n");
            
            // Creates the `argv` array for the upcoming `exec` call.
            char **argv = NULL;
            for (uint32_t i = 0; i < worker_to_handle->task.commandline.argc; i++) {
                char *arg = NULL;
                fatal_assert(cstring_from_string(&arg, worker_to_handle->task.commandline.argv) != -1, "cannot convert cstring from string!\n");
                fatal_assert(array_push(argv, arg) != -1, "cannot push arg to argv!\n");
            }
            char *null_arg = NULL;
            fatal_assert(array_push(argv, null_arg) != -1, "cannot push arg to argv!\n");
            
            // Execute the command in the fork.
            execvp(argv[0], argv);
            perror("execve");
            exit(EXIT_FAILURE);
        }
        
        fatal_assert(close(stdout_pipe[1]) != -1, "cannot close stdout pipe!\n");
        fatal_assert(close(stderr_pipe[1]) != -1, "cannot close stderr pipe!\n");
        
        char buf[PIPE_BUF] = { 0 };
        
        // Read the last stdout in a buffer.
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
        
        // Read the last stderr in a buffer.
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
    
        fatal_assert(close(stdout_pipe[0]) != -1, "cannot close stdout pipe!\n");
        fatal_assert(close(stderr_pipe[0]) != -1, "cannot close stderr pipe!\n");
        
        int status;
        fatal_assert(waitpid(fork_pid, &status, 0) != -1, "cannot waitpid for fork!\n");
        
        if (stdout_buf != NULL) {
            // Saves the last stdout.
            string_from_cstring(&worker_to_handle->last_stdout, stdout_buf);
            array_free(stdout_buf);
    
            // Writes to the `last_stdout` file.
            fatal_assert(lseek(worker_to_handle->last_stdout_file_fd, 0L, SEEK_SET) != -1, "cannot lseek!\n");
            fatal_assert(ftruncate(worker_to_handle->last_stdout_file_fd, 0) != -1, "cannot truncate!\n");
            buffer wbuf = create_buffer();
            fatal_assert(write_string(&wbuf, &worker_to_handle->last_stdout) != -1, "cannot write string!\n");
            fatal_assert(write_buffer(worker_to_handle->last_stdout_file_fd, &wbuf) != -1, "cannot write buffer!\n");
        }
        
        if (stderr_buf != NULL) {
            // Saves the last stderr.
            string_from_cstring(&worker_to_handle->last_stderr, stderr_buf);
            array_free(stderr_buf);
    
            // Writes to the `last_stderr` file.
            fatal_assert(lseek(worker_to_handle->last_stderr_file_fd, 0L, SEEK_SET) != -1, "cannot lseek!\n");
            fatal_assert(ftruncate(worker_to_handle->last_stderr_file_fd, 0) != -1, "cannot truncate!\n");
            buffer wbuf = create_buffer();
            fatal_assert(write_string(&wbuf, &worker_to_handle->last_stderr) != -1, "cannot write string!\n");
            fatal_assert(write_buffer(worker_to_handle->last_stderr_file_fd, &wbuf) != -1, "cannot write buffer!\n");
        }
        
        // Saves the last run.
        run cur_run = {
            .exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : 0xFFFF,
            .time = execution_time
        };
        array_push(worker_to_handle->runs, cur_run);
    
        // Writes to the `runs` file.
        fatal_assert(lseek(worker_to_handle->runs_file_fd, 0L, SEEK_SET) != -1, "cannot lseek!\n");
        fatal_assert(ftruncate(worker_to_handle->runs_file_fd, 0) != -1, "cannot truncate!\n");
        buffer wbuf = create_buffer();
        fatal_assert(write_run_array(&wbuf, worker_to_handle->runs) != -1, "cannot write string!\n");
        fatal_assert(write_buffer(worker_to_handle->runs_file_fd, &wbuf) != -1, "cannot write buffer!\n");
        
        // Now that the task has been handled, we can sleep until the next minute.
        sleep_worker(&lock, &cond);
    }
    
    goto cleanup;
    
    error:
    log("error in worker thread!\n");
    
    cleanup:
    pthread_cleanup_pop(1)
    
    return NULL;
}