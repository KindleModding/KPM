#include "kpm.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

int main()
{
    struct KPM kpm;
    KPM_Initialise(&kpm, "./repo_test.db");
    
    fprintf(stderr, "Testing repository list is empty\n");
    size_t repositoryCount;

    KPM_ListRepositories(&kpm, &repositoryCount, NULL);
    assert(repositoryCount == 0);

    fprintf(stderr, "Adding test repository\n");

    char curDir[2048];
    getcwd(curDir, 2048);
    char* dir = dirname(curDir);

    char* uri = malloc(strlen("file://") + strlen(dir) + strlen("/examples/examplerepo/manifest.json") + 1);
    strcpy(uri, "file://");
    strcpy(uri + strlen("file://"), dir);
    strcpy(uri + strlen("file://") + strlen(dir), "/examples/examplerepo/manifest.json");
    uri[strlen("file://") + strlen(dir) + strlen("/examples/examplerepo/manifest.json")] = '\0';
    
    fprintf(stderr, "URI: %s\n", uri);
    struct Repository repo;
    assert(KPM_AddRepository(&kpm, uri, &repo) == KPM_OK);

    fprintf(stderr, "Testing repository list is not empty\n");
    KPM_ListRepositories(&kpm, &repositoryCount, NULL);
    assert(repositoryCount == 1);

    fprintf(stderr, "Testing repository ID\n");
    assert(strcmp(repo.id, "org.kindlemodding.examplerepo") == 0);

    free(uri);
    KPM_FreeRepository(&repo);
    KPM_Cleanup(&kpm);

    return 0;
}