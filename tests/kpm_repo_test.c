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
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 1);
    assert(KPM_ListRepositories(&kpm, &repositoryCount, &repositories) == KPM_OK);
    assert(repositoryCount == 1);

    assert(strcmp(repositories[0].id, "kindlemodding") == 0);
    assert(strcmp(repositories[0].name, "Official KMC Repo") == 0);
    assert(strcmp(repositories[0].description, "The official KMC repo") == 0);
    assert(strcmp(repositories[0].url, "https://repo.kindlemodding.org/manifest.v2.json") == 0);

    Repository repository;
    KPM_GetRepository(&kpm, "kindlemodding", &repository);
    assert(strcmp(repository.id, "kindlemodding") == 0);
    assert(strcmp(repository.name, "Official KMC Repo") == 0);
    assert(strcmp(repository.description, "The official KMC repo") == 0);
    assert(strcmp(repository.url, "https://repo.kindlemodding.org/manifest.v2.json") == 0);
    KPM_FreeRepository(&repository);
    KPM_FreeRepositoryList(repositoryCount, repositories);

    assert(KPM_AddRepository(&kpm, "https://google.com", NULL, &kpm_io) != KPM_OK);
    assert(KPM_RemoveRepository(&kpm, "thisrepodoesnotexist") == KPM_OK); // @TODO: Should we fail?

    KPM_Cleanup(&kpm);
}