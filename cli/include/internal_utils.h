#pragma once

#include <stdio.h>
#include <sys/stat.h>

void mkdir_r(char* path, __mode_t mode);
void rmdir_r(char* path);

char* vasprintf_hd(const char* format, va_list args);
char* asprintf_hd(const char* format, ...) __attribute__((format(printf, 1, 2)));