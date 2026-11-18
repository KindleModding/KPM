#include "kpm.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"
#include "semver.h"

int main(int argc, char *argv[])
{
    KPM *kpm = initialise_kpm_with_repos(argc, argv);
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);

    KPM_Artifact artifact;
    SemVer version = { .major = 1, .minor = 2, .patch = 3 };
    ASSERT_HD(KPM_GetArtifact(kpm, "testrepo", "testpackage", version, &artifact), KPM_OK);
    ASSERT_HD_STR(artifact.repository, "testrepo");
    ASSERT_HD_STR(artifact.id, "testpackage");
    ASSERT_HD_STR(artifact.url, "/packages/testpackage/artifacts/testpackage_1.2.3_kindlepw2_kindlehf.kpkg");
    ASSERT_HD(artifact.installed_size, 213);
    ASSERT_HD_STR(artifact.digest, "20fe35d9cffc5caf57117fa153c3f100a0312e06a6c59e8745d8188295517d94");
    ASSERT_HD_SEMVER(artifact.version, version);
    KPM_FreeArtifact(&artifact);

    ASSERT_HD(KPM_GetArtifact(kpm, NULL, "testpackage", version, &artifact), KPM_OK);
    ASSERT_HD_STR(artifact.repository, "testrepo");
    ASSERT_HD_STR(artifact.id, "testpackage");
    ASSERT_HD_STR(artifact.url, "/packages/testpackage/artifacts/testpackage_1.2.3_kindlepw2_kindlehf.kpkg");
    ASSERT_HD(artifact.installed_size, 213);
    ASSERT_HD_STR(artifact.digest, "20fe35d9cffc5caf57117fa153c3f100a0312e06a6c59e8745d8188295517d94");
    ASSERT_HD_SEMVER(artifact.version, version);
    KPM_FreeArtifact(&artifact);

    char *cur_path = get_path(argc, argv);
    add_repository(kpm, cur_path, "resources/valid_repo_manifests/valid_v2.json");
    add_repository(kpm, cur_path, "resources/valid_repo_manifests/valid_v3.json");
    free(cur_path);
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);
    
    ASSERT_HD(KPM_GetArtifact(kpm, "testrepov2", "testpackage", version, &artifact), KPM_SQLITE_ERROR);
    version = (SemVer) { 0, 2, 0 };
    ASSERT_HD(KPM_GetArtifact(kpm, "testrepov2", "testpackage", version, &artifact), KPM_OK);
    ASSERT_HD_STR(artifact.repository, "testrepov2");
    ASSERT_HD_STR(artifact.id, "testpackage");
    ASSERT_HD_STR(artifact.url, "packages/test/artifacts/test_0.2.0_kindlehf-kindlepw2_compat.kpkg");
    ASSERT_HD(artifact.installed_size, 0);
    ASSERT_HD(artifact.digest, NULL);
    ASSERT_HD_SEMVER(artifact.version, version);
    KPM_FreeArtifact(&artifact);

    size_t dependency_count;
    KPM_Dependency *dependencies;
    version = (SemVer) { 7, 8, 9 };
    ASSERT_HD(KPM_GetArtifact(kpm, "testrepo", "testpackage", version, &artifact), KPM_OK);
    ASSERT_HD(KPM_ListArtifactDependencies(kpm, artifact.repository, artifact.id, artifact.url, &dependency_count, NULL), KPM_OK);
    ASSERT_HD(dependency_count, 4);
    ASSERT_HD(KPM_ListArtifactDependencies(kpm, artifact.repository, artifact.id, artifact.url, &dependency_count, &dependencies), KPM_OK);
    ASSERT_HD(dependency_count, 4);
    KPM_FreeArtifact(&artifact);

    for (int i=0; i < dependency_count; i++)
    {
        for (int j=0; j < dependency_count; j++)
        {
            if (j == i)
                continue;
            ASSERT_HD(strcmp(dependencies[i].id, dependencies[j].id) != 0, 1)
        }

        if (!strcmp(dependencies[i].id, "kpm"))
        {
            SemVer min_version = { 1,0,0 };
            ASSERT_HD_SEMVER(dependencies[i].min_version, min_version);
            ASSERT_HD_SEMVER(dependencies[i].max_version, SEMVER_MAX);
        }
        else if (!strcmp(dependencies[i].id, "juno"))
        {
            SemVer min_version = { 5,16,0 };
            ASSERT_HD_SEMVER(dependencies[i].min_version, min_version);
            ASSERT_HD_SEMVER(dependencies[i].max_version, SEMVER_MAX);
        }
        else if (!strcmp(dependencies[i].id, "fbink"))
        {
            SemVer min_version = { 2,3,0 };
            SemVer max_version = { 2,3,1 };
            ASSERT_HD_SEMVER(dependencies[i].min_version, min_version);
            ASSERT_HD_SEMVER(dependencies[i].max_version, max_version);
        }
        else if (!strcmp(dependencies[i].id, "testdep"))
        {
            SemVer max_version = { 4,5,6 };
            ASSERT_HD_SEMVER(dependencies[i].min_version, SEMVER_MIN);
            ASSERT_HD_SEMVER(dependencies[i].max_version, max_version);
        }
        else
        {
            ASSERT_UNREACHABLE_HD();
        }
    }

    version = (SemVer) { 4294967295, 4294967295, 4294967295 };
    ASSERT_HD(KPM_GetArtifact(kpm, "testrepo", "testpackage", version, &artifact), KPM_OK);
    ASSERT_HD_STR(artifact.repository, "testrepo");
    ASSERT_HD_STR(artifact.id, "testpackage");
    ASSERT_HD_STR(artifact.url, "/packages/testpackage/artifacts/testpackage_4294967295.4294967295.4294967295_kindlepw2_kindlehf.kpkg");
    ASSERT_HD(artifact.installed_size, 321);
    ASSERT_HD_STR(artifact.digest, "3e47c6e12a8c7b483178bd22942542b0c4725991ad64cc8acc1b5c9e520ae874");
    ASSERT_HD_SEMVER(artifact.version, version);
    KPM_FreeArtifact(&artifact);

    version = (SemVer) { 0, 0, 0 };
    ASSERT_HD(KPM_GetArtifact(kpm, "testrepo", "testpackage", version, &artifact), KPM_OK);
    ASSERT_HD_STR(artifact.repository, "testrepo");
    ASSERT_HD_STR(artifact.id, "testpackage");
    ASSERT_HD_STR(artifact.url, "/packages/testpackage/artifacts/testpackage_0.0.0_kindlepw2_kindlehf.kpkg");
    ASSERT_HD(artifact.installed_size, 213);
    ASSERT_HD_STR(artifact.digest, "e2e13f28192e5637294615cbae614fc62034fe136fbd4997863ba788d3a6b536");
    ASSERT_HD_SEMVER(artifact.version, version);
    KPM_FreeArtifact(&artifact);

    KPM_Cleanup(kpm);
    
    return 0;
}