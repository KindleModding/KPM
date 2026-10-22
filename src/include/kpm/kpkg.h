#pragma once

#include "kpm/semver.h"
#include <stddef.h>
#include <stdint.h>

#define KPKG_MAGIC "KPKG"
#define KPKG_MAGIC_LENGTH 4
#define KPKG_VERSION 3

typedef enum {
    KPKG_OK,
    KPKG_FS_ERROR,
    KPKG_MMAP_ERROR,
    KPKG_ZSTD_ERROR
} KPKG_Result;

typedef enum {
    KINDLEANY = 0, /** Any Kindle */
    KINDLE = 1 << 0, /**< Legacy pre-K5 devices */
    KINDLE5 = 1 << 1, /**< Legacy Kindle5 devices */
    KINDLEPW2 = 1 << 2, /**< The Kindle PW2 and onwards on ARMLF firmware */
    KINDLEHF = 1 << 3 /**< Kindles running ARMHF firmware */
} KPKG_SupportedPlatforms;

typedef struct
{
    char magic[KPKG_MAGIC_LENGTH]; /**< "KPKG" */
    uint8_t version; /**< Format version */
    SemVer package_version; /**< Version of the package */
    uint8_t supported_platforms; /**< Bitfield of KPKG_SupportedPlatforms */
    uint64_t installed_size; /**< Total size of installed package */
    uint32_t dependency_count; /**< Number of dependencies in package */
    uint32_t file_count; /**< Number of files in package */
    uint64_t strings_size; /**< Size of the string section */
} KPKG_Header;

typedef struct
{
    uint64_t id; /**< Pointer to string section */
    SemVer min_version; /*<< Minimum dependency version (inclusive) */
    SemVer max_version; /**< Maximum dependency version (exclusive) */
} KPKG_DependencyEntry;

typedef struct
{
    uint64_t path; /**< Pointer in string section to the file's path */
    uint8_t checksum[16]; /**< The file's compressed checksum (sha256) */
    uint32_t file_mode; /**< The file mode */
    uint64_t installed_size; /**< The size of this file once decompressed */
    uint64_t offset; /**< Pointer to compressed stream in file data section */
} KPKG_FileEntry;

typedef struct
{
    char* location; /**< The URL/path to the file */
    void* internal_file; /**< The memory mapped file or NULL */
    size_t map_size; /**< If file is local and memory mapped, this will indicate the size */
    KPKG_Header* header; /**< Local storage of header (always present) */
    KPKG_DependencyEntry* dependencies; /**< The package's dependencies */
    KPKG_FileEntry* files; /**< The package's files */
    char* strings; /**< The string section */
} KPKG_File;

/**
 * @brief Load a KPKG_File from the specified location
 * 
 * @param location The URL or path to the KPKG file
 * @return KPKG_File* A pointer to the KPKG_File file object if valid, otherwise NULL
 */
KPKG_File* KPKG_LoadFile(const char* location);

void KPKG_CloseFile(KPKG_File* file);

/**
 * @brief Validate a KPKG_File object such that none of the pointers are OOB, note: this function assumes that the entry and string arrays are not malformed.
 * 
 * @param kpkg_file The file object to validate
 * @return true 
 * @return false 
 */
bool KPKG_ValidateFile(KPKG_File* kpkg_file);

char* KPKG_GetString(KPKG_File* kpkg_file, uint64_t offset);
KPKG_DependencyEntry* KPKG_GetDependencies(KPKG_File* kpkg_file, size_t* count);
KPKG_FileEntry* KPKG_GetFileEntries(KPKG_File* kpkg_file, size_t* count);

/**
 * @brief Extract a memory-mapped compressed file
 * 
 * @param ptr The location of the file in memory
 * @param size The size of the compressed data
 * @param out_size The size of the output file
 * @param out The path of the output file
 * @return KPKG_Result 
 */
KPKG_Result KPKG_ExtractData(void* ptr, uint64_t size, uint64_t out_size, char* out_path);

