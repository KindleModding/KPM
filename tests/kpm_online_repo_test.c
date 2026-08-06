#include "kpm.h"
#include "common/kpm_test_common.h"
#include "common/kpm_test_io.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    KPM kpm = initialise_kpm();
    Repository* repositories;
    size_t repositoryCount;
    kpm_io.log(KPM_VERBOSITY_INFO, "Checking repo count");
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 1);
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, &repositories) == KPM_OK);
    assert_hd(repositoryCount == 1);

    kpm_io.log(KPM_VERBOSITY_INFO, "Checking repo list info");
    assert_hd(strcmp(repositories[0].id, "kindlemodding") == 0);
    assert_hd(strcmp(repositories[0].name, "Official KMC Repo") == 0);
    assert_hd(strcmp(repositories[0].description, "The official KMC repo") == 0);
    assert_hd(strcmp(repositories[0].url, "https://repo.kindlemodding.org/manifest.v2.json") == 0);

    Repository repository;
    kpm_io.log(KPM_VERBOSITY_INFO, "Testing GetRepository");
    KPM_GetRepository(&kpm, "kindlemodding", &repository);
    assert_hd(strcmp(repository.id, "kindlemodding") == 0);
    assert_hd(strcmp(repository.name, "Official KMC Repo") == 0);
    assert_hd(strcmp(repository.description, "The official KMC repo") == 0);
    assert_hd(strcmp(repository.url, "https://repo.kindlemodding.org/manifest.v2.json") == 0);
    KPM_FreeRepository(&repository);
    KPM_FreeRepositoryList(repositoryCount, repositories);

    kpm_io.log(KPM_VERBOSITY_INFO, "Testing AddRepository");
    assert_hd(KPM_AddRepository(&kpm, "https://google.com", NULL, &kpm_io) != KPM_OK);

    kpm_io.log(KPM_VERBOSITY_INFO, "Testing RemoveRepository");
    assert_hd(KPM_RemoveRepository(&kpm, "thisrepodoesnotexist") == KPM_OK); // @TODO: Should we fail?

    kpm_io.log(KPM_VERBOSITY_INFO, "Testing UpdateIndex");
    assert_hd(KPM_UpdateIndex(&kpm, &kpm_io) == KPM_OK);

    KPM_Cleanup(&kpm);

    return 0;
}