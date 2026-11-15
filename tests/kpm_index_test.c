#include "internal_utils.h"
#include "kpm.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"
#include <libgen.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    KPM *kpm = initialise_kpm();
    char *cur_path = get_path(argc, argv);

    char *repo_path = asprintf_hd("file://%s/resources/valid_repo_manifests/valid_v1.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, NULL, &kpmio), KPM_OK);
    free(repo_path);


    repo_path = asprintf_hd("file://%s/resources/valid_repo_manifests/valid_v2.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, NULL, &kpmio), KPM_OK);
    free(repo_path);


    repo_path = asprintf_hd("file://%s/resources/valid_repo_manifests/valid_v3.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, NULL, &kpmio), KPM_OK);
    free(repo_path);

    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);

    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo1"), KPM_OK);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo2"), KPM_OK);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo3"), KPM_OK);

    free(cur_path);
    return 0;
}