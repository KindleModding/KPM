#include <errno.h>
#include <sys/sendfile.h>
#include "curl/curl.h"
#include "curl/easy.h"
#include "curl_helpers.h"
#include "internal_utils.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "zstd.h"

#include "kpm/kpkg.h"
#include "kpkg3.h"
#include "kpkg1.h"
#include "kpm/kpmio.h"
#include "simpleGET.h"

char *KPKG_ResultStr(KPKG_Result result)
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
        case KPKG_GENERIC_ERROR:
            return "KPKG_GENERIC_ERROR";
        case KPKG_NOTIMPL_ERROR:
            return "KPKG_NOTIMPL_ERROR";
        case KPKG_HASH_ERROR:
            return "KPKG_HASH_ERROR";
        case KPKG_OPENSSL_ERROR:
            return "KPKG_OPENSSL_ERROR";
        }
}

enum KPKG_CURLBufType
{
    KPKG_CURLType_NONE,
    KPKG_CURLType_KPKG1, // 1 or 2
    KPKG_CURLType_KPKG3 // 3 or newer
};

typedef struct
{
    KPMIO *kpmio;
    char *location;
    char *out_path; /**< For KPKG1, stores path of the output file */
    KPKG_Result error;
    enum KPKG_CURLBufType type; /**< Identify what we're storing */
    union {
        unsigned char *buf; /**< Stores a buffer for KPKG3+ files to load the header */
        FILE *out_file; /**< We cannot stream KPKG1/2 files, so we write it out instead */
    };
    size_t buf_alloc;
    size_t downloaded;
    KPKG_File *file; /**< Populated once parsed */
} KPKG_CURLData;

KPKG_CURLData *KPKG_InitCURLData(KPKG_File *file, const char *location, const char *out_path, KPMIO *kpmio)
{
    KPKG_CURLData *data = malloc(sizeof(KPKG_CURLData));
    *data = (KPKG_CURLData) {
        .kpmio = kpmio,
        .location = strdup(location),
        .out_path = strdup(out_path),
        .error = KPKG_GENERIC_ERROR,
        .type = KPKG_CURLType_NONE,
        .buf = NULL,
        .buf_alloc = 0,
        .downloaded = 0,
        .file = file
    };
    return data;
}

void KPKG_FreeCURLData(KPKG_CURLData **data)
{
    free((*data)->location);
    free((*data)->out_path);

    if ((*data)->type != KPKG_CURLType_KPKG1)
    {
        free((*data)->buf);
        (*data)->buf_alloc = 0;
        (*data)->buf = NULL;
    }
    else if ((*data)->out_file != NULL)
    {
        fclose((*data)->out_file);
        (*data)->out_file = NULL;
    }

    KPKG_CloseFile((*data)->file);
    (*data)->file = NULL;
    free(*data);
    *data = NULL;
}

size_t KPKG_CURLCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    KPKG_CURLData *data = (KPKG_CURLData*) userdata;
    if (data->type == KPKG_CURLType_KPKG1)
        return fwrite(ptr, size, nmemb, data->out_file);
    else
        data->error = KPKG_EARLY_EOF;
    
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
    if (data->type == KPKG_CURLType_NONE && data->downloaded >= 5)
    {
        data->kpmio->log(KPM_VERBOSITY_DEBUG, "Downloaded %i - Determining type of file", data->downloaded);
        // Must be legacy if not "KPKG"
        if (memcmp(data->buf, "KPKG", 4))
        {
            data->kpmio->log(KPM_VERBOSITY_DEBUG, "This is a KPKG1 file");
            data->error = KPKG_OK;
            // For KPKG1/2 we want to download to the FS to unpack later (@TODO: Progress reporting from here)
            data->type = KPKG_CURLType_KPKG1;
            data->kpmio->log(KPM_VERBOSITY_DEBUG, "Writing to %s", data->out_path);
            FILE *out_file = fopen(data->out_path, "w+");
            if (!out_file)
            {
                free(data->buf);
                data->buf_alloc = 0;
                data->buf = NULL;
                data->out_file = NULL;
                data->error = KPKG_FILESYSTEM_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }
            size_t written = fwrite(data->buf, 1, data->downloaded, out_file);
            free(data->buf);
            data->buf_alloc = 0;
            data->buf = NULL;
            data->out_file = out_file;
            return written;
        }
        
        // Not legacy...
        uint8_t version = data->buf[4];
        data->kpmio->log(KPM_VERBOSITY_DEBUG, "This is a KPKG%i file", version);
        if (version < 3 || version > KPKG_VERSION)
        {
            free(data->buf);
            data->buf_alloc = 0;
            data->buf = NULL;
            data->error = KPKG_VERSION_OUT_OF_RANGE;
            return CURL_WRITEFUNC_ERROR;
        }
        
        // Handle newer versions here
        data->type = KPKG_CURLType_KPKG3;
    }

    data->kpmio->log(KPM_VERBOSITY_DEBUG, "Downloaded: %i", data->downloaded);

    if (data->type == KPKG_CURLType_KPKG3 && data->downloaded >= sizeof(KPKG3_Header))
    {
        data->kpmio->log(KPM_VERBOSITY_DEBUG, "Got enough to read KPKG3 header (%i)", data->downloaded);
        KPKG3_Header *header = (KPKG3_Header*) data->buf;
        if (
            will_overflow64(
                sizeof(KPKG3_Header),
                header->dependency_count * sizeof(KPKG3_DependencyEntry)
            ) ||
            will_overflow64(
                sizeof(KPKG3_Header) +
                    header->dependency_count * sizeof(KPKG3_DependencyEntry),
                header->file_count * sizeof(KPKG3_FileEntry)
            ) ||
            will_overflow64(
                sizeof(KPKG3_Header) +
                    header->dependency_count * sizeof(KPKG3_DependencyEntry) +
                    header->file_count * sizeof(KPKG3_FileEntry),
                header->strings_size
            )
        )
        {
            data->error = KPKG_VALIDATION_ERROR;
            return CURL_WRITEFUNC_ERROR;
        }

        size_t target_size = sizeof(KPKG3_Header) + header->dependency_count * sizeof(KPKG3_DependencyEntry) + header->file_count * sizeof(KPKG3_FileEntry) + header->strings_size;
        if (data->downloaded < target_size)
            return realsize; // Haven't read full header yet
        data->kpmio->log(KPM_VERBOSITY_DEBUG, "Enough for target size");
        
        
        void *internal_file = malloc(target_size);
        memcpy(internal_file, data->buf, target_size);
        data->file->kpkg_version = *(uint8_t*)&internal_file[4]; // @FIXME
        data->file->location = strdup(data->location);
        data->file->kpkg3_payload = (KPKG3_Payload) {
            .internal_file = internal_file,
            .mapped_size = -1,
            .header = internal_file,
            .dependencies = internal_file + sizeof(KPKG3_Header),
            .files = internal_file + sizeof(KPKG3_Header) + sizeof(KPKG3_DependencyEntry)*header->dependency_count,
            .strings = internal_file + sizeof(KPKG3_Header) + sizeof(KPKG3_DependencyEntry)*header->dependency_count + sizeof(KPKG3_FileEntry)*header->file_count
        };
        data->error = KPKG_OK;
        return CURL_WRITEFUNC_ERROR;
    }

    return realsize;
}

KPKG_File *KPKG_AllocFile()
{
    KPKG_File *file = malloc(sizeof(KPKG_File));
    file->kpkg_version = -1;
    file->location = NULL;
    return file;
}

KPKG_Result KPKG_LoadFile(KPKG_File **file, const char *location, const char *out_path, KPMIO *kpmio)
{
    *file = NULL;
    if (is_file(location))
    {
        FILE *in_file = fopen(location, "r");
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
            *file = KPKG_AllocFile();
            int res = KPKG1_LoadFile(*file, location, in_file, kpmio);
            if (res != KPKG_OK)
                *file = NULL;
            fclose(in_file);
            return res;
        }

        // KPKG Version
        if (miniheader[4] < 3 || miniheader[4] > KPKG_VERSION)
        {
            fclose(in_file);
            return KPKG_VERSION_OUT_OF_RANGE;
        }

        *file = KPKG_AllocFile();
        int res = KPKG3_LoadFile(*file, location, in_file, kpmio);
        if (res != KPKG_OK)
            *file = NULL;
        fclose(in_file);
        return res;
    }

    *file = KPKG_AllocFile();
    KPKG_CURLData *curl_data = KPKG_InitCURLData(*file, location, out_path, kpmio);

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, location);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (void *)KPKG_CURLCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, curl_data);
    KPM_SET_CURL_OPTIONS(curl);
    CURLcode code = curl_easy_perform(curl);
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    curl_easy_cleanup(curl);

    if (
        curl_data->error == KPKG_OK &&
        (
            (code != CURLE_WRITE_ERROR && code != CURLE_OK) ||
            curl_data->type == KPKG_CURLType_NONE
        )
    )
    {
        KPKG_FreeCURLData(&curl_data);
        *file = NULL;
        return KPKG_CURL_ERROR; // Non-specific CURL related error
    }

    if (curl_data->error != KPKG_OK || curl_data->type == KPKG_CURLType_NONE)
    {
        KPKG_Result res = curl_data->error;
        kpmio->log(KPM_VERBOSITY_DEBUG, "CURL returned %i", curl_data->error);
        KPKG_FreeCURLData(&curl_data);
        *file = NULL;
        return res;
    }

    if (curl_data->type == KPKG_CURLType_KPKG1)
    {
        kpmio->log(KPM_VERBOSITY_DEBUG, "Reading downloaded KPKG1 file");
        fflush(curl_data->out_file);
        fseek(curl_data->out_file, 0, SEEK_SET);
        int res = KPKG1_LoadFile(*file, curl_data->out_path, curl_data->out_file, kpmio);
        if (res != KPKG_OK)
            *file = NULL;
        curl_data->file = NULL; // Ensure it doesn't get freed
        KPKG_FreeCURLData(&curl_data);
        return res;
    }

    // Valid KPKG3+
    if (!KPKG3_ValidateFile(*file, curl_data->downloaded, kpmio))
    {
        KPKG_FreeCURLData(&curl_data); // Will also free the file
        *file = NULL;
        return KPKG_VALIDATION_ERROR;
    }
    curl_data->file = NULL; // Ensure it doesn't get freed
    KPKG_FreeCURLData(&curl_data);
    return KPKG_OK;
}

uint16_t KPKG_GetSupportedPlatforms(KPKG_File *file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        return file->kpkg1_payload.supported_platforms;
    else
        return file->kpkg3_payload.header->supported_platforms;
}

char *KPKG_GetId(KPKG_File *file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        return file->kpkg1_payload.id;
    else
        return file->kpkg3_payload.strings;
}

char *KPKG_GetName(KPKG_File *file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        return file->kpkg1_payload.name;

    int offset = 0;
    for (int i = 0; i < 1; i++)
        while (file->kpkg3_payload.strings[offset++] != '\0') {}
    return file->kpkg3_payload.strings + offset;
}

char *KPKG_GetAuthor(KPKG_File *file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        return file->kpkg1_payload.author;

    int offset = 0;
    for (int i = 0; i < 2; i++)
        while (file->kpkg3_payload.strings[offset++] != '\0') {}
    return file->kpkg3_payload.strings + offset;
}

char *KPKG_GetDescription(KPKG_File *file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        return file->kpkg1_payload.description;

    int offset = 0;
    for (int i = 0; i < 3; i++)
        while (file->kpkg3_payload.strings[offset++] != '\0') {}
    return file->kpkg3_payload.strings + offset;
}

char *KPKG_GetCategory(KPKG_File *file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        return "miscellaneous";

    int offset = 0;
    for (int i = 0; i < 4; i++)
        while (file->kpkg3_payload.strings[offset++] != '\0') {}
    return file->kpkg3_payload.strings + offset;
}

char *KPKG_GetSubcategory(KPKG_File *file)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        return NULL;

    int offset = 0;
    for (int i = 0; i < 5; i++)
        while (file->kpkg3_payload.strings[offset++] != '\0') {}
    if (strlen(file->kpkg3_payload.strings + offset) == 0)
        return NULL;
    return file->kpkg3_payload.strings + offset;
}

SemVer KPKG_GetVersion(KPKG_File *file)
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

uint32_t KPKG_GetDependencies(KPKG_File *file, KPKG_DependencyEntry **dependencies)
{
    *dependencies = NULL;
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
    {
        if (file->kpkg1_payload.dependency_count == 0)
            return file->kpkg1_payload.dependency_count;
        *dependencies = malloc(file->kpkg1_payload.dependency_count * sizeof(KPKG_DependencyEntry));
        for (uint32_t i=0; i < file->kpkg1_payload.dependency_count; i++)
        {
            (*dependencies)[i] = (KPKG_DependencyEntry) {
                .id = strdup(file->kpkg1_payload.dependencies[i].id),
                .min_version = file->kpkg1_payload.dependencies[i].min_version,
                .max_version = file->kpkg1_payload.dependencies[i].max_version
            };
        }
        return file->kpkg1_payload.dependency_count;
    }
    else
    {
        if (file->kpkg3_payload.header->dependency_count == 0)
            return file->kpkg3_payload.header->dependency_count;
        *dependencies = malloc(file->kpkg3_payload.header->dependency_count * sizeof(KPKG_DependencyEntry));
        for (uint32_t i=0; i < file->kpkg3_payload.header->dependency_count; i++)
        {
            (*dependencies)[i] = (KPKG_DependencyEntry) {
                .id = strdup(KPKG3_GetString(&file->kpkg3_payload, file->kpkg3_payload.dependencies[i].id)),
                .min_version = file->kpkg3_payload.dependencies[i].min_version,
                .max_version = file->kpkg3_payload.dependencies[i].max_version
            };
        }
        return file->kpkg3_payload.header->dependency_count;
    }
}

void KPKG_FreeDependencyEntry(KPKG_DependencyEntry *dependency)
{
    free(dependency->id);
    dependency->id = NULL;
}

void KPKG_FreeDependencyEntryList(uint32_t dependency_count, KPKG_DependencyEntry **dependencies)
{
    for (size_t i=0; i < dependency_count; i++)
        KPKG_FreeDependencyEntry(&(*dependencies)[i]);
    free(*dependencies);
    *dependencies = NULL;
}

KPKG_Result KPKG_Unpack(KPKG_File *file, const char *out_path, const char *cmp_path, KPMIO *kpmio)
{
    if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        return KPKG1_Unpack(file, out_path, kpmio);

    return KPKG3_Unpack(file, out_path, cmp_path, kpmio);
}

void KPKG_CloseFile(KPKG_File *file)
{
    if (file == NULL)
        return;
    
    if (file->kpkg_version >= KPKG_VERSION_3)
    {
        if (file->kpkg3_payload.mapped_size != -1)
        {
            munmap(file->kpkg3_payload.internal_file, file->kpkg3_payload.mapped_size);
            file->kpkg3_payload.internal_file = NULL;
        }
        else
        {
            free(file->kpkg3_payload.internal_file);
            file->kpkg3_payload.internal_file = NULL;
        }
    }
    else if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
    {
        free(file->kpkg1_payload.id);
        free(file->kpkg1_payload.name);
        free(file->kpkg1_payload.author);
        free(file->kpkg1_payload.description);
        for (uint32_t i = 0; file->kpkg1_payload.dependencies != NULL && i < file->kpkg1_payload.dependency_count; i++)
            free(file->kpkg1_payload.dependencies[i].id);
        free(file->kpkg1_payload.dependencies);
        file->kpkg1_payload.id = NULL;
        file->kpkg1_payload.name = NULL;
        file->kpkg1_payload.author = NULL;
        file->kpkg1_payload.description = NULL;
        file->kpkg1_payload.dependencies = NULL;
        file->kpkg1_payload.dependency_count = 0;
    }
    free(file->location);
    free(file);
}