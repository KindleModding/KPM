#pragma once

#include <stdio.h>
#include <sys/stat.h>

#define AssertOrReturn(expr, expected, retval) \
    if (__builtin_expect(expr != expected, 0)) \
        return retval;

void mkdir_r(char* path, __mode_t mode);
void rmdir_r(char* path);

char* vasprintf_hd(const char* format, va_list args);
char* asprintf_hd(const char* format, ...) __attribute__((format(printf, 1, 2)));
char* get_realpath(const char* location);