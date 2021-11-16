#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sy5/utils.h>

int mkdir_recursively(const char *path, mode_t mode) {
    const char *pathIterator = path;
    char *directoryName = calloc(1, strlen(path) + 1);
    int err = 0;
    
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
    
    return err;
}