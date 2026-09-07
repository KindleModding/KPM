#pragma once

#include "kpm/semver.h"
#include <stddef.h>
#include <stdint.h>

#define KPKG_MAGIC "KPKG"
#define KPKG_MAGIC_LENGTH 4
#define KPKG_VERSION 3

typedef enum {
    KPKG_OK,
} KPKG_Result;

typedef enum {
    KINDLEANY = 0,
    KINDLE = 1 << 0,
    KINDLE5 = 1 << 1,
    KINDLEPW2 = 1 << 2,
    KINDLEHF = 1 << 3
} KPKG_SupportedPlatforms;

struct KPKG_Header
{
    char magic[KPKG_MAGIC_LENGTH]; // "KPKG"
    uint8_t version; // Format version
    SemVer package_version; // Version of the package
    uint8_t supported_platforms; // Bitfield of KPKG_SupportedPlatforms
    uint32_t dependency_count;
    uint32_t file_count;
    uint64_t strings_size; // String section size
};
typedef struct KPKG_Header KPKG_Header;

struct KPKG_DependencyEntry
{
    uint64_t id; // Pointer in string section
    SemVer min_version;
    SemVer max_version;
};
typedef struct KPKG_DependencyEntry KPKG_DependencyEntry;

struct KPKG_FileEntry
{
    uint64_t path; // Pointer in string section
    uint8_t checksum[16];
    uint32_t file_mode;
    uint64_t offset; // Pointer in file data section
};
typedef struct KPKG_FileEntry KPKG_FileEntry;

struct KPKG_File
{
    char* location; // The URL/path to the file
    void* internal_file; // The memory mapped file or NULL
    size_t map_size; // If file is local and memory mapped, this will indicate the length
    KPKG_Header* header; // Local storage of header (always present)
    KPKG_DependencyEntry* dependencies;
    KPKG_FileEntry* files;
    char* strings;
};
typedef struct KPKG_File KPKG_File;

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
