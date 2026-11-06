#include "kpm/kpkg.h"
#include "archive.h"
#include "archive_entry.h"
#include "cjson/cJSON.h"
#include "curl/curl.h"
#include "curl/easy.h"
#include "curl_helpers.h"
#include "simpleGET.h"
#include "internal_utils.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "zstd.h"

char* KPKG1_GetManifest(FILE* file)
{
    // Read the file
    struct archive_entry* entry;
    struct archive* a = archive_read_new();
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

KPKG_Result KPKG1_LoadFile(KPKG_File* kpkg_file, const char* location, FILE* in_file)
{
    char* raw_manifest = KPKG1_GetManifest(in_file);
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

    cJSON* platform;
    cJSON_ArrayForEach(platform, json)
    {
        if (!cJSON_IsString(platform))
        {
            free(raw_manifest);
            cJSON_Delete(json);
            KPKG_CloseFile(kpkg_file);
            return KPKG_VALIDATION_ERROR;
        }

        char* platform_str = cJSON_GetStringValue(platform);
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
            KPKG_CloseFile(kpkg_file);
            return KPKG_VALIDATION_ERROR;
        }
    }

    for (int i=0; i < (*kpkg_file).kpkg1_payload.dependency_count; i++)
    {
        cJSON* el = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "dependencies"), i);
        cJSON* min = cJSON_GetObjectItem(el, "min");
        cJSON* max = cJSON_GetObjectItem(el, "max");
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
KPKG_Result KPKG1_ExtractTo(KPKG_File* file, char* out_path, KPMIO* kpmIO)
{
    assert(file->kpkg_version == 1 || file->kpkg_version == 2);
    // location is ALWAYS guaranteed to be a path for KPKG1 (see KPKG_CURLCallback)
    if (access(file->location, R_OK) != 0)
    {
        return KPKG_FILESYSTEM_ERROR;
    }

    // Read the file
    struct archive_entry* entry;

    int flags = ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_ACL;
    flags |= ARCHIVE_EXTRACT_FFLAGS;
    flags |= ARCHIVE_EXTRACT_SECURE_NODOTDOT;

    struct archive* a = archive_read_new();
    struct archive* ext = archive_write_disk_new();
    int r;

    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);

    if ((r = archive_read_open_filename(a, file->location, 10240)))
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "Could not open file %s", file->location);
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
            kpmIO->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
        }
        if (r < ARCHIVE_WARN)
        {
            goto libarchive_error;
        }

        char* entryPath = asprintf_hd("%s/%s", out_path, archive_entry_pathname(entry));
        archive_entry_set_pathname(entry, entryPath);
        r = archive_write_header(ext, entry);
        free(entryPath);

        if (r < ARCHIVE_OK)
        {
            kpmIO->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
        }
        else if (archive_entry_size(entry) > 0)
        {
            r = copy_data(a, ext);
            if (r < ARCHIVE_OK)
            {
                kpmIO->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
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

    return true;
}

KPKG_Result KPKG3_LoadFile(KPKG_File* kpkg_file, const char* location, FILE* in_file)
{
    struct stat stats;
    fstat(fileno(in_file), &stats);

    *kpkg_file = (KPKG_File) {
        .location = strdup(location),
        .kpkg3_payload = {
            .map_size = stats.st_size,
            .internal_file = mmap(NULL, stats.st_size, PROT_READ, MAP_PRIVATE, fileno(in_file), 0),
            .header = NULL,
            .dependencies = NULL,
            .files = NULL,
            .strings = NULL
        }
    };
    
    fclose(in_file);
    if (!kpkg_file->kpkg3_payload.internal_file)
    {
        KPKG_CloseFile(kpkg_file);
        return KPKG_IO_ERROR;
    }

    kpkg_file->kpkg3_payload.header = kpkg_file->kpkg3_payload.internal_file;
    kpkg_file->kpkg3_payload.dependencies = kpkg_file->kpkg3_payload.internal_file + sizeof(KPKG3_Header);
    kpkg_file->kpkg3_payload.files = kpkg_file->kpkg3_payload.internal_file + sizeof(KPKG3_Header) +
                                    (sizeof(KPKG3_DependencyEntry) * kpkg_file->kpkg3_payload.header->dependency_count);
    kpkg_file->kpkg3_payload.strings = kpkg_file->kpkg3_payload.internal_file + sizeof(KPKG3_Header) +
                                    (sizeof(KPKG3_DependencyEntry) * kpkg_file->kpkg3_payload.header->dependency_count) +
                                    (sizeof(KPKG3_FileEntry) * kpkg_file->kpkg3_payload.header->file_count);

    // Validate dependency, file and string sections do not OOB
    if (!KPKG3_ValidateFile(kpkg_file, stats.st_size))
    {
        KPKG_CloseFile(kpkg_file);
        return KPKG_VALIDATION_ERROR;
    }

    return KPKG_OK;
}

char* KPKG_ResultStr(KPKG_Result result)
{
    switch (result)
    {
        case KPKG_OK:
            return "KPKG_OK";
        case KPKG_UNKNOWN_FORMAT:
            return "KPKG_UNKNOWN_FORMAT";
        case KPKG_VALIDATION_ERROR:
            return "KPKG_VALIDATION_ERROR";
        case KPKG_IO_ERROR:
            return "KPKG_IO_ERROR";
        case KPKG_MMAP_ERROR:
            return "KPKG_MMAP_ERROR";
        case KPKG_ZSTD_ERROR:
            return "KPKG_ZSTD_ERROR";
        case KPKG_VERSION_OUT_OF_RANGE:
            return "KPKG_VERSION_OUT_OF_RANGE";
        case KPKG_EARLY_EOF:
            return "KPKG_EARLY_EOF";
        case KPKG_CURL_ERROR:
            return "KPKG_CURL_ERROR";
        case KPKG_LIBARCHIVE_ERROR:
            return "KPKG_LIBARCHIVE_ERROR";
        case KPKG_FILESYSTEM_ERROR:
            return "KPKG_FILESYSTEM_ERROR";
        default:
            return "KPKG_GENERIC_ERROR";
    }
}

enum KPKG_CURLBufType
{
    KPKG_CURLTypeNONE,
    KPKG_CURLTypeKPKG1, // 1 or 2
    KPKG_CURLTypeKPKG3, // 3 or newer
};

typedef struct
{
    char* location;
    char* out_path; /**< For KPKG1, stores path of the output file */
    KPKG_Result error;
    enum KPKG_CURLBufType type; /**< Identify what we're storing */
    union {
        unsigned char* buf; /**< Stores a buffer for KPKG3+ files to load the header */
        FILE* out_file; /**< We cannot stream KPKG1/2 files, so we write it out instead */
    };
    size_t buf_alloc;
    size_t downloaded;
    KPKG_File* file; /**< Populated once parsed */
} KPKG_CURLData;

KPKG_CURLData* KPKG_InitCURLData(KPKG_File* file, const char* location, const char* out_path)
{
    KPKG_CURLData* data = malloc(sizeof(KPKG_CURLData));
    *data = (KPKG_CURLData) {
        .location = strdup(location),
        .out_path = strdup(out_path),
        .error = KPKG_GENERIC_ERROR,
        .type = KPKG_CURLTypeNONE,
        .buf = NULL,
        .buf_alloc = 0,
        .downloaded = 0,
        .file = file
    };
    return data;
}

void KPKG_FreeCURLData(KPKG_CURLData** data)
{
    free((*data)->location);
    free((*data)->out_path);

    if ((*data)->type != KPKG_CURLTypeKPKG1)
        free((*data)->buf);
    else if ((*data)->out_file != NULL)
        fclose((*data)->out_file);

    KPKG_CloseFile((*data)->file);
    free((*data)->file);
    free(*data);
    *data = NULL;
}

size_t KPKG_CURLCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    KPKG_CURLData* data = (KPKG_CURLData*) ptr;
    if (data->type == KPKG_CURLTypeKPKG1)
        return fwrite(ptr, size, nmemb, data->out_file);
    
    // Write to buffer
    size_t realsize = nmemb;
    if (data->downloaded + realsize > data->buf_alloc)
    {
        data->buf_alloc *= 1.5;
        if (data->buf_alloc < data->downloaded + realsize)
            data->buf_alloc = data->downloaded + realsize;

        data->buf = realloc(data->buf, data->buf_alloc);
    }

    memcpy(data->buf + data->downloaded, ptr, realsize);
    data->downloaded += realsize;


    // Get version
    if (data->type == KPKG_CURLTypeNONE && data->downloaded >= 5)
    {
        // Must be legacy if not "KPKG"
        if (memcmp(data->buf, "KPKG", 4))
        {
            // For KPKG1/2 we want to download to the FS to unpack later (@TODO: Progress reporting from here)
            data->type = KPKG_CURLTypeKPKG1;
            FILE* out_file = fopen(data->out_path, "w");
            size_t written = fwrite(data->buf, 1, data->downloaded, out_file);
            free(data->buf);
            data->buf_alloc = 0;
            data->buf = NULL;
            data->out_file = out_file;
            return written;
        }
        
        // Not legacy...
        uint8_t version = data->buf[4];
        if (version < 3 || version > KPKG_VERSION)
        {
            free(data->buf);
            data->buf_alloc = 0;
            data->buf = NULL;
            data->error = KPKG_VERSION_OUT_OF_RANGE;
            return CURL_WRITEFUNC_ERROR;
        }
        
        // Handle newer versions here
        data->type = KPKG_CURLTypeKPKG3;
        return realsize;
    }

    if (data->type == KPKG_CURLTypeKPKG3 && data->downloaded >= sizeof(KPKG3_Header))
    {
        KPKG3_Header* header = (KPKG3_Header*) data->buf;
        size_t target_size = sizeof(KPKG3_Header) + header->dependency_count * sizeof(KPKG3_DependencyEntry) + header->file_count * sizeof(KPKG3_FileEntry) + header->strings_size;
        if (data->downloaded < target_size)
            return realsize; // Haven't read full header yet
        
        
        void* internal_file = malloc(target_size);
        memcpy(internal_file, data->buf, target_size);
        data->file->kpkg_version = *((int*) &(internal_file[4])); // @FIXME
        data->file->location = strdup(data->location);
        data->file->kpkg3_payload = (KPKG3_Payload) {
            .internal_file = internal_file,
            .map_size = -1,
            .header = internal_file,
            .dependencies = internal_file + sizeof(KPKG3_Header),
            .files = internal_file + sizeof(KPKG3_Header) + sizeof(KPKG3_DependencyEntry)*header->dependency_count,
            .strings = internal_file + sizeof(KPKG3_Header) + sizeof(KPKG3_DependencyEntry)*header->dependency_count + sizeof(KPKG3_FileEntry)*header->file_count
        };
        free(data->buf);
        data->buf_alloc = 0;
        data->downloaded = 0;
        data->buf = NULL;
        data->error = KPKG_OK;
        return CURL_WRITEFUNC_ERROR;
    }

    return realsize;
}

KPKG_Result KPKG_LoadFile(KPKG_File** kpkg_file, const char* location, const char* out_path)
{
    *kpkg_file = NULL;
    if (is_path(location))
    {
        FILE* in_file = fopen(location, O_RDONLY);
        if (!in_file)
            return KPKG_IO_ERROR;

        char miniheader[5];
        if (fread(&miniheader, 1, 5, in_file) != 5)
        {
            fclose(in_file);
            return KPKG_VALIDATION_ERROR;
        }

        fseek(in_file, 0, SEEK_SET);
        if (memcmp(miniheader, "KPKG", 4)) // This is most likely a legacy file!
        {
            *kpkg_file = malloc(sizeof(KPKG_File));
            int res = KPKG1_LoadFile(*kpkg_file, location, in_file);
            fclose(in_file);
            return res;
        }

        // KPKG Version
        if (miniheader[4] < 3 || miniheader[4] > KPKG_VERSION)
        {
            fclose(in_file);
            return KPKG_VERSION_OUT_OF_RANGE;
        }

        *kpkg_file = malloc(sizeof(KPKG_File));
        int res = KPKG3_LoadFile(*kpkg_file, location, in_file);
        fclose(in_file);
        return res;
    }

    *kpkg_file = malloc(sizeof(KPKG_File));
    **kpkg_file = (KPKG_File) {
        .kpkg_version = -1
    };
    KPKG_CURLData* curl_data = KPKG_InitCURLData(*kpkg_file, location, out_path);

    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, location);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SimpleGET_Callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)KPKG_CURLCallback); // This _should_ work
    KPM_SET_CURL_OPTIONS(curl);
    CURLcode code = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);

    if (
        curl_data->error == KPKG_OK &&
        (
            (code != CURLE_WRITE_ERROR && code != CURLE_OK) ||
            curl_data->type == KPKG_CURLTypeNONE
        )
    )
    {
        KPKG_FreeCURLData(&curl_data);
        return KPKG_CURL_ERROR; // Non-specific CURL related error
    }

    if (curl_data->error != KPKG_OK || curl_data->type == KPKG_CURLTypeNONE)
    {
        KPKG_Result res = curl_data->error;
        if (res == KPKG_OK)
            res = KPKG_EARLY_EOF; // Buffer must be none

        KPKG_FreeCURLData(&curl_data);
        return res;
    }

    if (curl_data->type == KPKG_CURLTypeKPKG1)
    {
        int res = KPKG1_LoadFile(*kpkg_file, curl_data->out_path, curl_data->out_file);
        KPKG_FreeCURLData(&curl_data);
        return res;
    }

    // Valid KPKG3+
    if (!KPKG3_ValidateFile(*kpkg_file, curl_data->downloaded))
    {
        KPKG_FreeCURLData(&curl_data); // Will also free the file
        return KPKG_VALIDATION_ERROR;
    }
    curl_data->file = NULL; // Ensure it doesn't get freed
    KPKG_FreeCURLData(&curl_data);
    return KPKG_OK;
}

char* KPKG3_GetString(KPKG3_Payload* payload, uint64_t offset)
{
    return payload->strings + offset;
}

char* KPKG_GetId(KPKG_File* file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        return file->kpkg1_payload.id;
    else
        return file->kpkg3_payload.strings;
}

char* KPKG_GetName(KPKG_File* file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
    {
        return file->kpkg1_payload.name;
    }
    else
    {
        int offset = 0;
        for (int i = 0; i < 1; i++)
            while (file->kpkg3_payload.strings[offset++] != '\0') {}
        return file->kpkg3_payload.strings + offset + 1;
    }
}

char* KPKG_GetAuthor(KPKG_File* file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
    {
        return file->kpkg1_payload.author;
    }
    else
    {
        int offset = 0;
        for (int i = 0; i < 2; i++)
            while (file->kpkg3_payload.strings[offset++] != '\0') {}
        return file->kpkg3_payload.strings + offset + 1;
    }
}

char* KPKG_GetDescription(KPKG_File* file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
    {
        return file->kpkg1_payload.description;
    }
    else
    {
        int offset = 0;
        for (int i = 0; i < 3; i++)
            while (file->kpkg3_payload.strings[offset++] != '\0') {}
        return file->kpkg3_payload.strings + offset + 1;
    }
}

SemVer KPKG_GetVersion(KPKG_File* file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
    {
        return file->kpkg1_payload.package_version;
    }
    else
    {
        return file->kpkg3_payload.header->package_version;
    }
}

size_t KPKG_GetDependencies(KPKG_File* file, KPKG_DependencyEntry** dependencies)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
    {
        *dependencies = malloc(file->kpkg1_payload.dependency_count * sizeof(KPKG_DependencyEntry));
        memcpy(*dependencies, file->kpkg1_payload.dependencies, file->kpkg1_payload.dependency_count * sizeof(KPKG_DependencyEntry));
        return file->kpkg1_payload.dependency_count;
    }
    else
    {
        *dependencies = malloc(file->kpkg3_payload.header->dependency_count * sizeof(KPKG_DependencyEntry));
        memcpy(*dependencies, file->kpkg3_payload.dependencies, file->kpkg3_payload.header->dependency_count * sizeof(KPKG_DependencyEntry));
        return file->kpkg1_payload.dependency_count;
    }
}

void KPKG_CloseFile(KPKG_File* file)
{
    if (file == NULL)
        return;
    
    if (file->kpkg_version >= KPKG_VERSION_3)
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
    else if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
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