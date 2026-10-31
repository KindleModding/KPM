#include "kpm/kpkg.h"
#include "archive.h"
#include "archive_entry.h"
#include "cjson/cJSON.h"
#include "curl/curl.h"
#include "simpleGET.h"
#include "internal_utils.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "zstd.h"

char* KPKG1_GetManifest(int fd)
{
    // Read the file
    struct archive_entry* entry;
    struct archive* a = archive_read_new();
    int r;
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if ((r = archive_read_open_fd(a, fd, 1024)))
    {
        archive_read_close(a);
        archive_read_free(a);
        return NULL;
    }

    for (;;)
    {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF || r < ARCHIVE_WARN)
            break;
        if (r < ARCHIVE_OK)
            return NULL;

        if (strcmp(archive_entry_pathname(entry), "manifest.json") && strcmp(archive_entry_pathname(entry), "/manifest.json") && strcmp(archive_entry_pathname(entry), "./manifest.json"))
            continue;

        char* out = malloc(archive_entry_size(entry) + 1);
        memset(out, 0, archive_entry_size(entry) + 1);
        const void* blockBuffer;
        size_t blockSize = 0;
        la_int64_t offset = 0;
        int r = 0;
        for (;;)
        {
            r = archive_read_data_block(a, &blockBuffer, &blockSize, &offset);
            memcpy(out+offset, blockBuffer, blockSize);
            if (r == ARCHIVE_EOF || r < ARCHIVE_OK)
                break;
        }

        if (r < ARCHIVE_OK)
        {
            free(out);
            return NULL; // Triggers error case
        }

        return out;
    }

    archive_read_close(a);
    archive_read_free(a);

    return NULL;
}

KPKG_Result KPKG1_LoadFile(KPKG_File** file, const char* location, int fd)
{
    char* raw_manifest = KPKG1_GetManifest(fd);
    if (!raw_manifest)
        return KPKG_UNKNOWN_FORMAT;
    cJSON* json = cJSON_Parse(raw_manifest);
    if (!json)
    {
        free(raw_manifest);
        return KPKG_UNKNOWN_FORMAT;
    }

    if (cJSON_GetObjectItem(json, "manifest_version") == NULL)
    {
        free(raw_manifest);
        cJSON_Delete(json);
        return KPKG_VALIDATION_ERROR;
    }

    int manifest_version = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "manifest_version"));

    if (
        manifest_version < 1 ||
        manifest_version > 2 ||
        !cJSON_IsString(cJSON_GetObjectItem(json, "id")) ||
        !cJSON_IsString(cJSON_GetObjectItem(json, "name")) ||
        !cJSON_IsString(cJSON_GetObjectItem(json, "author")) ||
        !cJSON_IsString(cJSON_GetObjectItem(json, "description")) ||
        !cJSON_GetObjectItem(json, "version") ||
        !cJSON_IsArray(cJSON_GetObjectItem(json, "version")) ||
        cJSON_GetArraySize(cJSON_GetObjectItem(json, "version")) != 3 ||
        !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "version"), 0)) ||
        !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "version"), 1)) ||
        !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "version"), 2)) ||
        !cJSON_IsArray(cJSON_GetObjectItem(json, "dependencies"))
    )
    {
        free(raw_manifest);
        cJSON_Delete(json);
        return KPKG_VALIDATION_ERROR;
    }

    *file = malloc(sizeof(KPKG_File));
    **file = (KPKG_File) {
        .location = strdup(location),
        .kpkg_version = manifest_version,
        .kpkg1_payload = {
            .id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "id"))),
            .name = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "name"))),
            .author = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "author"))),
            .description = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "description"))),
            .dependency_count = cJSON_GetArraySize(cJSON_GetObjectItem(json, "dependencies")),
            .dependencies = NULL,
            .supported_platforms = 0
        }
    };

    cJSON* platform;
    cJSON_ArrayForEach(platform, json)
    {
        if (!cJSON_IsString(platform))
        {
            free(raw_manifest);
            cJSON_Delete(json);
            KPKG_CloseFile(*file);
            *file = NULL;
            return KPKG_VALIDATION_ERROR;
        }

        char* platform_str = cJSON_GetStringValue(platform);
        if (!strcmp(platform_str, "kindle"))
            (**file).kpkg1_payload.supported_platforms |= PLATFORM_KINDLE;
        else if (!strcmp(platform_str, "kindle5"))
            (**file).kpkg1_payload.supported_platforms |= PLATFORM_KINDLE5;
        else if (!strcmp(platform_str, "kindlepw2"))
            (**file).kpkg1_payload.supported_platforms |= PLATFORM_KINDLEPW2;
        else if (!strcmp(platform_str, "kindlehf"))
            (**file).kpkg1_payload.supported_platforms |= PLATFORM_KINDLEHF;
        else
        {
            free(raw_manifest);
            cJSON_Delete(json);
            KPKG_CloseFile(*file);
            *file = NULL;
            return KPKG_VALIDATION_ERROR;
        }
    }

    (**file).kpkg1_payload.dependencies = malloc(sizeof(KPKG3_DependencyEntry) * (**file).kpkg1_payload.dependency_count);

    for (int i=0; i < (**file).kpkg1_payload.dependency_count; i++)
    {
        cJSON* el = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "dependencies"), i);
        cJSON* min = cJSON_GetObjectItem(el, "min");
        cJSON* max = cJSON_GetObjectItem(el, "max");
        if (
            !cJSON_IsString(cJSON_GetObjectItem(el, "id")) ||
            (
                min && 
                (
                    !cJSON_IsArray(min) ||
                    cJSON_GetArraySize(min) != 3 ||
                    !cJSON_IsNumber(cJSON_GetArrayItem(min, 0)) ||
                    !cJSON_IsNumber(cJSON_GetArrayItem(min, 1)) ||
                    !cJSON_IsNumber(cJSON_GetArrayItem(min, 2))
                )
            ) ||
            (
                max &&
                (
                    !cJSON_IsArray(max) ||
                    cJSON_GetArraySize(max) != 3 ||
                    !cJSON_IsNumber(cJSON_GetArrayItem(max, 0)) ||
                    !cJSON_IsNumber(cJSON_GetArrayItem(max, 1)) ||
                    !cJSON_IsNumber(cJSON_GetArrayItem(max, 2))
                )
            )
        )
        {
            free(raw_manifest);
            cJSON_Delete(json);
            KPKG_CloseFile(*file);
            *file = NULL;
            return KPKG_VALIDATION_ERROR;
        }
    }

    for (int i=0; i < (**file).kpkg1_payload.dependency_count; i++)
    {
        cJSON* el = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "dependencies"), i);
        cJSON* min = cJSON_GetObjectItem(el, "min");
        cJSON* max = cJSON_GetObjectItem(el, "max");
        (**file).kpkg1_payload.dependencies[i].id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(el, "id")));
        if (!min)
            (**file).kpkg1_payload.dependencies[i].min_version = (SemVer) {
                .major = 0,
                .minor = 0,
                .patch = 0
            };
        else
            (**file).kpkg1_payload.dependencies[i].min_version = (SemVer) {
                .major = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 0)),
                .minor = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 1)),
                .patch = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 2))
            };
        
        if (!max)
            (**file).kpkg1_payload.dependencies[i].max_version = (SemVer) {
                .major = UINT32_MAX,
                .minor = UINT32_MAX,
                .patch = UINT32_MAX
            };
        else
            (**file).kpkg1_payload.dependencies[i].max_version = (SemVer) {
                .major = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 0)),
                .minor = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 1)),
                .patch = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 2))
            };
    }

    return KPKG_OK;
}

bool KPKG3_ValidateFile(KPKG_File* file, int file_size)
{
    if (file->kpkg3_payload.internal_file && file->kpkg3_payload.map_size != file_size) // Shouldn't be possible
        return false;

    if (
        sizeof(KPKG3_Header) +
        sizeof(KPKG3_DependencyEntry) * file->kpkg3_payload.header->dependency_count +
        sizeof(KPKG3_FileEntry) * file->kpkg3_payload.header->file_count +
        file->kpkg3_payload.header->strings_size > file_size
    )
        return false; // Sizes incorrect

    if (file->kpkg3_payload.header->file_count == 0)
        return false; // Must have at least 1 file

    if (file->kpkg3_payload.files[file->kpkg3_payload.header->file_count-1].offset > file_size)
        return false; // File offset too big

    if (file->kpkg3_payload.strings[file->kpkg3_payload.header->strings_size - 1] != 0)
        return false; // String section not null-terminated

    for (int i=0; i < file->kpkg3_payload.header->dependency_count; i++)
        if (file->kpkg3_payload.dependencies[i].id >= file->kpkg3_payload.header->strings_size)
            return false; // Dependency id OOB

    for (int i=0; i < file->kpkg3_payload.header->file_count; i++)
        if (file->kpkg3_payload.files[i].path >= file->kpkg3_payload.header->strings_size)
            return false; // File path OOB

    __off_t file_section_size = file_size - (
                                sizeof(KPKG3_Header) +
                                file->kpkg3_payload.header->dependency_count * sizeof(KPKG3_DependencyEntry) +
                                file->kpkg3_payload.header->file_count * sizeof(KPKG3_FileEntry) +
                                file->kpkg3_payload.header->strings_size
                            );

    uint64_t last_offset = 0;
    for (int i=0; i < file->kpkg3_payload.header->file_count; last_offset = file->kpkg3_payload.files[i].offset, i++)
        if (file->kpkg3_payload.files[i].offset >= file_section_size || file->kpkg3_payload.files[i].offset <= last_offset)
            return false; // File content OOB, or offsets invalid
}

KPKG_Result KPKG3_LoadFile(KPKG_File** file, const char* location, int fd)
{
    struct stat stats;
    fstat(fd, &stats);

    *file = malloc(sizeof(KPKG_File));
    **file = (KPKG_File) {
        .location = strdup(location),
        .kpkg3_payload = {
            .map_size = stats.st_size,
            .internal_file = mmap(NULL, stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0),
            .header = NULL,
            .dependencies = NULL,
            .files = NULL,
            .strings = NULL
        }
    };
    
    close(fd);
    if (!(*file)->kpkg3_payload.internal_file)
    {
        KPKG_CloseFile(*file);
        *file = NULL;
        return KPKG_IO_ERROR;
    }

    (*file)->kpkg3_payload.header = (*file)->kpkg3_payload.internal_file;
    (*file)->kpkg3_payload.dependencies = (*file)->kpkg3_payload.internal_file + sizeof(KPKG3_Header);
    (*file)->kpkg3_payload.files = (*file)->kpkg3_payload.internal_file + sizeof(KPKG3_Header) +
                                    (sizeof(KPKG3_DependencyEntry) * (*file)->kpkg3_payload.header->dependency_count);
    (*file)->kpkg3_payload.strings = (*file)->kpkg3_payload.internal_file + sizeof(KPKG3_Header) +
                                    (sizeof(KPKG3_DependencyEntry) * (*file)->kpkg3_payload.header->dependency_count) +
                                    (sizeof(KPKG3_FileEntry) * (*file)->kpkg3_payload.header->file_count);

    // Validate dependency, file and string sections do not OOB
    if (!KPKG3_ValidateFile(*file, stats.st_size))
    {
        KPKG_CloseFile(*file);
        *file = NULL;
        return KPKG_VALIDATION_ERROR;
    }

    return KPKG_OK;
}

size_t KPKG_CURLCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    KPKG_File* payload = (KPKG3_Payload*) ptr;
}

KPKG_Result KPKG_LoadFile(KPKG_File** file, const char* location)
{
    *file = NULL;
    if (is_path(location))
    {
        int fd = open(location, O_RDONLY);
        if (!fd)
            return KPKG_IO_ERROR;

        char miniheader[5];
        if (read(fd, &miniheader, 5) != 5)
        {
            close(fd);
            return KPKG_VALIDATION_ERROR;
        }

        lseek(fd, 0, SEEK_SET);
        if (strncmp(miniheader, "KPKG", 4)) // This is most likely a legacy file!
        {
            int res = KPKG1_LoadFile(file, location, fd);
            close(fd);
            return res;
        }

        // KPKG Version
        if (miniheader[4] < 3 || miniheader[4] > KPKG_VERSION)
        {
            close(fd);
            return KPKG_VERSION_OUT_OF_RANGE;
        }

        int res = KPKG3_LoadFile(file, location, fd);
        close(fd);
        return res;
    }

    *file = malloc(sizeof(KPKG_File));

    // @TODO: Curl path

    return KPKG_OK;
}

void KPKG_CloseFile(KPKG_File* file)
{
    if (file->kpkg_version >= 2)
    {
        if (file->kpkg3_payload.internal_file)
        {
            munmap(file->kpkg3_payload.internal_file, file->kpkg3_payload.map_size);
            file->kpkg3_payload.internal_file = NULL;
        }
        else
        {
            free(file->kpkg3_payload.header);
            free(file->kpkg3_payload.dependencies);
            free(file->kpkg3_payload.files);
            free(file->kpkg3_payload.strings);
            file->kpkg3_payload.header = NULL;
            file->kpkg3_payload.dependencies = NULL;
            file->kpkg3_payload.files = NULL;
            file->kpkg3_payload.strings = NULL;
        }
    }
    else
    {
        free(file->kpkg1_payload.id);
        free(file->kpkg1_payload.name);
        free(file->kpkg1_payload.author);
        free(file->kpkg1_payload.description);
        free(file->kpkg1_payload.dependencies);
        file->kpkg1_payload.id = NULL;
        file->kpkg1_payload.name = NULL;
        file->kpkg1_payload.author = NULL;
        file->kpkg1_payload.description = NULL;
        file->kpkg1_payload.dependencies = NULL;
    }
    free(file->location);
    free(file);
}

char* KPKG3_GetString(KPKG3_Payload* payload, uint64_t offset)
{
    return payload->strings + offset;
}

KPKG3_DependencyEntry* KPKG3_GetDependencies(KPKG3_Payload* payload, size_t* count)
{
    *count = payload->header->dependency_count;
    return payload->dependencies;
}

KPKG3_FileEntry* KPKG3_GetFileEntries(KPKG3_Payload* payload, size_t* count)
{
    *count = payload->header->file_count;
    return payload->files;
}

KPKG_Result KPKG3_ExtractData(void* ptr, uint64_t size, uint64_t out_size, char* out_path)
{
    int fd = open(out_path, O_WRONLY);
    AssertOrReturn(fd != 0, 0, KPKG_IO_ERROR);
    if (!ftruncate(fd, out_size))
    {
        close(fd);
        return KPKG_IO_ERROR;
    }

    void* file = mmap(NULL, out_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (file == NULL)
    {
        close(fd);
        return KPKG_MMAP_ERROR;
    }

    if (ZSTD_isError(
        ZSTD_decompress(
            file, 
            out_size, 
            ptr + out_size,
            size
        )
    ))
    {
        munmap(file, out_size);
        close(fd);
        return KPKG_MMAP_ERROR;
    }

    munmap(file, out_size);
    close(fd);
    return KPKG_OK;
}