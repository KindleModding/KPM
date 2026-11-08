#include "kpm.h"
#include "common/kpm_test_common.h"
#include "common/kpm_test_io.h"
#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    char *actual_path = dirname(realpath(argv[0], NULL));
    fprintf(stderr, "%s\n", actual_path);

    char *valid_repo_url = asprintf_hd("file://%s/resources/test_repository/manifest.json", actual_path); // We don't need that much portability for our tests
    
    KPM kpm = initialise_kpm();
    
    kpm_io.log(KPM_VERBOSITY_INFO, "Removing default repository");
    KPM_RemoveRepository(&kpm, "kindlemodding");
    size_t repository_count;
    assert_hd(KPM_ListRepositories(&kpm, &repository_count, NULL) == KPM_OK);
    assert_hd(repository_count == 0);

    Repository repository;
    kpm_io.log(KPM_VERBOSITY_INFO, "Adding valid repository");
    assert_hd(KPM_AddRepository(&kpm, valid_repo_url, &repository, &kpm_io) == KPM_OK);
    assert_hd(KPM_ListRepositories(&kpm, &repository_count, NULL) == KPM_OK);
    assert_hd(repository_count == 1);


    
    kpm_io.log(KPM_VERBOSITY_INFO, "Removing repository");
    assert_hd(KPM_RemoveRepository(&kpm, repository.id) == KPM_OK);

    kpm_io.log(KPM_VERBOSITY_INFO, "Listing repositories");
    assert_hd(KPM_ListRepositories(&kpm, &repository_count, NULL) == KPM_OK);
    assert_hd(repository_count == 0);
    assert_hd(strcmp(repository.id, "testrepo") == 0);
    KPM_FreeRepository(&repository);

    KPM_Cleanup(&kpm);

    return 0;
}