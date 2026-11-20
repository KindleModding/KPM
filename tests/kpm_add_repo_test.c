#include "internal_utils.h"
#include "kpm.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"
#include <libgen.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    KPM *kpm = initialise_kpm();
    char *cur_path = get_path(argc, argv);

    size_t repository_count;
    KPM_Repository *repositories;
    ASSERT_HD(KPM_ListRepositories(kpm, &repository_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListRepositories(kpm, &repository_count, &repositories), KPM_OK);
    ASSERT_HD(repository_count, 1);

    ASSERT_HD_STR(repositories[0].id, "kindlemodding");
    ASSERT_HD_STR(repositories[0].url, "https://repo.kindlemodding.org/manifest.v3.json");
    ASSERT_HD_STR(repositories[0].name, "Official KMC Repo");
    ASSERT_HD_STR(repositories[0].description, "The official KMC repo");
    KPM_FreeRepositoryList(repository_count, repositories);

    KPM_Repository repository;
    ASSERT_HD(KPM_GetRepository(kpm, "kindlemodding", &repository), KPM_OK);
    ASSERT_HD_STR(repository.id, "kindlemodding");
    ASSERT_HD_STR(repository.url, "https://repo.kindlemodding.org/manifest.v3.json");
    ASSERT_HD_STR(repository.name, "Official KMC Repo");
    ASSERT_HD_STR(repository.description, "The official KMC repo");
    ASSERT_HD(KPM_RemoveRepository(kpm, "kindlemodding"), KPM_GENERIC_ERROR);
    KPM_FreeRepository(&repository);
    ASSERT_HD(KPM_GetRepository(kpm, "notfound", &repository), KPM_SQLITE_ERROR);

    ASSERT_HD(KPM_AddRepository(kpm, "", &repository, &kpmio), KPM_CURL_ERROR)
    ASSERT_HD(KPM_AddRepository(kpm, "https://repo.kindlemodding.org/404", &repository, &kpmio), KPM_INVALID_RESPONSE_CODE);
    ASSERT_HD(KPM_AddRepository(kpm, "https://repo.kindlemodding.org/manifest.json", &repository, &kpmio), KPM_SQLITE_ERROR);
    ASSERT_HD(KPM_AddRepository(kpm, "https://repo.kindlemodding.org/manifest.v2.json", &repository, &kpmio), KPM_SQLITE_ERROR);

    char *repo_path = asprintf_hd("file://%s/resources/invalid_repo_manifests/empty_id.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, &repository, &kpmio), KPM_INVALID_RESPONSE_CONTENT);
    free(repo_path);

    char *invalid_repo_manifests[] = {
        "bad_repo_version.json",
        "empty_id.json",
        "malformed_repo_version.json",
        "missing_repo_version.json",
        "negative_repo_version.json",
        "no_description.json",
        "no_id.json",
        "no_name.json",
        "not_json.txt"
    };

    KPM_Result invalid_repo_manifests_responses[] = {
        KPM_INVALID_RESPONSE_CONTENT,
        KPM_INVALID_RESPONSE_CONTENT,
        KPM_INVALID_RESPONSE_CONTENT,
        KPM_INVALID_RESPONSE_CONTENT,
        KPM_INVALID_RESPONSE_CONTENT,
        KPM_INVALID_RESPONSE_CONTENT,
        KPM_INVALID_RESPONSE_CONTENT,
        KPM_INVALID_RESPONSE_CONTENT,
        KPM_PARSE_ERROR
    };

    for (int i=0; i < sizeof(invalid_repo_manifests)/sizeof(invalid_repo_manifests[0]); i++)
    {
        fprintf(stderr, "Adding repo %s\n", invalid_repo_manifests[i]);
        repo_path = asprintf_hd("file://%s/resources/invalid_repo_manifests/%s", cur_path, invalid_repo_manifests[i]);
        ASSERT_HD(KPM_AddRepository(kpm, repo_path, &repository, &kpmio), invalid_repo_manifests_responses[i]);
        free(repo_path);
    }

    repo_path = asprintf_hd("file://%s/resources/valid_repo_manifests/valid_v1.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, &repository, &kpmio), KPM_OK);
    ASSERT_HD_STR(repository.id, "testrepov1");
    ASSERT_HD_STR(repository.url, repo_path);
    ASSERT_HD_STR(repository.name, "Test Repo");
    ASSERT_HD_STR(repository.description, "The test repo");
    KPM_FreeRepository(&repository);

    ASSERT_HD(KPM_ListRepositories(kpm, &repository_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListRepositories(kpm, &repository_count, &repositories), KPM_OK);
    ASSERT_HD(repository_count, 2);
    ASSERT_HD_STR(repositories[1].id, "testrepov1");
    ASSERT_HD_STR(repositories[1].url, repo_path);
    ASSERT_HD_STR(repositories[1].name, "Test Repo");
    ASSERT_HD_STR(repositories[1].description, "The test repo");
    KPM_FreeRepositoryList(repository_count, repositories);
    free(repo_path);

    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepov1"), KPM_OK);


    repo_path = asprintf_hd("file://%s/resources/valid_repo_manifests/valid_v2.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, &repository, &kpmio), KPM_OK);
    ASSERT_HD_STR(repository.id, "testrepov2");
    ASSERT_HD_STR(repository.url, repo_path);
    ASSERT_HD_STR(repository.name, "Test Repo 2");
    ASSERT_HD_STR(repository.description, "The test repo 2");
    KPM_FreeRepository(&repository);

    ASSERT_HD(KPM_ListRepositories(kpm, &repository_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListRepositories(kpm, &repository_count, &repositories), KPM_OK);
    ASSERT_HD(repository_count, 2);
    ASSERT_HD_STR(repositories[1].id, "testrepov2");
    ASSERT_HD_STR(repositories[1].url, repo_path);
    ASSERT_HD_STR(repositories[1].name, "Test Repo 2");
    ASSERT_HD_STR(repositories[1].description, "The test repo 2");
    KPM_FreeRepositoryList(repository_count, repositories);
    free(repo_path);

    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepov2"), KPM_OK);


    repo_path = asprintf_hd("file://%s/resources/valid_repo_manifests/valid_v3.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, &repository, &kpmio), KPM_OK);
    ASSERT_HD_STR(repository.id, "testrepov3");
    ASSERT_HD_STR(repository.url, repo_path);
    ASSERT_HD_STR(repository.name, "Test Repo 3");
    ASSERT_HD_STR(repository.description, "The test repo 3");
    KPM_FreeRepository(&repository);

    ASSERT_HD(KPM_ListRepositories(kpm, &repository_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListRepositories(kpm, &repository_count, &repositories), KPM_OK);
    ASSERT_HD(repository_count, 2);
    ASSERT_HD_STR(repositories[1].id, "testrepov3");
    ASSERT_HD_STR(repositories[1].url, repo_path);
    ASSERT_HD_STR(repositories[1].name, "Test Repo 3");
    ASSERT_HD_STR(repositories[1].description, "The test repo 3");
    KPM_FreeRepositoryList(repository_count, repositories);
    free(repo_path);

    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepov3"), KPM_OK);

    ASSERT_HD(KPM_RemoveRepository(kpm, ""), KPM_OK);

    KPM_Cleanup(kpm);
    free(cur_path);
    return 0;
}