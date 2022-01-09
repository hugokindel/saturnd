#include <sy5/utils.h>
#include <pwd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sy5/array.h>
#include <sy5/common.h>
#ifdef __APPLE__
#include <sys/syslimits.h>
#include <libkern/OSByteOrder.h>
#define htobe16(x) OSSwapHostToBigInt16(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#else
#include <limits.h>
#include <endian.h>
#endif

buffer create_buffer() {
    buffer buffer = {
        .length = 0,
        .data = NULL
    };
    
    return buffer;
}

int allocate_paths() {
    if (g_pipes_path == NULL) {
        g_pipes_path = calloc(1, PATH_MAX);
        assert(g_pipes_path);
        assert(sprintf(g_pipes_path, "/tmp/%s/saturnd/pipes/", getpwuid(getuid())->pw_name) != -1);
    } else if (g_pipes_path[strlen(g_pipes_path) - 1] != '/') {
        char *tmp = calloc(1, PATH_MAX);
        assert(tmp);
        assert(sprintf(tmp, "%s/", g_pipes_path) != -1);
        g_pipes_path = tmp;
    }
    
    g_request_pipe_path = calloc(1, PATH_MAX);
    assert(g_request_pipe_path);
    assert(sprintf(g_request_pipe_path, "%s%s", g_pipes_path, REQUEST_PIPE_NAME) != -1);
    
    g_reply_pipe_path = calloc(1, PATH_MAX);
    assert(g_reply_pipe_path);
    assert(sprintf(g_reply_pipe_path, "%s%s", g_pipes_path, REPLY_PIPE_NAME) != -1);
    
    return 0;
}

void cleanup_paths() {
    if (g_pipes_path != NULL) {
        free(g_pipes_path);
        g_pipes_path = NULL;
    }
    
    if (g_request_pipe_path != NULL) {
        free(g_request_pipe_path);
        g_request_pipe_path = NULL;
    }
    
    if (g_reply_pipe_path != NULL) {
        free(g_reply_pipe_path);
        g_reply_pipe_path = NULL;
    }
}

int get_error() {
    if (errno != 0) {
        perror(EXECUTABLE_NAME);
    }
    
    return EXIT_FAILURE;
}

int create_directory(const char *path) {
    DIR *task_dir = opendir(path);
    
    if (!task_dir) {
        assert(errno == ENOENT && mkdir_recursively(path, 0777) != -1);
        task_dir = opendir(path);
        assert(task_dir);
    }
    
    assert(closedir(task_dir) != -1);
    
    return 0;
}

int open_file(int *dest, const char *path, const char *filename, int oflags) {
    char *final_path = calloc(1, PATH_MAX);
    assert(final_path);
    assert(sprintf(final_path, "%s%s", path, filename) != -1);
    
    int fd = open(final_path, oflags, 0666);
    assert(fd != -1);
    *dest = fd;
    
    free(final_path);
    
    return fd;
}

int mkdir_recursively(const char *path, uint16_t mode) {
    int err = 0;
    const char *pathIterator = path;
    char *directoryName = calloc(1, strlen(path) + 1);
    
    if (directoryName == NULL) {
        return -1;
    }
    
    while ((pathIterator = strchr(pathIterator, '/')) != NULL) {
        long directoryNameLength = pathIterator - path;
        memcpy(directoryName, path, directoryNameLength);
        directoryName[directoryNameLength] = '\0';
        pathIterator++;
        
        if (directoryNameLength == 0) {
            continue;
        }
        
        if (mkdir(directoryName, mode) == -1 && errno != EEXIST) {
            err = -1;
            break;
        }
    }
    
    free(directoryName);
    directoryName = NULL;
    
    return err;
}

int string_from_cstring(string *dest, const char *cstring) {
    dest->length = strlen(cstring);
    
    dest->data = malloc(dest->length);
    assert(dest->data);
    for (uint32_t i = 0; i < dest->length; i++) {
        dest->data[i] = cstring[i];
    }
    
    dest->data[dest->length] = '\0';
    
    return 0;
}

int cstring_from_string(char **dest, const string *string) {
    *dest = malloc(string->length);
    assert(*dest);
    for (uint32_t i = 0; i < string->length; i++) {
        (*dest)[i] = (uint8_t)string->data[i]; // NOLINT
    }
    
    (*dest)[string->length] = '\0';
    
    return 0;
}

int timing_from_strings(timing *dest, const char *minutes_str, const char *hours_str, const char *daysofweek_str) {
    uint64_t field;
    
    assert(timing_field_from_string(&field, minutes_str, 0, 59) > 0);
    dest->minutes = field;
    
    assert(timing_field_from_string(&field, hours_str, 0, 23) > 0);
    dest->hours = (uint32_t)field;
    
    assert(timing_field_from_string(&field, daysofweek_str, 0, 6) > 0);
    dest->daysofweek = (uint8_t)field;
    
    return 0;
}

int timing_string_from_timing(char *dest, const timing *timing) {
    unsigned int pos = 0;
    
    pos += timing_string_from_field(dest + pos, 0, 59, timing->minutes);
    
    dest[pos] = ' ';
    pos++;
    
    pos += timing_string_from_field(dest + pos, 0, 23, timing->hours);
    
    dest[pos] = ' ';
    pos++;
    
    pos += timing_string_from_field(dest + pos, 0, 6, timing->daysofweek);
    
    return (int)pos;
}

int timing_field_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max) {
    assert(string[0] != 0);
    
    uint64_t result = 0;
    unsigned int pos = 0;
    
    if (string[pos] == '*') {
        for (unsigned int i = min; i <= max; i++) {
            result <<= 1;
            result |= 1;
        }
        
        *dest = result;
        
        return 1;
    } else {
        unsigned int range_from_string_result = timing_range_from_string(&result, string + pos, min, max);
        assert(range_from_string_result > 0);
        
        pos += range_from_string_result;
        
        while (string[pos] == ',') {
            pos++;
            range_from_string_result = timing_range_from_string(&result, string + pos, min, max);
            assert(range_from_string_result > 0);
            
            pos += range_from_string_result;
        }
    }
    
    *dest = result;
    
    return (int)pos;
}

int timing_range_from_string(uint64_t *dest, const char *string, unsigned int min, unsigned int max) {
    unsigned long int start;
    unsigned long int end;
    unsigned int pos = 0;
    assert(min <= max && max <= min + 63);
    
    int uint_from_string_result = timing_uint_from_string(&start, string + pos);
    assert(uint_from_string_result > 0);
    
    pos += uint_from_string_result;
    
    if (string[pos] == '-') {
        pos++;
        uint_from_string_result = timing_uint_from_string(&end, string + pos);
        assert(uint_from_string_result > 0);
        
        pos += uint_from_string_result;
    } else {
        end = start;
    }
    
    assert(start >= min && end >= start && max >= end);
    
    uint64_t mask = 1LL << (start - min);
    for (unsigned long int i = start; i <= end; i++) {
        *dest |= mask;
        mask <<= 1;
    }
    
    return (int)pos;
}

int timing_uint_from_string(unsigned long int *dest, const char *string) {
    assert(isdigit(string[0]));
    
    char *endp;
    *dest = strtoul(string, &endp, 10);
    
    return (int)(endp - string);
}

int timing_string_from_field(char *dest, unsigned int min, unsigned int max, uint64_t field) {
    assert(min <= max && max <= min + 63);
    
    unsigned int pos = 0;
    int range_active = 0;
    unsigned int range_start;
    unsigned int range_stop;
    uint64_t mask = 1LL;
    
    for (unsigned int i = min; i <= max + 1; i++) {
        if (!range_active) {
            if ((mask & field) != 0) {
                range_active = 1;
                range_start = i;
            }
        } else {
            if (i == max + 1 || (mask & field) == 0) {
                range_stop = i - 1;
                
                if (pos > 0) {
                    dest[pos] = ',';
                    pos++;
                }
                
                if (range_start == min && range_stop == max) {
                    dest[pos] = '*';
                    pos++;
                    dest[pos] = '\0';
                } else {
                    pos += timing_string_from_range(dest + pos, range_start, range_stop);
                }
                
                range_active = 0;
            }
        }
        
        mask <<= 1;
    }
    
    return (int)pos;
}

int timing_string_from_range(char *dest, unsigned int start, unsigned int stop) {
    int sprintf_result;
    
    if (stop == start) {
        sprintf_result = sprintf(dest, "%u", start);
    } else {
        sprintf_result = sprintf(dest, "%u-%u", start, stop);
    }
    
    return sprintf_result;
}

int commandline_from_args(commandline *dest, unsigned int argc, char *argv[]) {
    dest->argc = argc;
    
    dest->argv = malloc(dest->argc * sizeof(string));
    assert(dest->argv);
    for (uint32_t i = 0; i < dest->argc; i++) {
        assert(string_from_cstring(&dest->argv[i], argv[i]) != -1);
    }
    
    return 0;
}

int write_buffer(int fd, const buffer *buf) {
    uint32_t remaining = buf->length;
    uint32_t i = 0;
    
    do {
        if (i > 0) {
            remaining -= PIPE_BUF;
        }
        
        assert(write(fd, buf->data + i * PIPE_BUF, remaining > PIPE_BUF ? PIPE_BUF : remaining) != -1);
    
        i++;
    } while (remaining > PIPE_BUF);
    
    return 0;
}

int write_uint8(buffer *buf, const uint8_t *n) {
    buf->data = realloc(buf->data, buf->length + sizeof(uint8_t));
    assert(buf->data);
    assert(memcpy(buf->data + buf->length, n, sizeof(uint8_t)) != NULL);
    buf->length += sizeof(uint8_t);
    
    return 0;
}

int write_uint16(buffer *buf, const uint16_t *n) {
    buf->data = realloc(buf->data, buf->length + sizeof(uint16_t));
    assert(buf->data);
    uint16_t be_n = htobe16(*n);
    assert(memcpy(buf->data + buf->length, &be_n, sizeof(uint16_t)) != NULL);
    buf->length += sizeof(uint16_t);
    
    return 0;
}

int write_uint32(buffer *buf, const uint32_t *n) {
    buf->data = realloc(buf->data, buf->length + sizeof(uint32_t));
    assert(buf->data);
    uint32_t be_n = htobe32(*n);
    assert(memcpy(buf->data + buf->length, &be_n, sizeof(uint32_t)) != NULL);
    buf->length += sizeof(uint32_t);
    
    return 0;
}

int write_uint64(buffer *buf, const uint64_t *n) {
    buf->data = realloc(buf->data, buf->length + sizeof(uint64_t));
    assert(buf->data);
    uint64_t be_n = htobe64(*n);
    assert(memcpy(buf->data + buf->length, &be_n, sizeof(uint64_t)) != NULL);
    buf->length += sizeof(uint64_t);
    
    return 0;
}

int write_string(buffer *buf, const string *string) {
    assert(write_uint32(buf, &string->length) != -1);
    
    for (uint32_t i = 0; i < string->length; i++) {
        assert(write_uint8(buf, &string->data[i]) != -1);
    }
    
    return 0;
}

int write_timing(buffer *buf, const timing *timing) {
    assert(write_uint64(buf, &timing->minutes) != -1);
    assert(write_uint32(buf, &timing->hours) != -1);
    assert(write_uint8(buf, &timing->daysofweek) != -1);
    
    return 0;
}

int write_commandline(buffer *buf, const commandline *commandline) {
    assert(write_uint32(buf, &commandline->argc) != -1);
    
    for (uint32_t i = 0; i < commandline->argc; i++) {
        assert(write_string(buf, &commandline->argv[i]) != -1);
    }
    
    return 0;
}

int write_task(buffer *buf, const task *task, int write_taskid) {
    if (write_taskid) {
        assert(write_uint64(buf, &task->taskid) != -1);
    }
    
    assert(write_timing(buf, &task->timing) != -1);
    assert(write_commandline(buf, &task->commandline) != -1);
    
    return 0;
}

int write_task_array(buffer *buf, const task *tasks) {
    uint32_t size = array_size(tasks);
    assert(write_uint32(buf, &size) != -1);
    
    for (uint32_t i = 0; i < size; i++) {
        assert(write_task(buf, &tasks[i], 1) != -1);
    }
    
    return 0;
}

int write_run(buffer *buf, const run *run) {
    assert(write_uint64(buf, &run->time) != -1);
    assert(write_uint16(buf, &run->exitcode) != -1);
    
    return 0;
}

int write_run_array(buffer *buf, const run *runs) {
    uint32_t size = array_size(runs);
    assert(write_uint32(buf, &size) != -1);
    
    for (uint32_t i = 0; i < size; i++) {
        assert(write_run(buf, &runs[i]) != -1);
    }
    
    return 0;
}

int read_uint8(int fd, uint8_t *n) {
    return (int)read(fd, n, sizeof(uint8_t));
}

int read_uint16(int fd, uint16_t *n) {
    uint16_t be_n;
    assert(read(fd, &be_n, sizeof(uint16_t)) != -1);
    *n = be16toh(be_n);
    
    return 0;
}

int read_uint32(int fd, uint32_t *n) {
    uint32_t be_n;
    assert(read(fd, &be_n, sizeof(uint32_t)) != -1);
    *n = be32toh(be_n);
    
    return 0;
}

int read_uint64(int fd, uint64_t *n) {
    uint64_t be_n;
    assert(read(fd, &be_n, sizeof(uint64_t)) != -1);
    *n = be64toh(be_n);
    
    return 0;
}

int read_string(int fd, string *string) {
    assert(read_uint32(fd, &string->length) != -1);
    
    string->data = malloc(string->length);
    assert(string->data);
    
    for (uint32_t i = 0; i < string->length; i++) {
        assert(read_uint8(fd, &string->data[i]) != -1);
    }
    
    return 0;
}

int read_timing(int fd, timing *timing) {
    assert(read_uint64(fd, &timing->minutes) != -1);
    assert(read_uint32(fd, &timing->hours) != -1);
    assert(read_uint8(fd, &timing->daysofweek) != -1);
    
    return 0;
}

int read_commandline(int fd, commandline *commandline) {
    assert(read_uint32(fd, &commandline->argc) != -1);
    
    commandline->argv = malloc(commandline->argc * sizeof(string));
    assert(commandline->argv);
    
    for (uint32_t i = 0; i < commandline->argc; i++) {
        assert(read_string(fd, &commandline->argv[i]) != -1);
    }
    
    return 0;
}

int read_task(int fd, task *task, int read_taskid) {
    if (read_taskid) {
        assert(read_uint64(fd, &task->taskid) != -1);
    }
    
    assert(read_timing(fd, &task->timing) != -1);
    assert(read_commandline(fd, &task->commandline) != -1);
    
    return 0;
}

int read_task_array(int fd, task **tasks) {
    uint32_t nbtasks;
    assert(read_uint32(fd, &nbtasks) != -1);
    
    for (uint32_t i = 0; i < nbtasks; i++) {
        task task;
        assert(read_task(fd, &task, 1) != -1);
        array_push(*tasks, task);
    }
    
    return (int)nbtasks;
}

int read_run(int fd, run *run) {
    assert(read_uint64(fd, &run->time) != -1);
    assert(read_uint16(fd, &run->exitcode) != -1);
    
    return 0;
}

int read_run_array(int fd, run **runs) {
    uint32_t nbruns;
    assert(read_uint32(fd, &nbruns) != -1);
    
    for (uint32_t i = 0; i < nbruns; i++) {
        run run;
        assert(read_run(fd, &run) != -1);
        array_push(*runs, run);
    }
    
    return (int)nbruns;
}

int free_string(string *string) {
    free(string->data);
    string->data = NULL;
    
    return 0;
}

int free_commandline(commandline *commandline) {
    for (uint32_t i = 0; i < commandline->argc; i++) {
        free_string(&commandline->argv[i]);
    }
    
    free(commandline->argv);
    commandline->argv = NULL;
    
    return 0;
}

int free_task(task *task) {
    free_commandline(&task->commandline);
    
    return 0;
}
