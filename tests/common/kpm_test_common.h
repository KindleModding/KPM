#pragma once
#include "kpm.h"
#include <stdio.h>
#include <stdlib.h>

KPM initialise_kpm();

char* vasprintf_hd(const char* format, va_list args);
char* asprintf_hd(const char* format, ...);