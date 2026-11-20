#ifndef INTERNAL_UTILS_H
#define INTERNAL_UTILS_H

#include "sqlite3.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>

void KPM_MigrateDB(sqlite3 *db);

void mkdir_r(char *path, __mode_t mode);
void rmdir_r(char *path);

char *vasprintf_hd(const char *format, va_list args);
char *asprintf_hd(const char *format, ...) __attribute__((format(printf, 1, 2)));
bool is_file(const char *location);

bool will_overflow32(uint32_t a, uint32_t b);
bool will_overflow64(uint64_t a, uint64_t b);

#endif