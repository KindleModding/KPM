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

    char* valid_repo_url = asprintf_hd("file://%s/resources/test_repository/manifest.json", actual_path); // We don't need that much portability for our tests
    
    KPM kpm = initialise_kpm();
    
    kpm_io.log(KPM_VERBOSITY_INFO, "Removing default repository");
    KPM_RemoveRepository(&kpm, "kindlemodding");
    size_t repositoryCount;
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 0);

    Repository repository;
    kpm_io.log(KPM_VERBOSITY_INFO, "Adding valid repository");
    assert_hd(KPM_AddRepository(&kpm, valid_repo_url, &repository, &kpm_io) == KPM_OK);
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 1);


    kpm_io.log(KPM_VERBOSITY_INFO, "Updating index");
    assert_hd(KPM_UpdateIndex(&kpm, &kpm_io) == KPM_OK);
    InstallTarget target  = {
        .id = "validpackage1",
        .repository = NULL,
        .version = NULL
    };
    kpm_io.log(KPM_VERBOSITY_INFO, "Installing validpackage1");
    assert_hd(KPM_InstallPackages(&kpm, 1, &target, &kpm_io) == KPM_OK);

    kpm_io.log(KPM_VERBOSITY_INFO, "Installing validpackage2");
    target.id = "validpackage2";
    assert_hd(KPM_InstallPackages(&kpm, 1, &target, &kpm_io) == KPM_OK);

    kpm_io.log(KPM_VERBOSITY_INFO, "Uninstalling both packages");
    char* package_ids[] = {
        "validpackage1",
        "validpackage2"
    };
    assert_hd(KPM_UninstallPackages(&kpm, 2, (const char**) package_ids, &kpm_io) == KPM_OK);

    kpm_io.log(KPM_VERBOSITY_INFO, "Installing invalidpackage1");
    target.id = "invalidpackage1";
    assert_hd(KPM_InstallPackages(&kpm, 1, &target, &kpm_io) != KPM_OK);

    kpm_io.log(KPM_VERBOSITY_INFO, "Installing invalidpackage2");
    target.id = "invalidpackage2";
    assert_hd(KPM_InstallPackages(&kpm, 1, &target, &kpm_io) != KPM_OK);

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
    kpm_io.log(KPM_VERBOSITY_INFO, "Installing both invalidpackage1 and invalidpackage2");
    assert_hd(KPM_InstallPackages(&kpm, 2, targets, &kpm_io) != KPM_OK);

    kpm_io.log(KPM_VERBOSITY_INFO, "Removing repository");
    assert_hd(KPM_RemoveRepository(&kpm, repository.id) == KPM_OK);

    kpm_io.log(KPM_VERBOSITY_INFO, "Listing repositories");
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 0);
    assert_hd(strcmp(repository.id, "testrepo") == 0);
    KPM_FreeRepository(&repository);

    KPM_Cleanup(&kpm);

    return 0;
}