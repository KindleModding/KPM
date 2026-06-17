#include "cli.h"
#include "kpm.h"
#include <stdio.h>
#include "fbink.h"

#include "logging.h"
#include "internal_utils.h"

void initialise()
{
    int framebuffer = fbink_open();
    FBInkConfig config = {
		.row = 0,
		.voffset = 0,
		.is_verbose = false,
		.is_quiet = true,
		.wfm_mode = WFM_AUTO,
	};

	// Initial init to pull info
	fbink_init(framebuffer, &config);
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

void kpm_log_progress(uint progress, const char* format, ...)
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