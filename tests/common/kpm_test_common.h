#ifndef KPM_TEST_COMMON_H
#define KPM_TEST_COMMON_H

#include "kpm.h"
#include "openssl/evp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

KPM *initialise_kpm();
KPM *initialise_kpm_with_repos(int argc, char *argv[]);
void add_repository(KPM *kpm, char *cur_path, char *path);

char *get_path(int argc, char *argv[]);

#define GET_FORMAT(a) _Generic(a,       \
    uint64_t : "%llu",                  \
    int : "%i",                         \
    uint8_t : "%i",                     \
    uint16_t : "%i",                    \
    unsigned int : "%u",                \
    char* : "%s",                       \
    char : "%c",                        \
    long: "%li",                        \
    bool: "%i",                         \
    default : "%p"                      \
)

#define ASSERT_HD_KPKG(a, b)                                                                \
    if (a != b) {                                                                           \
        fflush(stderr);                                                                     \
        fprintf(stderr, "Assertion failed at %s:%i.\n", __FILE__, __LINE__);                \
        fprintf(stderr, "Expected: %s, got: %s\n", KPKG_ResultStr(b), KPKG_ResultStr(a));   \
        exit(1);                                                                            \
    }

#define ASSERT_HD(a, b)                                                         \
    if (a != b) {                                                               \
        fflush(stderr);                                                         \
        fprintf(stderr, "Assertion failed at %s:%i.\n", __FILE__, __LINE__);    \
        fprintf(stderr, "Expected: ");                                          \
        fprintf(stderr, GET_FORMAT(b), b);                                      \
        fprintf(stderr, ", got: ");                                             \
        fprintf(stderr, GET_FORMAT(a), a);                                      \
        fprintf(stderr, "\n");                                                  \
        exit(1);                                                                \
    }

#define ASSERT_UNREACHABLE_HD()                                                         \
        fflush(stderr);                                                         \
        fprintf(stderr, "Assertion failed: Unreachable expression reached at %s:%i.\n", __FILE__, __LINE__);    \
        fprintf(stderr, "\n");                                                  \
        exit(1);

#define ASSERT_HD_STR(a, b)                                                     \
    if (strcmp(a, b)) {                                                         \
        fflush(stderr);                                                         \
        fprintf(stderr, "Assertion failed at %s:%i.\n", __FILE__, __LINE__);    \
        fprintf(stderr, "Expected: ");                                          \
        fprintf(stderr, GET_FORMAT(b), b);                                      \
        fprintf(stderr, ", got: ");                                             \
        fprintf(stderr, GET_FORMAT(a), a);                                      \
        fprintf(stderr, "\n");                                                  \
        exit(1);                                                                \
    }

#define ASSERT_HD_SEMVER(a, b)                                                  \
    if (SemVerCmp(a, b)) {                                                      \
        fflush(stderr);                                                         \
        fprintf(stderr, "Assertion failed at %s:%i.\n", __FILE__, __LINE__);    \
        fprintf(stderr, "Expected: %u.%u.%u", b.major, b.minor, b.patch);    \
        fprintf(stderr, ", got: %u.%u.%u", a.major, a.minor, a.patch);       \
        fprintf(stderr, "\n");                                                  \
        exit(1);                                                                \
    }

#define ASSERT_HD_FILE_HASH(path, expected_hash)    \
    {                                               \
        char *hash = sha256_file(path);             \
        ASSERT_HD_STR(hash, expected_hash);         \
        free(hash);                                 \
    }                                               \

char *sha256_file(const char *path);
 
#endif