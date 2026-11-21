#include "internal_utils.h"
#include "kpkg.h"
#include "kpm_test_common.h"
#include "kpm_test_io.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    char *cur_path = get_path(argc, argv);
    rmdir_r("./extracted");

    // Test KPKG1 unpacking
    KPKG_File *file;
    rmdir_r("./extracted");
    char *kpkg_location = asprintf_hd("file://%s/%s", cur_path, "resources/test_kpkg/kpkg1_good.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), KPKG_OK);
    free(kpkg_location);

    ASSERT_HD_KPKG(KPKG_Unpack(file, "/NONEXISTENT/PATH", NULL, &kpmio), KPKG_LIBARCHIVE_ERROR);
    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted", NULL, &kpmio), KPKG_OK);
    KPKG_CloseFile(file);

    ASSERT_HD(access("./extracted/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted/manifest.json", R_OK), 0);
    ASSERT_HD(access("./extracted/payload", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted/install.sh", "8608b37e4828e8b25439f2d3ae7350030b6500e969120b75a81459eedb60aa8b");
    ASSERT_HD_FILE_HASH("./extracted/manifest.json", "c06b654154f8ec7aece4e537b3db49be1f4e6b847adf08300fd32f4be350e3ee");
    ASSERT_HD_FILE_HASH("./extracted/payload", "76a23a1b245c5f417c275458a72ba4e7d2b5b37af565dac6968798c57dc7df95");

    // Test KPKG2 unpacking via CURL
    rmdir_r("./extracted");
    kpkg_location = asprintf_hd("file://%s/%s", cur_path, "resources/test_kpkg/kpkg2_good.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), KPKG_OK);
    free(kpkg_location);

    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted", NULL, &kpmio), KPKG_OK);
    KPKG_CloseFile(file);

    ASSERT_HD(access("./extracted/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted/manifest.json", R_OK), 0);
    ASSERT_HD(access("./extracted/payload", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted/install.sh", "8608b37e4828e8b25439f2d3ae7350030b6500e969120b75a81459eedb60aa8b");
    ASSERT_HD_FILE_HASH("./extracted/manifest.json", "a7431f711d75987045f9690a831871111361d923162a8d94e92d717f86628113");
    ASSERT_HD_FILE_HASH("./extracted/payload", "f767b86a7562879166d03cbd89ab36b3480c224f628fcd9621f6b095a2b76636");

    // Test KPKG2 unpacking via local
    rmdir_r("./extracted");
    kpkg_location = asprintf_hd("%s/%s", cur_path, "resources/test_kpkg/kpkg2_good.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), KPKG_OK);
    free(kpkg_location);

    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted", NULL, &kpmio), KPKG_OK);
    KPKG_CloseFile(file);

    ASSERT_HD(access("./extracted/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted/manifest.json", R_OK), 0);
    ASSERT_HD(access("./extracted/payload", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted/install.sh", "8608b37e4828e8b25439f2d3ae7350030b6500e969120b75a81459eedb60aa8b");
    ASSERT_HD_FILE_HASH("./extracted/manifest.json", "a7431f711d75987045f9690a831871111361d923162a8d94e92d717f86628113");
    ASSERT_HD_FILE_HASH("./extracted/payload", "f767b86a7562879166d03cbd89ab36b3480c224f628fcd9621f6b095a2b76636");

    // Test KPKG3 unpacking via CURL
    rmdir_r("./extracted");
    rmdir_r("./extracted2");
    kpkg_location = asprintf_hd("file://%s/%s", cur_path, "resources/test_repo_src/testpackage_1.2.3.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), KPKG_OK);
    free(kpkg_location);

    ASSERT_HD_KPKG(KPKG_Unpack(file, "/NONEXISTENT/PATH", NULL, &kpmio), KPKG_FILESYSTEM_ERROR);
    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted", NULL, &kpmio), KPKG_OK);

    ASSERT_HD(access("./extracted/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted/launch.sh", R_OK), 0);
    ASSERT_HD(access("./extracted/startup.sh", R_OK), 0);
    ASSERT_HD(access("./extracted/uninstall.sh", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted/install.sh", "0a305882feaa4e5f43cf99593ed663b3248d1ef828ed9742167e2338b3816367");
    ASSERT_HD_FILE_HASH("./extracted/launch.sh", "66eed44e2da648a0d2eb9547ffc51fb49074717d7feac9d0ea04eb4d9985e419");
    ASSERT_HD_FILE_HASH("./extracted/startup.sh", "cc02635183fa229900c5bd1f1d725f86d1a90a055abadb0f24d93479db50c27d");
    ASSERT_HD_FILE_HASH("./extracted/uninstall.sh", "ed0541d62fd6b3058b57747667a797bc5c427fd37914607b2072cc92ba381e62");

    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted2", "./extracted", &kpmio), KPKG_OK);
    ASSERT_HD(access("./extracted2/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/launch.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/startup.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/uninstall.sh", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted2/install.sh", "0a305882feaa4e5f43cf99593ed663b3248d1ef828ed9742167e2338b3816367");
    ASSERT_HD_FILE_HASH("./extracted2/launch.sh", "66eed44e2da648a0d2eb9547ffc51fb49074717d7feac9d0ea04eb4d9985e419");
    ASSERT_HD_FILE_HASH("./extracted2/startup.sh", "cc02635183fa229900c5bd1f1d725f86d1a90a055abadb0f24d93479db50c27d");
    ASSERT_HD_FILE_HASH("./extracted2/uninstall.sh", "ed0541d62fd6b3058b57747667a797bc5c427fd37914607b2072cc92ba381e62");

    remove("./extracted/launch.sh");
    remove("./extracted/uninstall.sh");

    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted2", "./extracted", &kpmio), KPKG_OK);
    ASSERT_HD(access("./extracted2/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/launch.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/startup.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/uninstall.sh", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted2/install.sh", "0a305882feaa4e5f43cf99593ed663b3248d1ef828ed9742167e2338b3816367");
    ASSERT_HD_FILE_HASH("./extracted2/launch.sh", "66eed44e2da648a0d2eb9547ffc51fb49074717d7feac9d0ea04eb4d9985e419");
    ASSERT_HD_FILE_HASH("./extracted2/startup.sh", "cc02635183fa229900c5bd1f1d725f86d1a90a055abadb0f24d93479db50c27d");
    ASSERT_HD_FILE_HASH("./extracted2/uninstall.sh", "ed0541d62fd6b3058b57747667a797bc5c427fd37914607b2072cc92ba381e62");
    KPKG_CloseFile(file);


    // Test KPKG3 unpacking via local
    rmdir_r("./extracted");
    rmdir_r("./extracted2");
    kpkg_location = asprintf_hd("%s/%s", cur_path, "resources/test_repo_src/testpackage_1.2.3.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), KPKG_OK);
    free(kpkg_location);

    ASSERT_HD_KPKG(KPKG_Unpack(file, "/NONEXISTENT/PATH", NULL, &kpmio), KPKG_FILESYSTEM_ERROR);
    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted", NULL, &kpmio), KPKG_OK);

    ASSERT_HD(access("./extracted/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted/launch.sh", R_OK), 0);
    ASSERT_HD(access("./extracted/startup.sh", R_OK), 0);
    ASSERT_HD(access("./extracted/uninstall.sh", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted/install.sh", "0a305882feaa4e5f43cf99593ed663b3248d1ef828ed9742167e2338b3816367");
    ASSERT_HD_FILE_HASH("./extracted/launch.sh", "66eed44e2da648a0d2eb9547ffc51fb49074717d7feac9d0ea04eb4d9985e419");
    ASSERT_HD_FILE_HASH("./extracted/startup.sh", "cc02635183fa229900c5bd1f1d725f86d1a90a055abadb0f24d93479db50c27d");
    ASSERT_HD_FILE_HASH("./extracted/uninstall.sh", "ed0541d62fd6b3058b57747667a797bc5c427fd37914607b2072cc92ba381e62");

    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted2", "./extracted", &kpmio), KPKG_OK);
    ASSERT_HD(access("./extracted2/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/launch.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/startup.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/uninstall.sh", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted2/install.sh", "0a305882feaa4e5f43cf99593ed663b3248d1ef828ed9742167e2338b3816367");
    ASSERT_HD_FILE_HASH("./extracted2/launch.sh", "66eed44e2da648a0d2eb9547ffc51fb49074717d7feac9d0ea04eb4d9985e419");
    ASSERT_HD_FILE_HASH("./extracted2/startup.sh", "cc02635183fa229900c5bd1f1d725f86d1a90a055abadb0f24d93479db50c27d");
    ASSERT_HD_FILE_HASH("./extracted2/uninstall.sh", "ed0541d62fd6b3058b57747667a797bc5c427fd37914607b2072cc92ba381e62");

    remove("./extracted/launch.sh");
    remove("./extracted/uninstall.sh");

    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted2", "./extracted", &kpmio), KPKG_OK);
    ASSERT_HD(access("./extracted2/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/launch.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/startup.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/uninstall.sh", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted2/install.sh", "0a305882feaa4e5f43cf99593ed663b3248d1ef828ed9742167e2338b3816367");
    ASSERT_HD_FILE_HASH("./extracted2/launch.sh", "66eed44e2da648a0d2eb9547ffc51fb49074717d7feac9d0ea04eb4d9985e419");
    ASSERT_HD_FILE_HASH("./extracted2/startup.sh", "cc02635183fa229900c5bd1f1d725f86d1a90a055abadb0f24d93479db50c27d");
    ASSERT_HD_FILE_HASH("./extracted2/uninstall.sh", "ed0541d62fd6b3058b57747667a797bc5c427fd37914607b2072cc92ba381e62");

    FILE *f = fopen("./extracted2/install.sh", "w");
    fwrite("Hi!", 1, 3, f);
    fclose(f);

    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted2", "./extracted2", &kpmio), KPKG_OK);
    ASSERT_HD(access("./extracted2/install.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/launch.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/startup.sh", R_OK), 0);
    ASSERT_HD(access("./extracted2/uninstall.sh", R_OK), 0);
    ASSERT_HD_FILE_HASH("./extracted2/install.sh", "0a305882feaa4e5f43cf99593ed663b3248d1ef828ed9742167e2338b3816367");
    ASSERT_HD_FILE_HASH("./extracted2/launch.sh", "66eed44e2da648a0d2eb9547ffc51fb49074717d7feac9d0ea04eb4d9985e419");
    ASSERT_HD_FILE_HASH("./extracted2/startup.sh", "cc02635183fa229900c5bd1f1d725f86d1a90a055abadb0f24d93479db50c27d");
    ASSERT_HD_FILE_HASH("./extracted2/uninstall.sh", "ed0541d62fd6b3058b57747667a797bc5c427fd37914607b2072cc92ba381e62");
    KPKG_CloseFile(file);

    rmdir_r("./extracted");
    kpkg_location = asprintf_hd("file://%s/%s", cur_path, "resources/test_kpkg/kpkg3_hash_mismatch.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), KPKG_OK);
    free(kpkg_location);
    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted", NULL, &kpmio), KPKG_HASH_ERROR);
    KPKG_CloseFile(file);

    rmdir_r("./extracted");
    kpkg_location = asprintf_hd("%s/%s", cur_path, "resources/test_kpkg/kpkg3_hash_mismatch.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), KPKG_OK);
    free(kpkg_location);
    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted", NULL, &kpmio), KPKG_HASH_ERROR);
    KPKG_CloseFile(file);

    remove("./extracted2/startup.sh");

    rmdir_r("./extracted");
    kpkg_location = asprintf_hd("file://%s/%s", cur_path, "resources/test_kpkg/kpkg3_hash_mismatch.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), KPKG_OK);
    free(kpkg_location);
    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted", "./extracted2", &kpmio), KPKG_HASH_ERROR);
    KPKG_CloseFile(file);

    rmdir_r("./extracted");
    kpkg_location = asprintf_hd("%s/%s", cur_path, "resources/test_kpkg/kpkg3_hash_mismatch.kpkg");
    ASSERT_HD_KPKG(KPKG_LoadFile(&file, kpkg_location, "./tmp.bin", &kpmio), KPKG_OK);
    free(kpkg_location);
    ASSERT_HD_KPKG(KPKG_Unpack(file, "./extracted", "./extracted2", &kpmio), KPKG_HASH_ERROR);
    KPKG_CloseFile(file);

    free(cur_path);
    return 0;
}