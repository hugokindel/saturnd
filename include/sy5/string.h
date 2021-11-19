#ifndef STRING_H
#define STRING_H

#include <sy5/types.h>

void string_from_cstring(sy5_string *dest, char *char_array);

void cstring_from_string(char *dest, sy5_string string);

int write_string(int fd, const sy5_string *string);

#endif // STRING_H.