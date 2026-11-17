#include "kpm.h"
#include "internal_utils.h"
#include <stdio.h>
#include <stdlib.h>

void kpm_stream(char c)
{
    printf("%c", c);
}

void vhd_log(const char *format, va_list args)
{
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

void hd_log(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vhd_log(format, args);
    va_end(args);
}

void kpm_log(KPMIO_Verbosity verbosity, const char *format, ...)
{
    char *prefixed_format;
    switch (verbosity)
    {
        case KPM_VERBOSITY_DEBUG:
            prefixed_format = asprintf_hd("\x1b[0;1;36m[DEBUG] %s\x1b[0m", format);
            break;
        case KPM_VERBOSITY_WARN:
            prefixed_format = asprintf_hd("\x1b[0;33m[WARN] %s\x1b[0m", format);
            break;
        case KPM_VERBOSITY_ERROR:
            prefixed_format = asprintf_hd("\x1b[0;1;31m[ERR] %s\x1b[0m", format);
            break;
        default:
            prefixed_format = asprintf_hd("\x1b[0m%s\x1b[0m", format);
            break;
    }

    va_list args;
    va_start(args, format);
    vhd_log(prefixed_format, args);
    va_end(args);
    
    free(prefixed_format);
}

void kpm_log_progress(unsigned int progress, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

bool kpm_get_input(const char *format, ...)
{
    return true;
}


KPMIO kpmio = {
    .stream = kpm_stream,
    .log = kpm_log,
    .logProgress = kpm_log_progress,
    .getInput = kpm_get_input
};