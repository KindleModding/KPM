#ifndef KPM_TEST_COMMON_H
#define KPM_TEST_COMMON_H

#include "kpm.h"
#include <stdio.h>
#include <stdlib.h>

KPM initialise_kpm();

char *vasprintf_hd(const char *format, va_list args);
char *asprintf_hd(const char *format, ...);

inline void assert_hd(bool assertion)
{
    if (!assertion)
    {
        fprintf(stderr, "Assertion failed at %s:%i.", __FILE__, __LINE__);
        exit(1);
    }
}

#endif