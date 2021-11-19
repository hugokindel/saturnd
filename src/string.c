#include <sy5/string.h>
#include <string.h>
#include <sy5/endian.h>
#include <unistd.h>

void string_from_cstring(sy5_string *dest, char *char_array) {
    dest->length = strlen(char_array);
    
    for (int i = 0; i < dest->length + 1; i++) {
        dest->data[i] = char_array[i];
    }
}

void cstring_from_string(char *dest, sy5_string string) {
    for (int i = 0; i < string.length + 1; i++) {
        dest[i] = (uint8_t)string.data[i];
    }
}

int write_string(int fd, const sy5_string *string) {
    uint32_t be_length = htobe32(string->length);
    if (write(fd, &be_length, sizeof(uint32_t)) == -1) {
        return -1;
    }
    
    for (int i = 0; i < string->length; i++) {
        // `string->data[i]` doesn't have to be converted because it's only one byte.
        if (write(fd, &string->data[i], sizeof(uint8_t)) == -1) {
            return -1;
        }
    }
    
    return 0;
}