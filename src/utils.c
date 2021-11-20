#include <sy5/utils.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define htobe16(x) OSSwapHostToBigInt16(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#else
#include <endian.h>
#endif

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
    
    assert(dest->length <= MAX_STRING_LENGTH);
    
    for (int i = 0; i < dest->length; i++) {
        dest->data[i] = cstring[i];
    }
    
    dest->data[dest->length] = '\0';
    
    return 0;
}

int cstring_from_string(char *dest, const string *string) {
    assert(string->length <= MAX_STRING_LENGTH);
    
    for (int i = 0; i < string->length; i++) {
        dest[i] = (uint8_t)string->data[i]; // NOLINT
    }
    
    dest[string->length] = '\0';
    
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
    assert(argc <= MAX_COMMANDLINE_ARGUMENTS);
    
    dest->argc = argc;
    
    for (int i = 0; i < dest->argc; i++) {
        assert(string_from_cstring(&dest->argv[i], argv[i]) != -1);
    }
    
    return 0;
}

int write_uint8(int fd, const uint8_t *n) {
    assert(write(fd, n, sizeof(uint8_t)) != -1);
    
    return 0;
}

int write_uint16(int fd, const uint16_t *n) {
    uint16_t be_n = htobe16(*n);
    assert(write(fd, &be_n, sizeof(uint16_t)) != -1);
    
    return 0;
}

int write_uint32(int fd, const uint32_t *n) {
    uint32_t be_n = htobe32(*n);
    assert(write(fd, &be_n, sizeof(uint32_t)) != -1);
    
    return 0;
}

int write_uint64(int fd, const uint64_t *n) {
    uint64_t be_n = htobe64(*n);
    assert(write(fd, &be_n, sizeof(uint64_t)) != -1);
    
    return 0;
}

int write_string(int fd, const string *string) {
    assert(write_uint32(fd, &string->length) != -1);
    
    for (int i = 0; i < string->length; i++) {
        assert(write_uint8(fd, &string->data[i]) != -1);
    }
    
    return 0;
}

int write_timing(int fd, const timing *timing) {
    assert(write_uint64(fd, &timing->minutes) != -1);
    assert(write_uint32(fd, &timing->hours) != -1);
    assert(write_uint8(fd, &timing->daysofweek) != -1);
    
    return 0;
}

int write_commandline(int fd, const commandline *commandline) {
    assert(write_uint32(fd, &commandline->argc) != -1);
    
    for (int i = 0; i < commandline->argc; i++) {
        assert(write_string(fd, &commandline->argv[i]) != -1);
    }
    
    return 0;
}

int write_task(int fd, const task *task, bool write_taskid) {
    if (write_taskid) {
        assert(write_uint64(fd, &task->taskid) != -1);
    }
    
    assert(write_timing(fd, &task->timing) != -1);
    assert(write_commandline(fd, &task->commandline) != -1);
    
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
    
    for (int i = 0; i < string->length; i++) {
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
    
    for (int i = 0; i < commandline->argc; i++) {
        assert(read_string(fd, &commandline->argv[i]) != -1);
    }
    
    return 0;
}

int read_task(int fd, task *task, bool read_taskid) {
    if (read_taskid) {
        assert(read_uint64(fd, &task->taskid) != -1);
    }
    
    assert(read_timing(fd, &task->timing) != -1);
    assert(read_commandline(fd, &task->commandline) != -1);
    
    return 0;
}

int read_task_array(int fd, task task[], bool read_taskid) {
    uint32_t nbtasks;
    assert(read_uint32(fd, &nbtasks) != -1);
    
    for (int i = 0; i < nbtasks; i++) {
        assert(read_task(fd, &task[i], read_taskid) != -1);
    }
    
    return (int)nbtasks;
}