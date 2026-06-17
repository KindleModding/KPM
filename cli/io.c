#include "cli.h"
#include "kpm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fbink.h"

#include "io.h"
#include "internal_utils.h"

#ifdef NDEBUG
constexpr bool verbose=false;
#else
constexpr bool verbose=true;
#endif

struct IOState io_state = {
    .current_row = 0,
    .framebuffer = 0
};

void io_initialise()
{
    io_state.framebuffer = fbink_open();
}

void vkpm_fbink_printf(const char* format, va_list args)
{
    const FBInkConfig config = {
		.row = io_state.current_row,
		.voffset = 0,
		.is_verbose = verbose,
		.is_quiet = !verbose,
		.wfm_mode = WFM_AUTO,
	};
	// Initial init to pull info
	int fbfd = fbink_init(io_state.framebuffer, &config);
    if (fbfd >= 0)
    {
        char* string = vasprintf_hd(format, args);
        size_t string_len = strlen(string);
        char* buffer = malloc(string_len + 1);
        buffer[0] = '\0';
        size_t buffer_start_index = 0;
        for (int i=0; i < string_len; i++)
        {
            if (string[i] == '\n')
            {
                strncpy(buffer, string+buffer_start_index, i-buffer_start_index);
                buffer[(i-buffer_start_index) + 1] = '\0';
                const FBInkConfig config = {
                    .row = io_state.current_row,
                    .voffset = 0,
                    .is_verbose = verbose,
                    .is_quiet = !verbose,
                    .wfm_mode = WFM_AUTO,
                };
                fbink_print(fbfd, buffer, &config);
                buffer_start_index = i+1;
                io_state.current_row++;
            }
        }
        free(buffer);
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
    if (cli_state.fbink)
    {
        va_list args2;
        va_copy (args2, args);
        vkpm_fbink_printf(format, args2);
        va_end(args2);
    }
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

void kpm_log(enum Verbosity verbosity, const char* format, ...)
{
    char* prefixed_format;
    switch (verbosity)
    {
        case KPM_VERBOSITY_DEBUG:
#ifdef NDEBUG
            return;
#endif
            prefixed_format = asprintf_hd("\x1b[0;1;36m[DEBUG] %s", format);
            break;
        case KPM_VERBOSITY_WARN:
            prefixed_format = asprintf_hd("\x1b[0;33m[WARN] %s", format);
            break;
        case KPM_VERBOSITY_ERROR:
            prefixed_format = asprintf_hd("\x1b[0;1;31m[ERR] %s", format);
            break;
        default:
            prefixed_format = asprintf_hd("\x1b[0m%s", format);
            break;
    }

    va_list args;
    va_start(args, format);
    vhd_log(prefixed_format, args);
    va_end(args);
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
    if (!cli_state.confirm)
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