#include "kpm_test_common.h"
#include "kpm.h"
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

KPM *initialise_kpm()
{
    remove("./repo_test.db");
    return KPM_InitialiseAdv("./repo_test.db", "./packages", "./kpm_tmp");
}

/**
 * @brief vasprintf implementation
 * 
 * @param format 
 * @param args
 * @return char*
 */
char *vasprintf_hd(const char *format, va_list args)
{
    va_list args2;
    va_copy (args2, args);
    int size = vsnprintf(NULL, 0, format, args) + 1;
    char *str = malloc(size);
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
char *asprintf_hd(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    char *result = vasprintf_hd(format, args);
    va_end(args);
    return result;
}

char *get_path(int argc, char* argv[])
{
    ASSERT_HD(argc > 0, 1);
    char cur_path[PATH_MAX];
    realpath(dirname(argv[0]), cur_path);
    return strdup(cur_path);
}