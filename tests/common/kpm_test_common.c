#include "kpkg.h"
#include "kpm_test_io.h"
#include "kpm_test_common.h"
#include "internal_utils.h"
#include "kpm.h"
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>

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

char *sha256_file(const char *path)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx)
    {
        ASSERT_UNREACHABLE_HD();
    }
    if (!EVP_DigestInit_ex2(ctx, EVP_sha256(), NULL))
    {
        ASSERT_UNREACHABLE_HD()
    }
    
    FILE *f = fopen(path, "r");
    char *buffer[1024];
    size_t read_size;
    while ((read_size = fread(buffer, 1, 1024, f)) > 0)
        EVP_DigestUpdate(ctx, buffer, read_size);
    
    if (!feof(f))
    {
        ASSERT_UNREACHABLE_HD();
    }

    fclose(f);

    unsigned char cs[EVP_MAX_MD_SIZE];
    unsigned int cs_len;
    EVP_DigestFinal_ex(ctx, cs, &cs_len);
    ASSERT_HD(cs_len, KPKG_CHECKSUM_SIZE);
    EVP_MD_CTX_free(ctx);

    char *hex = asprintf_hd("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", cs[0], cs[1], cs[2], cs[3], cs[4], cs[5], cs[6], cs[7], cs[8], cs[9], cs[10], cs[11], cs[12], cs[13], cs[14], cs[15], cs[16], cs[17], cs[18], cs[19], cs[20], cs[21], cs[22], cs[23], cs[24], cs[25], cs[26], cs[27], cs[28], cs[29], cs[30], cs[31]);

    return hex;
}