#include "internal_utils.h"
#include "kpm.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"
#include <libgen.h>
#include <string.h>
#include <unistd.h>

void add_repository(KPM *kpm, char *cur_path, char *path)
{
    char *repo_path = asprintf_hd("file://%s/%s", cur_path, path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, NULL, &kpmio), KPM_OK);
    free(repo_path);
}

int main(int argc, char *argv[])
{
    KPM *kpm = initialise_kpm();
    sqlite3_exec(kpm->db, "DELETE FROM repositories WHERE id='kindlemodding';", NULL, NULL, NULL);
    char *cur_path = get_path(argc, argv);
    size_t package_count;
    KPM_IndexedPackage *packages;

    char *array[] = {
        "resources/bad_repo_manifests/malformed_artifact_version.json",
        "resources/bad_repo_manifests/malformed_dependency_empty.json",
        "resources/bad_repo_manifests/malformed_dependency_id.json",
        "resources/bad_repo_manifests/malformed_dependency_max.json",
        "resources/bad_repo_manifests/malformed_dependency_min.json",
        "resources/bad_repo_manifests/malformed_dependency_version.json",
        "resources/bad_repo_manifests/missing_artifact_url.json",
        "resources/bad_repo_manifests/missing_artifact_version.json",
        "resources/bad_repo_manifests/missing_package_artifacts.json",
        "resources/bad_repo_manifests/missing_package_author.json",
        "resources/bad_repo_manifests/missing_package_description.json",
        "resources/bad_repo_manifests/missing_package_name.json",
        "resources/bad_repo_manifests/negative_artifact_version.json",
        "resources/bad_repo_manifests/negative_dependency_max.json",
        "resources/bad_repo_manifests/negative_dependency_min.json"
    };

    for (int i=0; i < sizeof(array)/sizeof(array[0]); i++)
    {
        fprintf(stderr, "Testing repository: %s\n", array[i]);
        add_repository(kpm, cur_path, array[i]);
        ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_GENERIC_ERROR);
        ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo_bad", &package_count, NULL), KPM_OK);
        ASSERT_HD(package_count, 1);
        ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo_bad"), KPM_OK);
    }

    char *repo_path = asprintf_hd("file://%s/resources/bad_repo_manifests/bad_repo_version.json", cur_path);
    ASSERT_HD(KPM_AddRepository(kpm, repo_path, NULL, &kpmio), KPM_INVALID_RESPONSE_CONTENT);
    free(repo_path);


    size_t artifact_count;
    KPM_Artifact *artifacts;
    add_repository(kpm, cur_path, "resources/valid_repo_manifests/nonexistent_dependency.json");
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo", packages[0].id, &artifact_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo", packages[0].id, &artifact_count, &artifacts), KPM_OK);
    ASSERT_HD(artifact_count, 1);
    ASSERT_HD_STR(artifacts[0].repository, "testrepo");
    ASSERT_HD_STR(artifacts[0].id, "testpackage");
    ASSERT_HD_STR(artifacts[0].url, "packages/test/artifacts/test_0.2.0_kindlehf-kindlepw2_compat.kpkg");
    SemVer target = { .major=0,.minor=2,.patch=0 };
    ASSERT_HD_SEMVER(artifacts[0].version, target);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo"), KPM_OK);
    KPM_FreeIndexedPackageList(package_count, packages);

    add_repository(kpm, cur_path, "resources/valid_repo_manifests/missing_artifact_supported_platforms.json");
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 0);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo"), KPM_OK);
    KPM_FreeIndexedPackageList(package_count, packages);

    add_repository(kpm, cur_path, "resources/valid_repo_manifests/missing_artifact_dependencies.json");
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo", packages[0].id, &artifact_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo", packages[0].id, &artifact_count, &artifacts), KPM_OK);
    ASSERT_HD(artifact_count, 1);
    ASSERT_HD_STR(artifacts[0].repository, "testrepo");
    ASSERT_HD_STR(artifacts[0].id, "testpackage");
    ASSERT_HD_STR(artifacts[0].url, "packages/test/artifacts/test_0.2.0_kindlehf-kindlepw2_compat.kpkg");
    ASSERT_HD_SEMVER(artifacts[0].version, target);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo"), KPM_OK);
    KPM_FreeIndexedPackageList(package_count, packages);

    add_repository(kpm, cur_path, "resources/valid_repo_manifests/unsupported_package.json");
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo"), KPM_OK);
    KPM_FreeIndexedPackageList(package_count, packages);

    add_repository(kpm, cur_path, "resources/valid_repo_manifests/valid_v1.json");
    add_repository(kpm, cur_path, "resources/valid_repo_manifests/valid_v2.json");
    add_repository(kpm, cur_path, "resources/valid_repo_manifests/valid_v3.json");

    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);

    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo1", &package_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo1", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD_STR(packages[0].repository, "testrepo1");
    ASSERT_HD_STR(packages[0].id, "testpackage");
    ASSERT_HD_STR(packages[0].name, "Test Package");
    ASSERT_HD_STR(packages[0].author, "Test Author");
    ASSERT_HD_STR(packages[0].description, "Test Package Description");
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo2", &package_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo2", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD_STR(packages[0].repository, "testrepo2");
    ASSERT_HD_STR(packages[0].id, "testpackage");
    ASSERT_HD_STR(packages[0].name, "Test Package");
    ASSERT_HD_STR(packages[0].author, "Test Author");
    ASSERT_HD_STR(packages[0].description, "Test Package Description");
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo3", &package_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo3", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD_STR(packages[0].repository, "testrepo3");
    ASSERT_HD_STR(packages[0].id, "testpackage");
    ASSERT_HD_STR(packages[0].name, "Test Package");
    ASSERT_HD_STR(packages[0].author, "Test Author");
    ASSERT_HD_STR(packages[0].description, "Test Package Description");

    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo1"), KPM_OK);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo2"), KPM_OK);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo3"), KPM_OK);

    free(cur_path);
    return 0;
}