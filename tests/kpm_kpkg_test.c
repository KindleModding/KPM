#include "internal_utils.h"
#include "kpkg.h"
#include "kpm.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"
#include "kpmio.h"
#include "semver.h"
#include <stdio.h>

struct KPKGTest {
    char *path;
    KPKG_Result load_result;
    KPKG_Result curl_result;
    uint8_t kpkg_version;
    char *id;
    char *name;
    char *author;
    char *description;
    char *category;
    char *subcategory;
    SemVer version;
    bool test_dep;
    char *dep_id;
    SemVer dep_min;
    SemVer dep_max;
};

int main(int argc, char *argv[])
{
    KPM *kpm = initialise_kpm();
    sqlite3_exec(kpm->db, "DELETE FROM repositories WHERE id='kindlemodding';", NULL, NULL, NULL);
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);

    char *cur_path = get_path(argc, argv);

    KPKG_File *file;
    char *kpkg_location = asprintf_hd("file://%s/%s", cur_path, "resources/test_kpkg/kpkg3_missing_magic.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "/INACCESSIBLE/PATH/", &kpmio), KPKG_FILESYSTEM_ERROR);
    free(kpkg_location);
    ASSERT_HD(file, NULL);
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, "fakeprotocolhd://notareallocationhd", "./tmp.bin", &kpmio), KPKG_GENERIC_ERROR);
    ASSERT_HD(file, NULL);

    struct KPKGTest tests[] = {
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_bad_version_3.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_bad_version.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_dep_malformed_id.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_dep_malformed_id_type.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_dep_malformed_object.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_dep_malformed_version_2.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_dep_malformed_version.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_dep_max_version.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 1,
            .id = "kpkg1_test",
            .name = "kpkg1_test Name",
            .author = "kpkg1_test Author",
            .description = "kpkg1_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "nonexistent",
            .dep_min = SEMVER_MIN,
            .dep_max = { 1, 2, 3 }
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_dep_minmax_version.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 1,
            .id = "kpkg1_test",
            .name = "kpkg1_test Name",
            .author = "kpkg1_test Author",
            .description = "kpkg1_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "nonexistent",
            .dep_min = { 1, 2, 3 },
            .dep_max = { 4, 5, 6 }
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_dep_min_version.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 1,
            .id = "kpkg1_test",
            .name = "kpkg1_test Name",
            .author = "kpkg1_test Author",
            .description = "kpkg1_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "nonexistent",
            .dep_min = { 1, 2, 3 },
            .dep_max = SEMVER_MAX
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_dep_noversion.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 1,
            .id = "kpkg1_test",
            .name = "kpkg1_test Name",
            .author = "kpkg1_test Author",
            .description = "kpkg1_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "nonexistent",
            .dep_min = SEMVER_MIN,
            .dep_max = SEMVER_MAX
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_empty_deps.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 1,
            .id = "kpkg1_test",
            .name = "kpkg1_test Name",
            .author = "kpkg1_test Author",
            .description = "kpkg1_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_empty_supported.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 1,
            .id = "kpkg1_test",
            .name = "kpkg1_test Name",
            .author = "kpkg1_test Author",
            .description = "kpkg1_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_empty_version.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_good.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 1,
            .id = "kpkg1_test",
            .name = "kpkg1_test Name",
            .author = "kpkg1_test Author",
            .description = "kpkg1_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "test_dep",
            .dep_min = { 1, 2, 3 },
            .dep_max = { 4, 5, 6 }
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_malformed_author.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_malformed_description.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_malformed_id.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_malformed_name.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_malformed_version_1.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_malformed_version_2.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_malformed_version_3.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_malformed_version_4.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_negative_version_1.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_negative_version_2.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_negative_version_3.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_no_author.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_no_deps.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_no_description.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_no_id.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_no_name.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_no_supported.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 1,
            .id = "kpkg1_test",
            .name = "kpkg1_test Name",
            .author = "kpkg1_test Author",
            .description = "kpkg1_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg1_str_version.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },

        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_bad_version_3.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_bad_version.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_dep_malformed_id.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_dep_malformed_id_type.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_dep_malformed_object.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_dep_malformed_version_2.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_dep_malformed_version.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_dep_max_version.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 2,
            .id = "kpkg2_test",
            .name = "kpkg2_test Name",
            .author = "kpkg2_test Author",
            .description = "kpkg2_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "nonexistent",
            .dep_min = SEMVER_MIN,
            .dep_max = { 1, 2, 3 }
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_dep_minmax_version.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 2,
            .id = "kpkg2_test",
            .name = "kpkg2_test Name",
            .author = "kpkg2_test Author",
            .description = "kpkg2_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "nonexistent",
            .dep_min = { 1, 2, 3 },
            .dep_max = { 4, 5, 6 }
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_dep_min_version.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 2,
            .id = "kpkg2_test",
            .name = "kpkg2_test Name",
            .author = "kpkg2_test Author",
            .description = "kpkg2_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "nonexistent",
            .dep_min = { 1, 2, 3 },
            .dep_max = SEMVER_MAX
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_dep_noversion.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 2,
            .id = "kpkg2_test",
            .name = "kpkg2_test Name",
            .author = "kpkg2_test Author",
            .description = "kpkg2_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "nonexistent",
            .dep_min = SEMVER_MIN,
            .dep_max = SEMVER_MAX
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_empty_deps.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 2,
            .id = "kpkg2_test",
            .name = "kpkg2_test Name",
            .author = "kpkg2_test Author",
            .description = "kpkg2_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_empty_supported.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 2,
            .id = "kpkg2_test",
            .name = "kpkg2_test Name",
            .author = "kpkg2_test Author",
            .description = "kpkg2_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_empty_version.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_good.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 2,
            .id = "kpkg2_test",
            .name = "kpkg2_test Name",
            .author = "kpkg2_test Author",
            .description = "kpkg2_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = true,
            .dep_id = "test_dep",
            .dep_min = { 1, 2, 3 },
            .dep_max = { 4, 5, 6 }
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_malformed_author.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_malformed_description.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_malformed_id.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_malformed_name.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_malformed_version_1.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_malformed_version_2.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_malformed_version_3.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_malformed_version_4.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_negative_version_1.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_negative_version_2.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_negative_version_3.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_no_author.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_no_deps.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_no_description.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_no_id.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_no_name.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_no_supported.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 2,
            .id = "kpkg2_test",
            .name = "kpkg2_test Name",
            .author = "kpkg2_test Author",
            .description = "kpkg2_test Description",
            .category = "miscellaneous",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg2_str_version.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR
        },

        (struct KPKGTest) {
            .path = "resources/test_repo_src/nonexistent.kpkg",
            .load_result = KPKG_GENERIC_ERROR,
            .curl_result = KPKG_GENERIC_ERROR
        },
        (struct KPKGTest) {
            .path = "https://kindlemodding.org/404",
            .load_result = KPKG_GENERIC_ERROR,
            .curl_result = KPKG_GENERIC_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_repo_src/testpackage_1.2.3.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 3,
            .id = "testpackage",
            .name = "testpackage Name",
            .author = "testpackage Author",
            .description = "testpackage Description",
            .category = "book",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_repo_src/testpackage_4294967295.4294967295.4294967295.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 3,
            .id = "testpackage",
            .name = "testpackage Name",
            .author = "testpackage Author",
            .description = "testpackage Description",
            .category = "book",
            .subcategory = NULL,
            .version = { 4294967295, 4294967295, 4294967295 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_00_file_data.kpkg",
            .load_result = KPKG_OK, // We do not validate file data checksum until unpack
            .curl_result = KPKG_OK,
            .kpkg_version = 3,
            .id = "testpackage",
            .name = "testpackage Name",
            .author = "testpackage Author",
            .description = "testpackage Description",
            .category = "book",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_00_strings.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_category_short.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_dep.kpkg",
            .load_result = KPKG_OK, // We do not validate file data checksum until unpack
            .curl_result = KPKG_OK,
            .kpkg_version = 3,
            .id = "testpackage",
            .name = "testpackage Name",
            .author = "testpackage Author",
            .description = "testpackage Description",
            .category = "book",
            .subcategory = NULL,
            .version = { 4, 5, 6 },
            .test_dep = true,
            .dep_id = "nonexistent",
            .dep_min = SEMVER_MIN,
            .dep_max = SEMVER_MAX
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_ff_file_data.kpkg",
            .load_result = KPKG_OK, // We do not validate file data checksum until unpack
            .curl_result = KPKG_OK,
            .kpkg_version = 3,
            .id = "testpackage",
            .name = "testpackage Name",
            .author = "testpackage Author",
            .description = "testpackage Description",
            .category = "book",
            .subcategory = NULL,
            .version = { 1, 2, 3 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_ff_strings.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_file_count_0.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_five_strings.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_id_short.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_dependency_count_high.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_EARLY_EOF,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_dependency_count_low.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_dependency_count_offone.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_dependency_id_high.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_dependency_id_short.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_dependency_version_flipped.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_dependency_version_same.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_dependency_version_same2.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_file_count_offone.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_file_count.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_EARLY_EOF,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_file_dependency_count.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_EARLY_EOF,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_first_file_offset.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_last_file_offset.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_path_pointer.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_string_size_high.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_string_size_low.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_version_high.kpkg",
            .load_result = KPKG_VERSION_OUT_OF_RANGE,
            .curl_result = KPKG_VERSION_OUT_OF_RANGE,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_invalid_version_low.kpkg",
            .load_result = KPKG_VERSION_OUT_OF_RANGE,
            .curl_result = KPKG_VERSION_OUT_OF_RANGE,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_missing_magic.kpkg",
            .load_result = KPKG_UNKNOWN_FORMAT,
            .curl_result = KPKG_UNKNOWN_FORMAT,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_no_file_data.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_no_strings.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_one_string.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_subcat_icon_1.0.0.kpkg",
            .load_result = KPKG_OK,
            .curl_result = KPKG_OK,
            .kpkg_version = 3,
            .id = "test_subcat_icon",
            .name = "test_subcat_icon Name",
            .author = "test_subcat_icon Author",
            .description = "test_subcat_icon Description",
            .category = "book",
            .subcategory = "subcategory",
            .version = { 1, 0, 0 },
            .test_dep = false
        },
        (struct KPKGTest) {
            .path = "resources/test_kpkg/kpkg3_tiny.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
            .curl_result = KPKG_EARLY_EOF,
        }
    };

    for (int i=0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        for (int j=0; j < 2; j++)
        {
            struct KPKGTest test = tests[i];
            KPKG_Result expected;
            if (j == 0)
            {
                kpkg_location = asprintf_hd("%s/%s", cur_path, test.path);
                expected = test.load_result;
            }
            else
            {
                kpkg_location = asprintf_hd("file://%s/%s", cur_path, test.path);
                expected = test.curl_result;
            }

            kpmio.log(KPM_VERBOSITY_DEBUG, "Testing (%i) %s", i, kpkg_location);
            ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), expected);
            free(kpkg_location);
            if (expected != KPKG_OK)
            {
                if (expected == KPKG_GENERIC_ERROR)
                    ASSERT_HD(file, NULL);
                KPKG_CloseFile(file);
            }
            else
            {
                ASSERT_HD(file->kpkg_version, test.kpkg_version);
                ASSERT_HD_STR(KPKG_GetId(file), test.id);
                ASSERT_HD_STR(KPKG_GetName(file), test.name);
                ASSERT_HD_STR(KPKG_GetAuthor(file), test.author);
                ASSERT_HD_STR(KPKG_GetDescription(file), test.description);
                ASSERT_HD_STR(KPKG_GetCategory(file), test.category);
                if (test.subcategory == NULL)
                {
                    ASSERT_HD(KPKG_GetSubcategory(file), test.subcategory);
                }
                else
                {
                    ASSERT_HD_STR(KPKG_GetSubcategory(file), test.subcategory);
                }
                ASSERT_HD_SEMVER(KPKG_GetVersion(file), test.version);
                KPKG_DependencyEntry *dependencies;
                size_t dependency_count = KPKG_GetDependencies(file, &dependencies);
                if (test.test_dep)
                {
                    ASSERT_HD(dependency_count, 1);
                    ASSERT_HD_STR(dependencies[0].id, test.dep_id);
                    ASSERT_HD_SEMVER(dependencies[0].min_version, test.dep_min);
                    ASSERT_HD_SEMVER(dependencies[0].max_version, test.dep_max);
                }
                else
                {
                    ASSERT_HD(dependency_count, 0);
                    ASSERT_HD(dependencies, NULL);
                }
                KPKG_FreeDependencyEntryList(dependency_count, &dependencies);
                KPKG_CloseFile(file);
            }
        }
    }

    free(cur_path);
    KPM_Cleanup(kpm);
    
    return 0;
}