#include "kpm.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

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
        .id = uri,
        .repository = "",
        .version = NULL
    };
    
    KPM_InstallPackage(&kpm, &target, NULL);

    KPM_Cleanup(&kpm);

    return 0;
}
