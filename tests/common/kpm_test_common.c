#include "kpm.h"
#include <stdio.h>
#include <stdlib.h>

KPM initialise_kpm()
{
    KPM kpm = {
        .dbPath = "./repo_test.db",
        .pkgPath = "./packages",
        .maxConnections = 5
    };

    KPM_Initialise(&kpm);
    return kpm;
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