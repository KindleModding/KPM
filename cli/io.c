#include "cli.h"
#include "kpm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fbink.h"

#include "io.h"
#include "internal_utils.h"

#ifdef NDEBUG
#define VERBOSE false
#else
#define VERBOSE true
#endif

struct IOState io_state = {
    .framebuffer = 0
};

FBInkConfig fbink_config = {
    .row = 1,
    .voffset = 0,
    .is_verbose = VERBOSE,
    .is_quiet = !VERBOSE,
    .wfm_mode = WFM_AUTO,
    .no_refresh = false,
    .is_cleared = false,
    .fontmult = 0
};

void io_initialise()
{
    if (cli_state.fbink)
    {
        io_state.framebuffer = fbink_open();
        fbink_init(io_state.framebuffer, &fbink_config);

        FBInkState fbink_initial_state = { };
        fbink_get_state(&fbink_config, &fbink_initial_state);
        fbink_config.fontmult = fbink_initial_state.fontsize_mult/1.5;
        FBInkRect rect = {
            .left = 0,
            .top = 0,
            .width = fbink_initial_state.screen_width,
            .height = fbink_initial_state.screen_height
        };
        fbink_cls(io_state.framebuffer, &fbink_config, &rect, false);
        fbink_refresh(io_state.framebuffer, 0, 0, fbink_initial_state.screen_width, fbink_initial_state.screen_height, &fbink_config);
    }
}

void io_cleanup()
{
    printf("\x1b[0m");
    if (cli_state.fbink)
    {
        fbink_close(io_state.framebuffer);
    }
}

void vkpm_fbink_printf(const char* format, va_list args)
{
	// Initial init to pull info
	int r = fbink_init(io_state.framebuffer, &fbink_config);
    if (r >= 0)
    {
        int rows;
        char* string = vasprintf_hd(format, args);
        if ((rows = fbink_print(io_state.framebuffer, string, &fbink_config)) > 0)
            fbink_config.row += rows;
    }
}

__attribute__((format(printf, 1, 2))) void kpm_fbink_printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vkpm_fbink_printf(format, args);
    va_end(args);
}

void kpm_stream(char c)
{
    printf("%c", c);
}

void vhd_log(const char* format, va_list args)
{
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
}

void hd_log(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vhd_log(format, args);
    va_end(args);
}

void kpm_log(enum KPMVerbosity verbosity, const char* format, ...)
{
    char* prefixed_format;
    char* prefixed_format_fbink;
    switch (verbosity)
    {
        case KPM_VERBOSITY_DEBUG:
#ifdef NDEBUG
            return;
#endif
            prefixed_format = asprintf_hd("\x1b[0;1;36m[DEBUG] %s", format);
            prefixed_format_fbink = asprintf_hd("[DEBUG] %s", format);
            break;
        case KPM_VERBOSITY_WARN:
            prefixed_format = asprintf_hd("\x1b[0;33m[WARN] %s", format);
            prefixed_format_fbink = asprintf_hd("[WARN] %s", format);
            break;
        case KPM_VERBOSITY_ERROR:
            prefixed_format = asprintf_hd("\x1b[0;1;31m[ERR] %s", format);
            prefixed_format_fbink = asprintf_hd("[ERR] %s", format);
            break;
        default:
            prefixed_format = asprintf_hd("\x1b[0m%s", format);
            prefixed_format_fbink = strdup(format);
            break;
    }

    va_list args;
    va_start(args, format);
    if (cli_state.fbink)
    {
        va_list args2;
        va_copy (args2, args);
        vkpm_fbink_printf(prefixed_format_fbink, args2);
        va_end(args2);
    }
    vhd_log(prefixed_format, args);
    va_end(args);
    
    free(prefixed_format);
    free(prefixed_format_fbink);
}

void kpm_log_progress(unsigned int progress, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

bool kpm_get_input(const char* format, ...)
{
    if (cli_state.fbink || !cli_state.confirm)
        return true;

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, " [Y/n] ");
    va_end(args);

    char c;
    scanf("%c", &c);
    return c != 'n' && c != 'N';
}


struct KPMIO kpm_io = {
    .stream = kpm_stream,
    .log = kpm_log,
    .logProgress = kpm_log_progress,
    .getInput = kpm_get_input
};