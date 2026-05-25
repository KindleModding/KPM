#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Recursively create a folder and its parents
 * 
 * @param path The full path to the target folder
 * @param mode See mkdir() docs
 */
void mkdir_r(char* path, __mode_t mode)
{
    char* current_path = malloc(strlen(path) + 1);
    current_path[0] = path[0];
    for (int i=1; i < strlen(path); i++)
    {
        current_path[i] = path[i];
        current_path[i+1] = 0;
        if (current_path[i] == '/' && access(current_path, R_OK) != 0)
            mkdir(current_path, mode);
    }
    free(current_path);
}