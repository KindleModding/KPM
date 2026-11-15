#include "kpkg1.h"

#include "archive.h"
#include "archive_entry.h"
#include "cjson/cJSON.h"
#include "internal_utils.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

char *KPKG1_GetManifest(FILE *file)
{
    // Read the file
    struct archive_entry *entry;
    struct archive *a = archive_read_new();
    int r;
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if ((r = archive_read_open_FILE(a, file)))
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

        char *out = malloc(archive_entry_size(entry) + 1);
        memset(out, 0, archive_entry_size(entry) + 1);
        const void *blockBuffer;
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

KPKG_Result KPKG1_LoadFile(KPKG_File *kpkg_file, const char *location, FILE *in_file)
{
    char *raw_manifest = KPKG1_GetManifest(in_file);
    if (!raw_manifest)
        return KPKG_UNKNOWN_FORMAT;
    cJSON *json = cJSON_Parse(raw_manifest);
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

    *kpkg_file = (KPKG_File) {
        .location = strdup(location),
        .kpkg_version = manifest_version,
        .kpkg1_payload = {
            .id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "id"))),
            .name = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "name"))),
            .author = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "author"))),
            .description = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "description"))),
            .package_version = {
                .major = cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "version"), 0)),
                .minor = cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "version"), 1)),
                .patch = cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "version"), 2))
            },
            .dependency_count = cJSON_GetArraySize(cJSON_GetObjectItem(json, "dependencies")),
            .dependencies = NULL,
            .supported_platforms = 0
        }
    };

    cJSON *platform;
    cJSON_ArrayForEach(platform, json)
    {
        if (!cJSON_IsString(platform))
        {
            free(raw_manifest);
            cJSON_Delete(json);
            KPKG_CloseFile(kpkg_file);
            return KPKG_VALIDATION_ERROR;
        }

        char *platform_str = cJSON_GetStringValue(platform);
        if (!strcmp(platform_str, "kindle"))
            (*kpkg_file).kpkg1_payload.supported_platforms |= PLATFORM_KINDLE;
        else if (!strcmp(platform_str, "kindle5"))
            (*kpkg_file).kpkg1_payload.supported_platforms |= PLATFORM_KINDLE5;
        else if (!strcmp(platform_str, "kindlepw2"))
            (*kpkg_file).kpkg1_payload.supported_platforms |= PLATFORM_KINDLEPW2;
        else if (!strcmp(platform_str, "kindlehf"))
            (*kpkg_file).kpkg1_payload.supported_platforms |= PLATFORM_KINDLEHF;
        else
        {
            free(raw_manifest);
            cJSON_Delete(json);
            KPKG_CloseFile(kpkg_file);
            return KPKG_VALIDATION_ERROR;
        }
    }

    (*kpkg_file).kpkg1_payload.dependencies = malloc(sizeof(KPKG3_DependencyEntry) * (*kpkg_file).kpkg1_payload.dependency_count);

    for (int i=0; i < (*kpkg_file).kpkg1_payload.dependency_count; i++)
    {
        cJSON *el = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "dependencies"), i);
        cJSON *min = cJSON_GetObjectItem(el, "min");
        cJSON *max = cJSON_GetObjectItem(el, "max");
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
            KPKG_CloseFile(kpkg_file);
            return KPKG_VALIDATION_ERROR;
        }
    }

    for (int i=0; i < (*kpkg_file).kpkg1_payload.dependency_count; i++)
    {
        cJSON *el = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "dependencies"), i);
        cJSON *min = cJSON_GetObjectItem(el, "min");
        cJSON *max = cJSON_GetObjectItem(el, "max");
        (*kpkg_file).kpkg1_payload.dependencies[i].id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(el, "id")));
        if (!min)
            (*kpkg_file).kpkg1_payload.dependencies[i].min_version = (SemVer) {
                .major = 0,
                .minor = 0,
                .patch = 0
            };
        else
            (*kpkg_file).kpkg1_payload.dependencies[i].min_version = (SemVer) {
                .major = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 0)),
                .minor = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 1)),
                .patch = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 2))
            };
        
        if (!max)
            (*kpkg_file).kpkg1_payload.dependencies[i].max_version = (SemVer) {
                .major = UINT32_MAX,
                .minor = UINT32_MAX,
                .patch = UINT32_MAX
            };
        else
            (*kpkg_file).kpkg1_payload.dependencies[i].max_version = (SemVer) {
                .major = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 0)),
                .minor = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 1)),
                .patch = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 2))
            };
    }

    return KPKG_OK;
}

/**
 * @brief Copy data between two libarchive archives
 * 
 * @param ar 
 * @param aw 
 * @return int 
 */
static int copy_data(struct archive *ar, struct archive *aw)
{
  int r;
  const void *buff;
  size_t size;
  la_int64_t offset;

  for (;;) {
    r = archive_read_data_block(ar, &buff, &size, &offset);
    if (r == ARCHIVE_EOF)
      return (ARCHIVE_OK);
    if (r < ARCHIVE_OK)
      return (r);
    r = archive_write_data_block(aw, buff, size, offset);
    if (r < ARCHIVE_OK) {
      fprintf(stderr, "%s\n", archive_error_string(aw));
      return (r);
    }
  }
}

// @TODO: Implement progress monitoring
KPKG_Result KPKG1_WriteTo(KPKG_File *file, const char *out_path, KPMIO *kpm_io)
{
    assert(file->kpkg_version == 1 || file->kpkg_version == 2);
    // location is ALWAYS guaranteed to be a path for KPKG1 (see KPKG_CURLCallback)
    if (access(file->location, R_OK) != 0)
    {
        return KPKG_FILESYSTEM_ERROR;
    }

    // Read the file
    struct archive_entry *entry;

    int flags = ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_ACL;
    flags |= ARCHIVE_EXTRACT_FFLAGS;
    flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;

    struct archive *a = archive_read_new();
    struct archive *ext = archive_write_disk_new();
    int r;

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);

    if ((r = archive_read_open_filename(a, file->location, 10240)))
    {
        kpm_io->log(KPM_VERBOSITY_ERROR, "Could not open file %s", file->location);
        goto libarchive_error;
    }

    for (;;)
    {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF)
        {
            break;
        }

        if (r < ARCHIVE_OK)
        {
            kpm_io->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
        }
        if (r < ARCHIVE_WARN)
        {
            goto libarchive_error;
        }

        char *entryPath = asprintf_hd("%s/%s", out_path, archive_entry_pathname(entry));
        archive_entry_set_pathname(entry, entryPath);
        r = archive_write_header(ext, entry);
        free(entryPath);

        if (r < ARCHIVE_OK)
        {
            kpm_io->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
        }
        else if (archive_entry_size(entry) > 0)
        {
            r = copy_data(a, ext);
            if (r < ARCHIVE_OK)
            {
                kpm_io->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
            }
            if (r < ARCHIVE_WARN)
            {
                goto libarchive_error;
            }
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);
    return KPKG_OK;
libarchive_error:
    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);
    return KPKG_LIBARCHIVE_ERROR;
}