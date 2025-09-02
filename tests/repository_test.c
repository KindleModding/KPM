#include "kpm.h"
#include <stdio.h>
#include <assert.h>

int main()
{
    struct KPM kpm;
    KPM_Initialise(&kpm, "./repo_test.db");
    
    fprintf(stderr, "Testing repository list is empty\n");
    size_t repositoryCount;

    KPM_ListRepositories(&kpm, &repositoryCount, NULL);
    assert(repositoryCount == 0);

    fprintf(stderr, "Adding test repository\n");

    assert(KPM_AddRepository(&kpm, "https://example.com", NULL) == KPM_OK);

    fprintf(stderr, "Testing repository list is not empty\n");
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == 1);

    fprintf(stderr, "Removing repository\n");
    KPM_RemoveRepository(&kpm, "");

    fprintf(stderr, "Testing repository list is empty\n");
    KPM_ListRepositories(&kpm, &repositoryCount, NULL);
    assert(repositoryCount == 0);

    KPM_Cleanup(&kpm);

    return 0;
}