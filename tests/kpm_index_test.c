#include "internal_utils.h"
#include "kpm.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"
#include <libgen.h>
#include <sqlite3.h>
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
    size_t artifact_count;
    KPM_Artifact *artifacts;

    sqlite3_exec(kpm->db, "INSERT INTO repositories (id, url, name, description) VALUES ('testrepo_invalid', 'https://repo.kindlemodding.org/404', 'Test Repo Invalid', 'Invalid Test Repo');", NULL, NULL, NULL);
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_GENERIC_ERROR);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo_invalid", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 0);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo_invalid"), KPM_OK);

    sqlite3_exec(kpm->db, "INSERT INTO repositories (id, url, name, description) VALUES ('testrepo_invalid', 'not_a_url', 'Test Repo Invalid', 'Invalid Test Repo');", NULL, NULL, NULL);
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_GENERIC_ERROR);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo_invalid", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 0);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo_invalid"), KPM_OK);

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

    for (int i=0; i < sizeof(invalid_repo_manifests)/sizeof(invalid_repo_manifests[0]); i++)
    {
        char *repo_path = asprintf_hd("file://%s/resources/invalid_repo_manifests/%s", cur_path, invalid_repo_manifests[i]);
        sqlite3_stmt *statement;
        sqlite3_prepare_v2(kpm->db, "INSERT INTO repositories (id, url, name, description) VALUES ('testrepo_invalid', ?, 'Test Repo Invalid', 'Invalid Test Repo');", -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, repo_path, -1, SQLITE_STATIC);
        sqlite3_step(statement);
        sqlite3_finalize(statement);
        ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_GENERIC_ERROR);
        ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo_invalid", &package_count, NULL), KPM_OK);
        ASSERT_HD(package_count, 0);
        ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo_invalid"), KPM_OK);
        free(repo_path);
    }

    fprintf(stderr, "Testing repository: %s\n", "resources/bad_repo_manifests/all_bad.json");
    add_repository(kpm, cur_path, "resources/bad_repo_manifests/all_bad.json");
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_GENERIC_ERROR);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo_bad", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 0);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo_bad"), KPM_OK);

    char *array[] = {
    "resources/bad_repo_manifests/bad_v2_v3_artifact.json",
    "resources/bad_repo_manifests/bad_v2_v3_dependency.json",
    "resources/bad_repo_manifests/bad_v2_v3_package.json",
    "resources/bad_repo_manifests/bad_v3_artifact_digest_number.json",
    "resources/bad_repo_manifests/bad_v3_artifact_install_size_string.json",
    "resources/bad_repo_manifests/bad_v3_artifact_malformed_dependencies.json",
    "resources/bad_repo_manifests/bad_v3_artifact_missing_all_v3.json",
    "resources/bad_repo_manifests/bad_v3_artifact_missing_dependencies.json",
    "resources/bad_repo_manifests/bad_v3_artifact_missing_digest.json",
    "resources/bad_repo_manifests/bad_v3_artifact_missing_install_size.json",
    "resources/bad_repo_manifests/bad_v3_artifact_missing_url.json",
    "resources/bad_repo_manifests/bad_v3_package_malformed_category.json",
    "resources/bad_repo_manifests/bad_v3_package_malformed_subcategory.json",
    "resources/bad_repo_manifests/bad_v3_package_missing_category.json",
    "resources/bad_repo_manifests/bad_v3_package_no_category.json",
    "resources/bad_repo_manifests/bad_v3_v2_artifact.json",
    "resources/bad_repo_manifests/bad_v3_v2_dependency.json",
    "resources/bad_repo_manifests/bad_v3_v2_package.json",
    "resources/bad_repo_manifests/duplicate_artifact_version.json",
    "resources/bad_repo_manifests/empty_package_artifacts.json",
    "resources/bad_repo_manifests/malformed_artifact_dependencies.json",
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
    "resources/bad_repo_manifests/missing_package_name.json"
    };

    for (int i=0; i < sizeof(array)/sizeof(array[0]); i++)
    {
        fprintf(stderr, "Testing repository: %s\n", array[i]);
        add_repository(kpm, cur_path, array[i]);
        ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_GENERIC_ERROR);
        ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo_bad", &package_count, NULL), KPM_OK);
        ASSERT_HD(package_count, 1);
        ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo_bad", &package_count, &packages), KPM_OK);
        
        ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo_bad", packages[0].id, &artifact_count, NULL), KPM_OK);
        ASSERT_HD(artifact_count, 1);
        ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo_bad", packages[0].id, &artifact_count, &artifacts), KPM_OK);
        KPM_FreeIndexedPackageList(package_count, packages);
        KPM_FreeArtifactList(artifact_count, artifacts);
        ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo_bad"), KPM_OK);
    }

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
    KPM_FreeArtifactList(artifact_count, artifacts);
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
    KPM_FreeArtifactList(artifact_count, artifacts);
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
    ASSERT_HD(packages[0].icon, NULL);
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo2", packages[0].id, &artifact_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo2", packages[0].id, &artifact_count, &artifacts), KPM_OK);
    ASSERT_HD(artifact_count, 1);
    ASSERT_HD_STR(artifacts[0].repository, "testrepo2");
    ASSERT_HD_STR(artifacts[0].id, "testpackage");
    ASSERT_HD_STR(artifacts[0].url, "packages/test/artifacts/test_0.2.0_kindlehf-kindlepw2_compat.kpkg");
    ASSERT_HD_SEMVER(artifacts[0].version, target);
    ASSERT_HD(artifacts[0].installed_size, 0);
    ASSERT_HD(artifacts[0].digest, NULL);
    KPM_FreeArtifactList(artifact_count, artifacts);
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo2", &package_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo2", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD_STR(packages[0].repository, "testrepo2");
    ASSERT_HD_STR(packages[0].id, "testpackage");
    ASSERT_HD_STR(packages[0].name, "Test Package");
    ASSERT_HD_STR(packages[0].author, "Test Author");
    ASSERT_HD_STR(packages[0].description, "Test Package Description");
    ASSERT_HD(packages[0].icon, NULL);
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo2", packages[0].id, &artifact_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo2", packages[0].id, &artifact_count, &artifacts), KPM_OK);
    ASSERT_HD(artifact_count, 1);
    ASSERT_HD_STR(artifacts[0].repository, "testrepo2");
    ASSERT_HD_STR(artifacts[0].id, "testpackage");
    ASSERT_HD_STR(artifacts[0].url, "packages/test/artifacts/test_0.2.0_kindlehf-kindlepw2_compat.kpkg");
    ASSERT_HD_SEMVER(artifacts[0].version, target);
    ASSERT_HD(artifacts[0].installed_size, 0);
    ASSERT_HD(artifacts[0].digest, NULL);
    KPM_FreeArtifactList(artifact_count, artifacts);
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo3", &package_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListRepositoryPackages(kpm, "testrepo3", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD_STR(packages[0].repository, "testrepo3");
    ASSERT_HD_STR(packages[0].id, "testpackage");
    ASSERT_HD_STR(packages[0].name, "Test Package");
    ASSERT_HD_STR(packages[0].author, "Test Author");
    ASSERT_HD_STR(packages[0].description, "Test Package Description");
    ASSERT_HD_STR(packages[0].icon, "test.png");
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo3", packages[0].id, &artifact_count, NULL), KPM_OK);
    ASSERT_HD(KPM_ListPackageArtifacts(kpm, "testrepo3", packages[0].id, &artifact_count, &artifacts), KPM_OK);
    ASSERT_HD(artifact_count, 1);
    ASSERT_HD_STR(artifacts[0].repository, "testrepo3");
    ASSERT_HD_STR(artifacts[0].id, "testpackage");
    ASSERT_HD_STR(artifacts[0].url, "packages/test/artifacts/test_0.2.0_kindlehf-kindlepw2_compat.kpkg");
    ASSERT_HD_SEMVER(artifacts[0].version, target);
    ASSERT_HD(artifacts[0].installed_size, 1024);
    ASSERT_HD_STR(artifacts[0].digest, "2e7d221c96fa73a85172ac4440fdf2bbbada6b0b3400cbf5eac07741a4680e5a");
    KPM_FreeArtifactList(artifact_count, artifacts);
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo1"), KPM_OK);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo2"), KPM_OK);
    ASSERT_HD(KPM_RemoveRepository(kpm, "testrepo3"), KPM_OK);

    free(cur_path);
    KPM_Cleanup(kpm);
    return 0;
}