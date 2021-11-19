#include <sy5/commandline.h>
#include <sy5/string.h>
#include <sy5/endian.h>
#include <unistd.h>

void commandline_from_args(sy5_commandline *dest, int argc, char *argv[]) {
    dest->argc = argc;
    for (int i = 0; i < dest->argc; i++) {
        string_from_cstring(&dest->argv[i], argv[i]);
    }
}

int write_commandline(int fd, const sy5_commandline *commandline) {
    uint32_t be_argc = htobe32(commandline->argc);
    if (write(fd, &be_argc, sizeof(uint32_t)) == -1) {
        return -1;
    }
    
    for (int i = 0; i < commandline->argc; i++) {
        if (write_string(fd, &commandline->argv[i]) == -1) {
            return -1;
        }
    }
    
    return 0;
}