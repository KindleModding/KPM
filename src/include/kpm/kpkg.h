#ifndef KPKG_H
#define KPKG_H

/**
 * @file kpkg.h
 * @author Hackerdude (hackerdude@hackerdude.tech)
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "semver.h"
#include "kpmio.h"
#include <stddef.h>
#include <stdint.h>

#define KPKG_MAGIC "KPKG"
#define KPKG_MAGIC_LENGTH 4
#define KPKG_VERSION 3
#define KPKG_CHECKSUM_SIZE 32

typedef enum {
    KPKG_VERSION_1 = 1,
    KPKG_VERSION_2 = 2,
    KPKG_VERSION_3 = 3
} KPKG_Version;

typedef enum {
    KPKG_OK,
    KPKG_GENERIC_ERROR,
    KPKG_NOTIMPL_ERROR, // Feature not yet implemented
    KPKG_UNKNOWN_FORMAT,
    KPKG_VALIDATION_ERROR,
    KPKG_IO_ERROR,
    KPKG_MMAP_ERROR,
    KPKG_ZSTD_ERROR,
    KPKG_VERSION_OUT_OF_RANGE,
    KPKG_EARLY_EOF,
    KPKG_CURL_ERROR,
    KPKG_LIBARCHIVE_ERROR,
    KPKG_FILESYSTEM_ERROR,
    KPKG_OPENSSL_ERROR,
    KPKG_HASH_ERROR
} KPKG_Result;

// Global types
typedef enum {
    PLATFORM_ALL = 0, /** Any platform */
    PLATFORM_KINDLE = 1 << 0, /**< Legacy pre-K5 devices */
    PLATFORM_KINDLE5 = 1 << 1, /**< Legacy Kindle5 devices */
    PLATFORM_KINDLEPW2 = 1 << 2, /**< The Kindle PW2 and onwards on ARMLF firmware */
    PLATFORM_KINDLEHF = 1 << 3 /**< Kindles running ARMHF firmware */
} KPKG_SupportedPlatforms;

typedef struct {
    char *id;
    SemVer min_version;
    SemVer max_version;
} KPKG_DependencyEntry;

// KPKG1 types
typedef struct
{
    char *id; /**< The package ID */
    char *name; /**< The package name */
    char *author; /**< The package author */
    char *description; /**< The package description */
    SemVer package_version; /**< The package version */
    uint8_t supported_platforms; /** Platforms this package supports */
    uint32_t dependency_count; /**< Number of package dependencies */
    KPKG_DependencyEntry *dependencies; /**< A list of package dependencies */
} KPKG1_Payload;

// KPKG3 types
typedef struct __attribute__ ((packed))
{
    char magic[KPKG_MAGIC_LENGTH]; /**< "KPKG" */
    uint8_t version; /**< Format version */
    SemVer package_version; /**< Version of the package */
    uint16_t supported_platforms; /**< Bitfield of KPKG_SupportedPlatforms */
    uint32_t dependency_count; /**< Number of dependencies in package */
    uint32_t file_count; /**< Number of files in package */
    uint64_t strings_size; /**< Size of the string section */
} KPKG3_Header;

typedef struct __attribute__ ((packed))
{
    uint64_t id; /**< Pointer to string section */
    SemVer min_version; /*<< Minimum dependency version (inclusive) */
    SemVer max_version; /**< Maximum dependency version (exclusive) */
} KPKG3_DependencyEntry;

typedef struct __attribute__ ((packed))
{
    uint64_t path; /**< Pointer in string section to the file's path */
    uint8_t installed_checksum[KPKG_CHECKSUM_SIZE]; /**< The file's decompressed checksum (sha256) */
    uint64_t installed_size; /**< The size of this file once decompressed */
    uint8_t checksum[KPKG_CHECKSUM_SIZE]; /**< The file's compressed checksum (sha256) */
    uint32_t file_mode; /**< The file mode */
    uint64_t offset; /**< Pointer to compressed stream in file data section */
    uint64_t size;  /**< Size of compressed stream */
} KPKG3_FileEntry;

typedef struct
{
    void *internal_file; /**< The memory mapped file or NULL */
    size_t mapped_size; /**< This will indicate the total size of the file */
    KPKG3_Header *header; /**< Local storage of header (always present) */
    KPKG3_DependencyEntry *dependencies; /**< The package's dependencies */
    KPKG3_FileEntry *files; /**< The package's files */
    char *strings; /**< The string section */
} KPKG3_Payload;

// General types
typedef struct
{
    int kpkg_version; /**< The KPKG version */
    char *location; /**< The location of the file, either a URL or path */
    union {
        KPKG1_Payload kpkg1_payload; /**< The payload for KPKG1 and KPKG2 files */
        KPKG3_Payload kpkg3_payload; /**< The payload for KPKG3 files */
    };
} KPKG_File;

//// General functions
char *KPKG_ResultStr(KPKG_Result result);
KPKG_Result KPKG_LoadFile(KPKG_File **file, const char *location, const char *out_path, KPMIO *kpmio);
uint16_t KPKG_GetSupportedPlatforms(KPKG_File *file);
char *KPKG_GetId(KPKG_File *file);
char *KPKG_GetName(KPKG_File *file);
char *KPKG_GetAuthor(KPKG_File *file);
char *KPKG_GetDescription(KPKG_File *file);
char *KPKG_GetCategory(KPKG_File *file);
char *KPKG_GetSubcategory(KPKG_File *file); // Note: May return NULL
SemVer KPKG_GetVersion(KPKG_File *file);
uint32_t KPKG_GetDependencies(KPKG_File *file, KPKG_DependencyEntry **dependencies);
void KPKG_FreeDependencyEntry(KPKG_DependencyEntry *dependency);
void KPKG_FreeDependencyEntryList(uint32_t dependency_count, KPKG_DependencyEntry **dependencies);
KPKG_Result KPKG_Unpack(KPKG_File *file, const char *out_path, const char *cmp_path, KPMIO *kpmio);
void KPKG_CloseFile(KPKG_File *file);

#endif