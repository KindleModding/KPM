#pragma once

#include <sys/stat.h>

void mkdir_r(char* path, __mode_t mode);
char* asprintf_hd(const char * format, ...) __attribute__((format(printf, 1, 2)));