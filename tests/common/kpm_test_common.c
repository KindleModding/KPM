#include "kpm_test_io.h"
#include "kpm_test_common.h"
#include "internal_utils.h"
#include "kpm.h"
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

KPM *initialise_kpm()
{
    remove("./repo_test.db");
    return KPM_InitialiseAdv("./repo_test.db", "./packages", "./kpm_tmp");
}

KPM *initialise_kpm_with_repos(int argc, char *argv[])
{
    KPM *kpm = initialise_kpm();
    sqlite3_exec(kpm->db, "DELETE FROM repositories WHERE id='kindlemodding';", NULL, NULL, NULL);

    char *cur_path = get_path(argc, argv);

    char *repo_path = asprintf_hd("file://%s/resources/test_repo/manifest.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, NULL, &kpmio), KPM_OK);
    free(repo_path);

    repo_path = asprintf_hd("file://%s/resources/test_repo2/manifest.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, NULL, &kpmio), KPM_OK);
    free(repo_path);
    free(cur_path);

    return kpm;
}

void add_repository(KPM *kpm, char *cur_path, char *path)
{
    char *repo_path = asprintf_hd("file://%s/%s", cur_path, path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, NULL, &kpmio), KPM_OK);
    free(repo_path);
}

char *get_path(int argc, char *argv[])
{
    ASSERT_HD(argc > 0, 1);
    char *exec_path = strdup(argv[0]);
    char cur_path[PATH_MAX];
    realpath(dirname(exec_path), cur_path);
    free(exec_path);
    return strdup(cur_path);
}