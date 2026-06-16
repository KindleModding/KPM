#include "kpm.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

void statusCallback(enum Verbosity verbosity, const char* format, ...)
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
    
    size_t repositoryCount;
    fprintf(stderr, "Testing repository list is not empty\n");
    KPM_ListRepositories(&kpm, &repositoryCount, NULL);
    assert(repositoryCount == 1);

    fprintf(stderr, "Getting repository\n");
    struct Repository repository;
    assert(KPM_GetRepository(&kpm, "org.kindlemodding.examplerepo", &repository) == KPM_OK);

    fprintf(stderr, "Indexing packages\n");
    struct KPMLogging logger = 
    {
        .log = statusCallback

    };
    assert(KPM_UpdateIndex(&kpm, &logger) == KPM_OK);

    fprintf(stderr, "Testing package list\n");
    size_t packageCount;
    struct IndexedPackage* packages;
    KPM_ListRepositoryPackages(&kpm, repository.id, &packageCount, &packages);

    fprintf(stderr, "Checking package count\n");
    assert(packageCount > 0);

    fprintf(stderr, "Got %zu packages:\n", packageCount);
    for (size_t i=0; i < packageCount; i++)
    {
        fprintf(stderr, "Repo: %s\n", packages[i].repository);
        fprintf(stderr, "Id: %s\n", packages[i].id);
        fprintf(stderr, "Name: %s\n", packages[i].name);
        fprintf(stderr, "Author: %s\n", packages[i].author);
        fprintf(stderr, "Description: %s\n", packages[i].description);

        size_t artifactCount;
        struct IndexedArtifact* artifacts;
        KPM_ListPackageArtifacts(&kpm, packages[i].repository, packages[i].id, &artifactCount, &artifacts);
        fprintf(stderr, "- %zu artifacts -\n", artifactCount);
        for (size_t j=0; j < artifactCount; j++)
        {
            fprintf(stderr, "\tId: %s\n", artifacts[j].id);
            fprintf(stderr, "\tUrl: %s\n", artifacts[j].url);
            fprintf(stderr, "\tVersion: %u.%u.%u\n", artifacts[j].version.major, artifacts[j].version.minor, artifacts[j].version.patch);
            fprintf(stderr, "\n");
        }
        KPM_FreeIndexedArtifactList(artifactCount, artifacts);
    }

    KPM_FreeRepository(&repository);
    KPM_FreeIndexedPackageList(packageCount, packages);
    KPM_Cleanup(&kpm);

    return 0;
}