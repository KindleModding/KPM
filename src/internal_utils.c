#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define __USE_XOPEN_EXTENDED   /* See feature_test_macros(7) */
#include <ftw.h>

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


int internal_delete(const char* fpath, const struct stat* sb, int typeflag, struct FTW *ftwbuf)
{
    switch (typeflag)
    {
        case FTW_F:
            remove(fpath);
            break;
        case FTW_D:
        case FTW_DP:
            rmdir(fpath);
            break;
        default:
            break;
    }
    return 0;
}

/**
 * @brief Deletes a folder recursively, equivalent to "rm -rf"
 * 
 * @param path 
 */
void rmdir_r(char* path)
{
    if (!access(path, R_OK))
        return;
    nftw(path, &internal_delete, 256, FTW_DEPTH);
    rmdir(path);
}

/**
 * @brief vasprintf implementation
 * 
 * @param format 
 * @param args
 * @return char* 
 */
char* vasprintf_hd(const char* format, va_list args)
{
    va_list args2;
    va_copy (args2, args);
    int size = vsnprintf(NULL, 0, format, args) + 1;
    char* str = malloc(size);
    vsnprintf(str, size, format, args2);
    va_end(args2);
    return str;
}

/**
 * @brief asprintf implementation
 * 
 * @param format 
 * @param ... 
 * @return char* 
 */
char* asprintf_hd(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char* result = vasprintf_hd(format, args);
    va_end(args);
    return result;
}