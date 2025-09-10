#include "kpm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include "install.h"

void statusCallback(enum Verbosity verbosity, uint progress, char * format, ...)
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

    char* uri = malloc(strlen("file://") + strlen(dir) + strlen("/examples/examplerepo/packages/com.kindlemodding.examplepackage/artifacts/com.kindlemodding.examplepackage_1.2.3_armhf-armel.kpkg") + 1);
    strcpy(uri, "file://");
    strcpy(uri + strlen("file://"), dir);
    strcpy(uri + strlen("file://") + strlen(dir), "/examples/examplerepo/packages/com.kindlemodding.examplepackage/artifacts/com.kindlemodding.examplepackage_1.2.3_armhf-armel.kpkg");
    uri[strlen("file://") + strlen(dir) + strlen("/examples/examplerepo/packages/com.kindlemodding.examplepackage/artifacts/com.kindlemodding.examplepackage_1.2.3_armhf-armel.kpkg")] = '\0';

    struct InstallTarget target = {
        .dependency_type = KPM_DT_NONE,
        .id = uri,
        .repository = NULL,
        .version = {0, 0, 0}
    };
    
    size_t dependencyCount;
    struct Dependency* dependencies;
    enum KPMResult result = Internal_GetDependencies(&target, &dependencyCount, &dependencies, statusCallback);
    fprintf(stderr, "Result: %i\n", result);
    if (result != KPM_OK)
    {
        
        return result;
    }

    fprintf(stderr, "Got %lu Dependencies:\n", dependencyCount);
    for (size_t i=0; i < dependencyCount; i++)
    {
        fprintf(stderr, "\t id: %s\n", dependencies[i].id);
        fprintf(stderr, "\t repository: %s\n", dependencies[i].repository);
        fprintf(stderr, "\t type: %i\n", dependencies[i].type);
        fprintf(stderr, "\t id: %lu.%lu.%lu\n", dependencies[i].version.major, dependencies[i].version.minor, dependencies[i].version.patch);
    }

    KPM_Cleanup(&kpm);

    return 0;
}
