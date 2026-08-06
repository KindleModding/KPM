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
    
    KPM kpm = initialise_kpm();
    
    KPM_RemoveRepository(&kpm, "kindlemodding");
    size_t repositoryCount;
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 0);

    Repository repository;
    assert(KPM_AddRepository(&kpm, valid_repo_url, &repository, &kpm_io) == KPM_OK);
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 1);


    assert(KPM_UpdateIndex(&kpm, &kpm_io) == KPM_OK);
    InstallTarget target  = {
        .id = "validpackage1",
        .repository = NULL,
        .version = NULL
    };
    assert(KPM_InstallPackages(&kpm, 1, &target, &kpm_io) == KPM_OK);
    target.id = "validpackage2";
    assert(KPM_InstallPackages(&kpm, 1, &target, &kpm_io) == KPM_OK);

    char* package_ids[] = {
        "validpackage1",
        "validpackage2"
    };
    assert(KPM_UninstallPackages(&kpm, 2, (const char**) package_ids, &kpm_io) == KPM_OK);

    target.id = "invalidpackage1";
    assert(KPM_InstallPackages(&kpm, 1, &target, &kpm_io) != KPM_OK);
    target.id = "invalidpackage2";
    assert(KPM_InstallPackages(&kpm, 1, &target, &kpm_io) != KPM_OK);

    InstallTarget targets[] = {
        {
            .id = "invalidpackage1",
            .repository = NULL,
            .version = NULL
        },
        {
            .id = "invalidpackage2",
            .repository = NULL,
            .version = NULL
        }
    };
    assert(KPM_InstallPackages(&kpm, 2, targets, &kpm_io) != KPM_OK);

    assert(KPM_RemoveRepository(&kpm, repository.id) == KPM_OK);
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 0);
    assert(strcmp(repository.id, "testrepo") == 0);
    KPM_FreeRepository(&repository);

    KPM_Cleanup(&kpm);
}