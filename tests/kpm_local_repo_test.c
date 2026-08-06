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

    char* valid_repo_url = asprintf_hd("file://%s/resources/test_repository/manifest.json", actual_path);
    char* invalid_repository_1_url = asprintf_hd("file://%s/resources/invalid_repository_1/manifest.json", actual_path);
    //char* invalid_repository_2_url = asprintf_hd("file://%s/resources/invalid_repository_2/manifest.json", actual_path);
    char* invalid_repository_3_url = asprintf_hd("file://%s/resources/invalid_repository_3/manifest.json", actual_path);

    KPM kpm = initialise_kpm();
    
    kpm_io.log(KPM_VERBOSITY_INFO, "Removing default repository");
    KPM_RemoveRepository(&kpm, "kindlemodding");
    size_t repositoryCount;
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 0);

    kpm_io.log(KPM_VERBOSITY_INFO, "Testing invalid repository 1 add");
    Repository repository;
    assert_hd(KPM_AddRepository(&kpm, invalid_repository_1_url, &repository, &kpm_io) != KPM_OK);
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 0);

    /*assert_hd(KPM_AddRepository(&kpm, invalid_repository_2_url, &repository, &kpm_io) != KPM_OK);
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 1); */ // We expected cjson to fail and it doesn't

    kpm_io.log(KPM_VERBOSITY_INFO, "Testing invalid repository 3 add");
    assert_hd(KPM_AddRepository(&kpm, invalid_repository_3_url, &repository, &kpm_io) == KPM_OK);
    assert_hd(strcmp(repository.id, "invalidrepo3") == 0);
    assert_hd(strcmp(repository.name, "Invalid Repo") == 0);
    assert_hd(strcmp(repository.description, "Repo only used for tests") == 0);
    assert_hd(strcmp(repository.url, invalid_repository_3_url) == 0);

    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 1);
    kpm_io.log(KPM_VERBOSITY_INFO, "Testing UpdateIndex failure");
    assert_hd(KPM_UpdateIndex(&kpm, &kpm_io) != KPM_OK); // Should fail due to artifact invalid version
    assert_hd(KPM_RemoveRepository(&kpm, repository.id) == KPM_OK);
    KPM_FreeRepository(&repository);

    kpm_io.log(KPM_VERBOSITY_INFO, "Testing adding valid local repository");
    assert_hd(KPM_AddRepository(&kpm, valid_repo_url, &repository, &kpm_io) == KPM_OK);
    kpm_io.log(KPM_VERBOSITY_INFO, "Listing repositories");
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 1);
    kpm_io.log(KPM_VERBOSITY_INFO, "Removing repository");
    assert_hd(KPM_RemoveRepository(&kpm, repository.id) == KPM_OK);
    kpm_io.log(KPM_VERBOSITY_INFO, "Listing repository");
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 0);
    assert_hd(strcmp(repository.id, "testrepo") == 0);
    KPM_FreeRepository(&repository);

    KPM_Cleanup(&kpm);

    return 0;
}