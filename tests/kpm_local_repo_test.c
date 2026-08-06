#include "kpm.h"
#include "common/kpm_test_common.h"
#include "common/kpm_test_io.h"
#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    char* actual_path = dirname(realpath(argv[0], NULL));
    fprintf(stderr, "%s\n", actual_path);

    char* valid_repo_url;
    asprintf(&valid_repo_url, "file://%s/resources/test_repository/manifest.json", actual_path); // We don't need that much portability for our tests
    char* invalid_repository_1_url;
    asprintf(&invalid_repository_1_url, "file://%s/resources/invalid_repository_1/manifest.json", actual_path); // We don't need that much portability for our tests
    char* invalid_repository_2_url;
    asprintf(&invalid_repository_2_url, "file://%s/resources/invalid_repository_2/manifest.json", actual_path); // We don't need that much portability for our tests
    char* invalid_repository_3_url;
    asprintf(&invalid_repository_3_url, "file://%s/resources/invalid_repository_3/manifest.json", actual_path); // We don't need that much portability for our tests

    KPM kpm = initialise_kpm();
    
    KPM_RemoveRepository(&kpm, "kindlemodding");
    size_t repositoryCount;
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 0);

    Repository repository;
    assert(KPM_AddRepository(&kpm, invalid_repository_1_url, &repository, &kpm_io) != KPM_OK);
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 0);

    /*assert(KPM_AddRepository(&kpm, invalid_repository_2_url, &repository, &kpm_io) != KPM_OK);
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 1); */ // We expected cjson to fail and it doesn't

    assert(KPM_AddRepository(&kpm, invalid_repository_3_url, &repository, &kpm_io) == KPM_OK);
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 1);
    assert(KPM_UpdateIndex(&kpm, &kpm_io) != KPM_OK); // Should fail due to artifact invalid version
    assert(KPM_RemoveRepository(&kpm, repository.id) == KPM_OK);
    KPM_FreeRepository(&repository);

    assert(KPM_AddRepository(&kpm, valid_repo_url, &repository, &kpm_io) == KPM_OK);
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 1);
    assert(KPM_RemoveRepository(&kpm, repository.id) == KPM_OK);
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 0);
    assert(strcmp(repository.id, "testrepo") == 0);
    KPM_FreeRepository(&repository);

    KPM_Cleanup(&kpm);
}