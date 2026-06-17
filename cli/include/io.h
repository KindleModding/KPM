#pragma once

#include <stdio.h>
#include "kpm.h"

void io_initialise();
void io_cleanup();
void vhd_log(const char* format, va_list args);
void hd_log(const char* format, ...)  __attribute__((format(printf, 1, 2)));

struct IOState {
    int current_row;
    int framebuffer;
};

extern struct IOState io_state;
extern struct KPMIO kpm_io;