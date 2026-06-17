#include "kpm.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

int main()
{
    struct KPM kpm = {
        .dbPath = "./repo_test.db",
        .pkgPath = "/tmp/packages",
        .maxConnections = 5
    };
    KPM_Initialise(&kpm);
    
    size_t repositoryCount;
    fprintf(stderr, "Testing repository list is not empty\n");
    KPM_ListRepositories(&kpm, &repositoryCount, NULL);
    assert(repositoryCount == 1);

    fprintf(stderr, "Removing repository\n");
    KPM_RemoveRepository(&kpm, "org.kindlemodding.examplerepo");

    fprintf(stderr, "Testing repository list is empty\n");
    KPM_ListRepositories(&kpm, &repositoryCount, NULL);
    assert(repositoryCount == 0);

    KPM_Cleanup(&kpm);

    return 0;
}