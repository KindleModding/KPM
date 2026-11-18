#include "internal_utils.h"
#include "kpm.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"

int main(int argc, char *argv[])
{
    KPM *kpm = initialise_kpm();
    sqlite3_exec(kpm->db, "DELETE FROM repositories WHERE id='kindlemodding';", NULL, NULL, NULL);
    char *cur_path = get_path(argc, argv);

    char *repo_path = asprintf_hd("file://%s/resources/test_repo/manifest.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, NULL, &kpmio), KPM_OK);
    free(repo_path);

    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);

    
    return 0;
}