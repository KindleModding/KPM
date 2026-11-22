#include "internal_utils.h"
#include "kpm.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"
#include "kpmio.h"

int main(int argc, char *argv[])
{
    KPM *kpm = initialise_kpm_with_repos(argc, argv);
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);

    KPM_IndexedPackage package;
    ASSERT_HD(KPM_GetPackage(kpm, "testrepo", "package_does_not_exist", &package), KPM_SQLITE_ERROR);
    ASSERT_HD(KPM_GetPackage(kpm, NULL, "package_does_not_exist", &package), KPM_SQLITE_ERROR);

    ASSERT_HD(KPM_GetPackage(kpm, "testrepo", "testsearch", &package), KPM_OK);
    ASSERT_HD_STR(package.repository, "testrepo");
    ASSERT_HD_STR(package.id, "testsearch");
    ASSERT_HD_STR(package.name, "testsearch Name");
    ASSERT_HD_STR(package.author, "testsearch Author");
    ASSERT_HD_STR(package.description, "testsearch Description");
    ASSERT_HD_STR(package.category, "book");
    ASSERT_HD(package.subcategory, NULL);
    ASSERT_HD(package.icon, NULL);
    ASSERT_HD(package.changelog, NULL);
    KPM_FreeIndexedPackage(&package);

    ASSERT_HD(KPM_GetPackage(kpm, "testrepo2", "testsearch", &package), KPM_OK);
    ASSERT_HD_STR(package.repository, "testrepo2");
    ASSERT_HD_STR(package.id, "testsearch");
    ASSERT_HD_STR(package.name, "testsearch Name");
    ASSERT_HD_STR(package.author, "testsearch Author");
    ASSERT_HD_STR(package.description, "testsearch Description");
    ASSERT_HD_STR(package.category, "book");
    ASSERT_HD(package.subcategory, NULL);
    ASSERT_HD(package.icon, NULL);
    ASSERT_HD(package.changelog, NULL);
    KPM_FreeIndexedPackage(&package);

    ASSERT_HD(KPM_GetPackage(kpm, NULL, "testsearch", &package), KPM_OK);
    if (strcmp(package.repository, "testrepo") && strcmp(package.repository, "testrepo2"))
    {
        kpmio.log(KPM_VERBOSITY_ERROR, "Got repository: %s", package.repository);
        ASSERT_UNREACHABLE_HD();
    }
    ASSERT_HD_STR(package.id, "testsearch");
    ASSERT_HD_STR(package.name, "testsearch Name");
    ASSERT_HD_STR(package.author, "testsearch Author");
    ASSERT_HD_STR(package.description, "testsearch Description");
    ASSERT_HD_STR(package.category, "book");
    ASSERT_HD(package.subcategory, NULL);
    ASSERT_HD(package.icon, NULL);
    ASSERT_HD(package.changelog, NULL);
    KPM_FreeIndexedPackage(&package);

    ASSERT_HD(KPM_GetPackage(kpm, NULL, "test_subcat_icon", &package), KPM_OK);
    ASSERT_HD_STR(package.repository, "testrepo2");
    ASSERT_HD_STR(package.id, "test_subcat_icon");
    ASSERT_HD_STR(package.name, "test_subcat_icon Name");
    ASSERT_HD_STR(package.author, "test_subcat_icon Author");
    ASSERT_HD_STR(package.description, "test_subcat_icon Description");
    ASSERT_HD_STR(package.category, "book");
    ASSERT_HD_STR(package.subcategory, "subcategory");
    ASSERT_HD_STR(package.icon, "/packages/test_subcat_icon/icon.png");
    ASSERT_HD(package.changelog, NULL);
    KPM_FreeIndexedPackage(&package);

    size_t package_count;
    KPM_IndexedPackage *packages;
    ASSERT_HD(KPM_GetPackages(kpm, "test_subcat_icon", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD(KPM_GetPackages(kpm, "test_subcat_icon", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD_STR(packages[0].repository, "testrepo2");
    ASSERT_HD_STR(packages[0].id, "test_subcat_icon");
    ASSERT_HD_STR(packages[0].name, "test_subcat_icon Name");
    ASSERT_HD_STR(packages[0].author, "test_subcat_icon Author");
    ASSERT_HD_STR(packages[0].description, "test_subcat_icon Description");
    ASSERT_HD_STR(packages[0].category, "book");
    ASSERT_HD_STR(packages[0].subcategory, "subcategory");
    ASSERT_HD_STR(packages[0].icon, "/packages/test_subcat_icon/icon.png");
    ASSERT_HD(package.changelog, NULL);
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_GetPackages(kpm, "package_does_not_exist", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 0);
    ASSERT_HD(KPM_GetPackages(kpm, "package_does_not_exist", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 0);
    KPM_FreeIndexedPackageList(package_count, packages);
    ASSERT_HD(KPM_GetPackages(kpm, "testsearch", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(KPM_GetPackages(kpm, "testsearch", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(strcmp(packages[0].repository, packages[1].repository) != 0, 1);
    ASSERT_HD_STR(packages[0].id, "testsearch");
    ASSERT_HD_STR(packages[0].name, "testsearch Name");
    ASSERT_HD_STR(packages[0].author, "testsearch Author");
    ASSERT_HD_STR(packages[0].description, "testsearch Description");
    ASSERT_HD_STR(packages[0].category, "book");
    ASSERT_HD(packages[0].subcategory, NULL);
    ASSERT_HD(packages[0].icon, NULL);
    ASSERT_HD(package.changelog, NULL);
    ASSERT_HD_STR(packages[0].id, packages[1].id);
    ASSERT_HD_STR(packages[0].name, packages[1].name);
    ASSERT_HD_STR(packages[0].author, packages[1].author);
    ASSERT_HD_STR(packages[0].description, packages[1].description);
    ASSERT_HD_STR(packages[0].category, packages[1].category);
    ASSERT_HD(packages[0].subcategory, packages[1].subcategory);
    ASSERT_HD(packages[0].icon, packages[1].icon);
    ASSERT_HD(package.changelog, NULL);
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_SearchPackages(kpm, "estsearch", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(KPM_SearchPackages(kpm, "estsearc", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(KPM_SearchPackages(kpm, "testsearch", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(KPM_SearchPackages(kpm, "estsearch", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(KPM_SearchPackages(kpm, "estsearch", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(strcmp(packages[0].repository, packages[1].repository) != 0, 1);
    ASSERT_HD_STR(packages[0].id, "testsearch");
    ASSERT_HD_STR(packages[0].name, "testsearch Name");
    ASSERT_HD_STR(packages[0].author, "testsearch Author");
    ASSERT_HD_STR(packages[0].description, "testsearch Description");
    ASSERT_HD_STR(packages[0].category, "book");
    ASSERT_HD(packages[0].subcategory, NULL);
    ASSERT_HD(packages[0].icon, NULL);
    ASSERT_HD(package.changelog, NULL);
    ASSERT_HD_STR(packages[0].id, packages[1].id);
    ASSERT_HD_STR(packages[0].name, packages[1].name);
    ASSERT_HD_STR(packages[0].author, packages[1].author);
    ASSERT_HD_STR(packages[0].description, packages[1].description);
    ASSERT_HD_STR(packages[0].category, packages[1].category);
    ASSERT_HD(packages[0].subcategory, packages[1].subcategory);
    ASSERT_HD(packages[0].icon, packages[1].icon);
    ASSERT_HD(package.changelog, NULL);
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_SearchPackages(kpm, "testsearch Name", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(KPM_SearchPackages(kpm, "estsearch Name", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(KPM_SearchPackages(kpm, "estsearch Nam", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(KPM_SearchPackages(kpm, "testsearch Nam", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(KPM_SearchPackages(kpm, "testsearch Nam", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 2);
    ASSERT_HD(strcmp(packages[0].repository, packages[1].repository) != 0, 1);
    ASSERT_HD_STR(packages[0].id, "testsearch");
    ASSERT_HD_STR(packages[0].name, "testsearch Name");
    ASSERT_HD_STR(packages[0].author, "testsearch Author");
    ASSERT_HD_STR(packages[0].description, "testsearch Description");
    ASSERT_HD_STR(packages[0].category, "book");
    ASSERT_HD(packages[0].subcategory, NULL);
    ASSERT_HD(package.changelog, NULL);
    ASSERT_HD(packages[0].icon, NULL);
    ASSERT_HD_STR(packages[0].id, packages[1].id);
    ASSERT_HD_STR(packages[0].name, packages[1].name);
    ASSERT_HD_STR(packages[0].author, packages[1].author);
    ASSERT_HD_STR(packages[0].description, packages[1].description);
    ASSERT_HD_STR(packages[0].category, packages[1].category);
    ASSERT_HD(packages[0].subcategory, packages[1].subcategory);
    ASSERT_HD(packages[0].icon, packages[1].icon);
    ASSERT_HD(package.changelog, NULL);
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_SearchPackages(kpm, "test_subcat_icon", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD(KPM_SearchPackages(kpm, "test_subcat_icon", &package_count, &packages), KPM_OK);
    ASSERT_HD(package_count, 1);
    ASSERT_HD_STR(packages[0].repository, "testrepo2");
    ASSERT_HD_STR(packages[0].id, "test_subcat_icon");
    ASSERT_HD_STR(packages[0].name, "test_subcat_icon Name");
    ASSERT_HD_STR(packages[0].author, "test_subcat_icon Author");
    ASSERT_HD_STR(packages[0].description, "test_subcat_icon Description");
    ASSERT_HD_STR(packages[0].category, "book");
    ASSERT_HD_STR(packages[0].subcategory, "subcategory");
    ASSERT_HD_STR(packages[0].icon, "/packages/test_subcat_icon/icon.png");
    ASSERT_HD(package.changelog, NULL);
    KPM_FreeIndexedPackageList(package_count, packages);

    ASSERT_HD(KPM_SearchPackages(kpm, "package_does_not_exist", &package_count, NULL), KPM_OK);
    ASSERT_HD(package_count, 0);

    size_t artifact_count;
    KPM_Artifact *artifacts;
    KPM_ListPackageArtifacts(kpm, NULL, "package_does_not_exist", &artifact_count, NULL);
    ASSERT_HD(artifact_count, 0);
    KPM_ListPackageArtifacts(kpm, NULL, "package_does_not_exist", &artifact_count, &artifacts);
    ASSERT_HD(artifact_count, 0);
    KPM_ListPackageArtifacts(kpm, NULL, "testsearch", &artifact_count, NULL);
    ASSERT_HD(artifact_count, 2);
    KPM_ListPackageArtifacts(kpm, NULL, "testsearch", &artifact_count, &artifacts);
    ASSERT_HD(artifact_count, 2);
    ASSERT_HD(strcmp(artifacts[0].repository, artifacts[1].repository) != 0, 1);
    ASSERT_HD_STR(artifacts[0].id, "testsearch");
    ASSERT_HD_STR(artifacts[0].url, "/packages/testsearch/artifacts/testsearch_1.2.3_kindlepw2_kindlehf.kpkg");
    ASSERT_HD(artifacts[0].installed_size, 209);
    ASSERT_HD_STR(artifacts[0].digest, "64e00f1d0b6cbff02740a87c9c2b03cde94024a3b4e9db4e3b03633e4fc5704c");
    ASSERT_HD(artifacts[0].version.major, 1);
    ASSERT_HD(artifacts[0].version.minor, 2);
    ASSERT_HD(artifacts[0].version.patch, 3);
    ASSERT_HD_STR(artifacts[0].id, artifacts[1].id);
    ASSERT_HD_STR(artifacts[0].url, artifacts[1].url);
    ASSERT_HD(artifacts[0].installed_size, artifacts[1].installed_size);
    ASSERT_HD_STR(artifacts[0].digest, artifacts[1].digest);
    ASSERT_HD(artifacts[0].version.major, artifacts[1].version.major);
    ASSERT_HD(artifacts[0].version.minor, artifacts[1].version.minor);
    ASSERT_HD(artifacts[0].version.patch, artifacts[1].version.patch);
    KPM_FreeArtifactList(artifact_count, artifacts);

    KPM_Cleanup(kpm);
    
    return 0;
}