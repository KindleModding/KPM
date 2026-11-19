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
    uint8_t kpkg_version;
    char *id;
    char *name;
    char *author;
    char *description;
    char *category;
    char *subcategory;
    SemVer version;
};

int main(int argc, char *argv[])
{
    KPM *kpm = initialise_kpm();
    sqlite3_exec(kpm->db, "DELETE FROM repositories WHERE id='kindlemodding';", NULL, NULL, NULL);
    ASSERT_HD(KPM_UpdateIndex(kpm, &kpmio), KPM_OK);

    char *cur_path = get_path(argc, argv);

    struct KPKGTest tests[] = {
        (struct KPKGTest) {
            .path = "resources/test_repo_src/nonexistent.kpkg",
            .load_result = KPKG_GENERIC_ERROR
        },
        (struct KPKGTest) {
            .path = "https://kindlemodding.org/404",
            .load_result = KPKG_GENERIC_ERROR
        },
        (struct KPKGTest) {
            .path = "resources/test_repo_src/testpackage_1.2.3.kpkg",
            .load_result = KPKG_OK,
            .kpkg_version = 3,
            .id = "testpackage",
            .name = "testpackage Name",
            .author = "testpackage Author",
            .description = "testpackage Description",
            .category = "book",
            .subcategory = NULL,
            .version = { 1, 2, 3 }
        },
        (struct KPKGTest) {
            .path = "resources/test_repo_src/testpackage_4294967295.4294967295.4294967295.kpkg",
            .load_result = KPKG_OK,
            .kpkg_version = 3,
            .id = "testpackage",
            .name = "testpackage Name",
            .author = "testpackage Author",
            .description = "testpackage Description",
            .category = "book",
            .subcategory = NULL,
            .version = { 4294967295, 4294967295, 4294967295 }
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_00_file_data.kpkg",
            .load_result = KPKG_OK, // We do not validate file data checksum until unpack
            .kpkg_version = 3,
            .id = "testpackage",
            .name = "testpackage Name",
            .author = "testpackage Author",
            .description = "testpackage Description",
            .category = "book",
            .subcategory = NULL,
            .version = { 1, 2, 3 }
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_ff_file_data.kpkg",
            .load_result = KPKG_OK, // We do not validate file data checksum until unpack
            .kpkg_version = 3,
            .id = "testpackage",
            .name = "testpackage Name",
            .author = "testpackage Author",
            .description = "testpackage Description",
            .category = "book",
            .subcategory = NULL,
            .version = { 1, 2, 3 }
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_00_strings.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_category_short.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_ff_strings.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_file_count_0.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_five_strings.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_id_short.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_dependency_count_high.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_dependency_count_low.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_dependency_count_offone.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_dependency_id_high.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_dependency_id_short.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_file_count_offone.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_file_count.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_file_dependency_count.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_first_file_offset.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_last_file_offset.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_path_pointer.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_string_size_high.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_string_size_low.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_version_high.kpkg",
            .load_result = KPKG_VERSION_OUT_OF_RANGE,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_invalid_version_low.kpkg",
            .load_result = KPKG_VERSION_OUT_OF_RANGE,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_missing_magic.kpkg",
            .load_result = KPKG_UNKNOWN_FORMAT,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_no_file_data.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_no_strings.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        },
        (struct KPKGTest) {
            .path = "resources/malformed_kpkg/kpkg3_one_string.kpkg",
            .load_result = KPKG_VALIDATION_ERROR,
        }
    };

    for (int i=0; i < sizeof(tests)/sizeof(tests[0]); i++)
    {
        struct KPKGTest test = tests[i];
        kpmio.log(KPM_VERBOSITY_DEBUG, "Testing %s", test.path);
        KPKG_File *file;
        char *kpkg_location = asprintf_hd("%s/%s", cur_path, test.path);
        ASSERT_HD(KPKG_LoadFile(&file, kpkg_location, "./tmp/tmp.bin", &kpmio), test.load_result);
        free(kpkg_location);
        if (test.load_result != KPKG_OK)
        {
            if (test.load_result == KPKG_GENERIC_ERROR)
                ASSERT_HD(file, NULL);
            KPKG_CloseFile(file);
            continue;
        }
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
        ASSERT_HD(dependency_count, 0);
        ASSERT_HD(dependencies, NULL);
        free(dependencies);
        KPKG_CloseFile(file);
    }

    free(cur_path);
    KPM_Cleanup(kpm);
    
    return 0;
}