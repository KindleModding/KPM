#include "kpm.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

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
    
    size_t repositoryCount;
    fprintf(stderr, "Testing repository list is empty\n");
    KPM_ListRepositories(&kpm, &repositoryCount, NULL);
    assert(repositoryCount == 0);

    fprintf(stderr, "Indexing packages\n");
    assert(KPM_UpdateIndex(&kpm, statusCallback) == KPM_OK);

    KPM_Cleanup(&kpm);

    return 0;
}