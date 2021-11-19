#ifndef COMMANDLINE_H
#define COMMANDLINE_H

#include <sy5/types.h>

void commandline_from_args(sy5_commandline *dest, int argc, char *argv[]);

int write_commandline(int fd, const sy5_commandline *commandline);

#endif // COMMANDLINE_H.