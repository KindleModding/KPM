#include "kpm.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <libgen.h>

void statusCallback(enum KPMVerbosity verbosity, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

int main()
{
    struct KPM kpm = {
        .dbPath = "./repo_test.db",
        .pkgPath = "/tmp/",
        .maxConnections = 5,
    };

    KPM_Initialise(&kpm);

    char* id = "examplepackage";

    struct InstallTarget target = {
        .id = id,
        .repository = NULL,
        .version = NULL
    };
    
    struct KPMIO kpmIO = 
    {
        .log = statusCallback

    };
    int status = KPM_InstallPackages(&kpm, 1, &target, &kpmIO);
    fprintf(stderr, "Status: %i\n", status);
    assert(status == KPM_OK);

    KPM_Cleanup(&kpm);

    return 0;
}
