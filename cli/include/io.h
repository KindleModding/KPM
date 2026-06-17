#pragma once

#include <stdio.h>

void vhd_log(const char* format, va_list args);
void hd_log(const char* format, ...)  __attribute__((format(printf, 1, 2)));

extern struct KPMIO kpm_io;