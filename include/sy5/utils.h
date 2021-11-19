#ifndef UTILS_H
#define UTILS_H

#include <sy5/types.h>

// Creates a directory by calling `mkdir` recursively on a path for every missing parts.
int mkdir_recursively(const char *path, mode_t mode);

#endif // UTILS_H.