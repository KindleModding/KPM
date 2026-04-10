#include "kpm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

void statusCallback(enum Verbosity verbosity, char * format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

int main()
{
    struct KPM kpm;
    KPM_Initialise(&kpm, "./repo_test.db");

    char curDir[2048];
    getcwd(curDir, 2048);
    char* dir = dirname(curDir);

    /*char* uri = malloc(strlen("file://") + strlen(dir) + strlen("/examples/examplerepo/packages/com.kindlemodding.examplepackage/artifacts/com.kindlemodding.examplepackage_1.2.3_kindlehf-kindlepw2.kpkg") + 1);
    strcpy(uri, "file://");
    strcpy(uri + strlen("file://"), dir);
    strcpy(uri + strlen("file://") + strlen(dir), "/examples/examplerepo/packages/com.kindlemodding.examplepackage/artifacts/com.kindlemodding.examplepackage_1.2.3_kindlehf-kindlepw2.kpkg");
    uri[strlen("file://") + strlen(dir) + strlen("/examples/examplerepo/packages/com.kindlemodding.examplepackage/artifacts/com.kindlemodding.examplepackage_1.2.3_kindlehf-kindlepw2.kpkg")] = '\0';

    fprintf(stderr, "%s\n", uri);*/

    char* uri = "com.kindlemodding.examplepackage";

    struct InstallTarget target = {
        .id = uri,
        .repository = "",
        .version = NULL
    };
    
    struct KPMLogging logger = 
    {
        .log = statusCallback

    };
    int status = KPM_InstallPackage(&kpm, &target, &logger);
    fprintf(stderr, "Status: %i\n", status);
    assert(status == KPM_OK);

    KPM_Cleanup(&kpm);

    return 0;
}
