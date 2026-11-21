#include "curl/curl.h"
#include "curl/easy.h"
#include "curl_helpers.h"
#include "internal_utils.h"
#include "kpm/kpkg.h"
#include "kpm/kpmio.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <openssl/evp.h>
#include "zstd.h"
#include "kpkg3.h"

bool KPKG3_ValidateFile(KPKG_File *file, size_t file_size, KPMIO *kpmio)
{
    if (file->kpkg_version != file->kpkg3_payload.header->version)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG version mismatch (%i:%i)", file->kpkg_version, file->kpkg3_payload.header->version);
        return false;
    }

    if (file->kpkg3_payload.header->version < KPKG_VERSION_3 || file->kpkg3_payload.header->version > KPKG_VERSION)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 version unsupported (%i)", file->kpkg3_payload.header->version);
        return false;
    }

    if (file->kpkg3_payload.mapped_size != -1 && file->kpkg3_payload.mapped_size != file_size) // Shouldn't be possible
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 file mapped size does not match file size");
        return false;
    }

    // Ensure that parsing this file does not cause any kind of overflow
    if (
        will_overflow64(
            sizeof(KPKG3_Header),
            file->kpkg3_payload.header->dependency_count * sizeof(KPKG3_DependencyEntry)
        ) ||
        will_overflow64(
            sizeof(KPKG3_Header) +
                file->kpkg3_payload.header->dependency_count * sizeof(KPKG3_DependencyEntry),
            file->kpkg3_payload.header->file_count * sizeof(KPKG3_FileEntry)
        ) ||
        will_overflow64(
            sizeof(KPKG3_Header) +
                file->kpkg3_payload.header->dependency_count * sizeof(KPKG3_DependencyEntry) +
                file->kpkg3_payload.header->file_count * sizeof(KPKG3_FileEntry),
            file->kpkg3_payload.header->strings_size
        )
    )
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 header count overflow");
        return false; // Sizes incorrect
    }

    if (
        file->kpkg3_payload.header->strings_size > file_size ||
        file->kpkg3_payload.header->dependency_count > file_size ||
        file->kpkg3_payload.header->file_count > file_size ||
        sizeof(KPKG3_DependencyEntry) * file->kpkg3_payload.header->dependency_count > file_size ||
        sizeof(KPKG3_FileEntry) * file->kpkg3_payload.header->file_count > file_size ||
        sizeof(KPKG3_DependencyEntry) * file->kpkg3_payload.header->dependency_count +
        sizeof(KPKG3_FileEntry) * file->kpkg3_payload.header->file_count > file_size ||
        sizeof(KPKG3_Header) +
            sizeof(KPKG3_DependencyEntry) * file->kpkg3_payload.header->dependency_count +
            sizeof(KPKG3_FileEntry) * file->kpkg3_payload.header->file_count +
            file->kpkg3_payload.header->strings_size > file_size
    )
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 header size (%llu) exceeds file size (%i)", sizeof(KPKG3_Header) +
        sizeof(KPKG3_DependencyEntry) * file->kpkg3_payload.header->dependency_count +
        sizeof(KPKG3_FileEntry) * file->kpkg3_payload.header->file_count +
        file->kpkg3_payload.header->strings_size, file_size);
        return false; // Sizes incorrect
    }

    if (file->kpkg3_payload.header->file_count == 0)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 file has no file entries");
        return false; // Must have at least 1 file
    }

    if (file->kpkg3_payload.strings[file->kpkg3_payload.header->strings_size - 1] != 0)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 string section is not null-terminated");
        return false; // String section not null-terminated
    }

    for (int i=0; i < file->kpkg3_payload.header->dependency_count; i++)
    {
        if (file->kpkg3_payload.dependencies[i].id >= file->kpkg3_payload.header->strings_size)
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 Dependency ID OOB");
            return false; // Dependency id OOB
        }
        if (strlen(file->kpkg3_payload.strings + file->kpkg3_payload.dependencies[i].id) < 2)
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 Invalid Dependency ID Length");
            return false;
        }
    }

    for (int i=0; i < file->kpkg3_payload.header->file_count; i++)
    {
        if (file->kpkg3_payload.files[i].path >= file->kpkg3_payload.header->strings_size)
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 file entry path OOB");
            return false; // File path OOB
        }
    }

    size_t string_count = 0;
    for (uint64_t off=0; off < file->kpkg3_payload.header->strings_size && string_count < 6; off += strlen(file->kpkg3_payload.strings + off) + 1, string_count++);
    if (string_count < 6)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 string section too small");
        return false;
    }

    if (strlen(file->kpkg3_payload.strings) < 2)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 ID must be at least 2 characters long");
        return false;
    }

    uint64_t cat_offset = 0;
    for (uint8_t i = 0; i < 4; i++)
        while (file->kpkg3_payload.strings[cat_offset++] != '\0') {}
    if (strlen(file->kpkg3_payload.strings + cat_offset) < 2)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 Category must be at least 2 characters long");
        return false;
    }

    __off_t file_section_size = file_size - (
                                sizeof(KPKG3_Header) +
                                file->kpkg3_payload.header->dependency_count * sizeof(KPKG3_DependencyEntry) +
                                file->kpkg3_payload.header->file_count * sizeof(KPKG3_FileEntry) +
                                file->kpkg3_payload.header->strings_size
                            );

    uint64_t last_offset = 0;
    for (uint32_t i=0; i < file->kpkg3_payload.header->file_count; last_offset = file->kpkg3_payload.files[i].offset, i++)
    {
        kpmio->log(KPM_VERBOSITY_DEBUG, "Checking KPKG3 file entry FS:%u P:%u M: %u O: %u S: %u", sizeof(KPKG3_FileEntry), file->kpkg3_payload.files[i].path, file->kpkg3_payload.files[i].file_mode, file->kpkg3_payload.files[i].offset, file->kpkg3_payload.files[i].size);
        if (
            file->kpkg3_payload.files[i].offset > file_section_size ||
            file->kpkg3_payload.files[i].size > file_section_size ||
            will_overflow64(file->kpkg3_payload.files[i].offset, file->kpkg3_payload.files[i].size) ||
            file->kpkg3_payload.files[i].size > file_section_size - file->kpkg3_payload.files[i].offset ||
            ((i != 0 && file->kpkg3_payload.files[i].offset < last_offset) || (i == 0 && file->kpkg3_payload.files[i].offset != 0))
        )
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 file entry %u with offset %u and size %u is invalid (LO:%u)", i, file->kpkg3_payload.files[i].offset, file->kpkg3_payload.files[i].size, last_offset);
            return false; // File content OOB, or offsets invalid
        }
    }

    for (uint32_t i=0; i < file->kpkg3_payload.header->dependency_count; i++)
    {
        kpmio->log(KPM_VERBOSITY_DEBUG, "Checking KPKG3 dependency", file->kpkg3_payload.dependencies[i].id);
        if (SemVerCmp(file->kpkg3_payload.dependencies[i].min_version, file->kpkg3_payload.dependencies[i].max_version) >= 0)
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "KPKG3 ");
            return false;
        }
    }

    return true;
}

KPKG_Result KPKG3_LoadFile(KPKG_File *kpkg_file, const char *location, FILE *in_file, KPMIO *kpmio)
{
    struct stat stats;
    fstat(fileno(in_file), &stats);

    *kpkg_file = (KPKG_File) {
        .location = strdup(location),
        .kpkg3_payload = {
            .mapped_size = stats.st_size,
            .internal_file = mmap(NULL, stats.st_size, PROT_READ, MAP_PRIVATE, fileno(in_file), 0),
            .header = NULL,
            .dependencies = NULL,
            .files = NULL,
            .strings = NULL
        }
    };
    
    if (kpkg_file->kpkg3_payload.internal_file == MAP_FAILED)
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

    kpkg_file->kpkg_version = kpkg_file->kpkg3_payload.header->version;

    // Validate dependency, file and string sections do not OOB
    if (!KPKG3_ValidateFile(kpkg_file, stats.st_size, kpmio))
    {
        kpmio->log(KPM_VERBOSITY_DEBUG, "Could not validate file of size %iB", stats.st_size);
        KPKG_CloseFile(kpkg_file);
        return KPKG_VALIDATION_ERROR;
    }

    return KPKG_OK;
}

char *KPKG3_GetString(KPKG3_Payload *payload, uint64_t offset)
{
    return payload->strings + offset;
}

typedef struct KPKG3_DownloadRange {
    KPKG3_FileEntry file_entry;
    uint64_t start;
    uint64_t end;
    char *out_path;
} KPKG3_DownloadRange;

void KPKG3_FreeDownloadRanges(uint32_t range_count, KPKG3_DownloadRange *ranges)
{
    for (uint32_t i=0; i < range_count; i++)
        free(ranges[i].out_path);
    free(ranges);
}


typedef enum {
    DOWNLOAD_FULL,          // Download the entire file at once
    DOWNLOAD_SINGLERANGE,   // Download the file one range at a time
    DOWNLOAD_MULTIRANGE     // Default, try to download file in chunks of ranges
} KPKG3_DownloadType;

typedef struct {
    KPKG3_DownloadType type;            /**< The type of download this is */
    KPKG_Result result;                 /**< The return value of this download */
    uint32_t range_count;               /**< The number of ranges to download */
    KPKG3_DownloadRange *ranges;        /**< The ranges to download */
    void *out_buf;                      /**< The ZSTD output buffer */
    size_t out_buf_size;                /**< The size of the ZSTD output buffer */
    uint32_t cur_range_idx;             /**< The current range being downloaded */
    EVP_MD_CTX *cur_evp_ctx;            /**< The current hash context */
    uint64_t total_downloaded;          /**< The total amount of data downloaded */
    size_t total_size;                  /**< The total size of the file */
    int cur_file;                     /**< The current file being written to */
    ZSTD_DStream *cur_zstream;          /**< The current zstream being decompressed */
    KPMIO *kpmio;                       /**< For logging */
} KPKG3_DownloadState;

size_t KPKG3_CurlWriteFunction(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    KPKG3_DownloadState *state = userdata;
    size_t realsize = size * nmemb;
    KPKG3_DownloadRange current_range = state->ranges[state->cur_range_idx];
    ZSTD_outBuffer out = { state->out_buf, state->out_buf_size, 0 };
    uint64_t read_remaining, offset = 0;

    switch (state->type)
    {
    case DOWNLOAD_SINGLERANGE:
        if (state->cur_file == -1)
        {
            state->cur_file = open(current_range.out_path, O_WRONLY|O_CREAT, current_range.file_entry.file_mode);
            if (state->cur_file == -1)
            {
                state->kpmio->log(KPM_VERBOSITY_ERROR, "Could not open %s for writing", current_range.out_path);
                state->result = KPKG_FILESYSTEM_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }
            state->cur_zstream = ZSTD_createDStream();
            state->cur_evp_ctx = EVP_MD_CTX_new();
            if (!EVP_DigestInit_ex2(state->cur_evp_ctx, EVP_sha256(), NULL))
            {
                state->result = KPKG_OPENSSL_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }
        }

        size_t process_size = realsize;
        if (state->total_downloaded + process_size > current_range.end - current_range.start)
            process_size = (current_range.end - current_range.start) - state->total_downloaded;

        ZSTD_inBuffer in = (ZSTD_inBuffer) { ptr, process_size, 0 };

        while (in.pos < in.size)
            if (ZSTD_isError(
                ZSTD_decompressStream(
                        state->cur_zstream,
                        &out,
                        &in
                )
            ))
            {
                state->result = KPKG_ZSTD_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }

        if (!EVP_DigestUpdate(state->cur_evp_ctx, in.src, in.size))
        {
            state->result = KPKG_OPENSSL_ERROR;
            return CURL_WRITEFUNC_ERROR;
        }
        state->total_downloaded += realsize;

        if (write(state->cur_file, state->out_buf, out.pos) != out.pos)
        {
            state->result = KPKG_FILESYSTEM_ERROR;
            return CURL_WRITEFUNC_ERROR;
        }

        if (state->total_downloaded >= current_range.end - current_range.start)
        {
            unsigned char checksum[EVP_MAX_MD_SIZE];
            unsigned int checksum_len;
            if (!EVP_DigestFinal_ex(state->cur_evp_ctx, checksum, &checksum_len) || checksum_len != KPKG_CHECKSUM_SIZE)
            {
                state->result = KPKG_OPENSSL_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }

            if (memcmp(current_range.file_entry.checksum, checksum, KPKG_CHECKSUM_SIZE))
            {
                
                state->kpmio->log(KPM_VERBOSITY_ERROR, "Hash mismatch error: Expected %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x, got %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", current_range.file_entry.checksum[0], current_range.file_entry.checksum[1], current_range.file_entry.checksum[2], current_range.file_entry.checksum[3], current_range.file_entry.checksum[4], current_range.file_entry.checksum[5], current_range.file_entry.checksum[6], current_range.file_entry.checksum[7], current_range.file_entry.checksum[8], current_range.file_entry.checksum[9], current_range.file_entry.checksum[10], current_range.file_entry.checksum[11], current_range.file_entry.checksum[12], current_range.file_entry.checksum[13], current_range.file_entry.checksum[14], current_range.file_entry.checksum[15], current_range.file_entry.checksum[16], current_range.file_entry.checksum[17], current_range.file_entry.checksum[18], current_range.file_entry.checksum[19], current_range.file_entry.checksum[20], current_range.file_entry.checksum[21], current_range.file_entry.checksum[22], current_range.file_entry.checksum[23], current_range.file_entry.checksum[24], current_range.file_entry.checksum[25], current_range.file_entry.checksum[26], current_range.file_entry.checksum[27], current_range.file_entry.checksum[28], current_range.file_entry.checksum[29], current_range.file_entry.checksum[30], current_range.file_entry.checksum[31], checksum[0], checksum[1], checksum[2], checksum[3], checksum[4], checksum[5], checksum[6], checksum[7], checksum[8], checksum[9], checksum[10], checksum[11], checksum[12], checksum[13], checksum[14], checksum[15], checksum[16], checksum[17], checksum[18], checksum[19], checksum[20], checksum[21], checksum[22], checksum[23], checksum[24], checksum[25], checksum[26], checksum[27], checksum[28], checksum[29], checksum[30], checksum[31]);
                state->result = KPKG_HASH_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }
        }
        return realsize;
    case DOWNLOAD_FULL:
        state->kpmio->log(KPM_VERBOSITY_DEBUG, "Writing full");
        while (state->cur_range_idx < state->range_count)
        {
            state->kpmio->log(KPM_VERBOSITY_DEBUG, "Checking range %i", state->cur_range_idx);
            current_range = state->ranges[state->cur_range_idx];
            read_remaining = realsize - offset;
            out.pos = 0;

            if (state->total_downloaded + read_remaining < current_range.start)
            {
                state->kpmio->log(KPM_VERBOSITY_DEBUG, "Not enough downloaded (%i/%i)");
                state->total_downloaded += read_remaining;
                return realsize;
            }

            if (state->total_downloaded < current_range.start)
            {
                uint64_t delta = current_range.start - state->total_downloaded;
                state->total_downloaded += delta;
                offset += delta;
                read_remaining -= delta;
            }

            if (state->cur_file == -1)
            {
                state->kpmio->log(KPM_VERBOSITY_DEBUG, "Writing to %s", current_range.out_path);
                state->cur_file = open(current_range.out_path, O_WRONLY|O_CREAT, current_range.file_entry.file_mode);
                if (state->cur_file == -1)
                {
                    state->kpmio->log(KPM_VERBOSITY_ERROR, "Could not open %s for writing", current_range.out_path);
                    state->result = KPKG_FILESYSTEM_ERROR;
                    return CURL_WRITEFUNC_ERROR;
                }
                state->cur_zstream = ZSTD_createDStream();
                state->cur_evp_ctx = EVP_MD_CTX_new();
                if (!EVP_DigestInit_ex2(state->cur_evp_ctx, EVP_sha256(), NULL))
                {
                    state->result = KPKG_OPENSSL_ERROR;
                    return CURL_WRITEFUNC_ERROR;
                }
            }

            ZSTD_inBuffer in;
            if (state->total_downloaded + read_remaining >= current_range.end)
                in = (ZSTD_inBuffer) { ptr + offset, current_range.end - state->total_downloaded, 0 };
            else
                in = (ZSTD_inBuffer) { ptr + offset, read_remaining, 0 };

            if (!EVP_DigestUpdate(state->cur_evp_ctx, in.src, in.size))
            {
                state->result = KPKG_OPENSSL_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }
            
            while (in.pos < in.size)
                if (ZSTD_isError(
                    ZSTD_decompressStream(
                            state->cur_zstream,
                            &out,
                            &in
                    )
                ))
                {
                    state->result = KPKG_ZSTD_ERROR;
                    return CURL_WRITEFUNC_ERROR;
                }
                
            state->total_downloaded += in.pos;
            offset += in.pos;
            if (write(state->cur_file, state->out_buf, out.pos) != out.pos)
            {
                state->result = KPKG_FILESYSTEM_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }

            state->kpmio->log(KPM_VERBOSITY_DEBUG, "Dowloaded %i/%i", state->total_downloaded, current_range.end);
            if (state->total_downloaded < current_range.end)
                return realsize;

            unsigned char checksum[EVP_MAX_MD_SIZE];
            unsigned int checksum_len;
            if (!EVP_DigestFinal_ex(state->cur_evp_ctx, checksum, &checksum_len) || checksum_len != KPKG_CHECKSUM_SIZE)
            {
                state->result = KPKG_OPENSSL_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }

            if (memcmp(current_range.file_entry.checksum, checksum, KPKG_CHECKSUM_SIZE))
            {
                state->kpmio->log(KPM_VERBOSITY_ERROR, "Hash mismatch error: Expected %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x, got %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", current_range.file_entry.checksum[0], current_range.file_entry.checksum[1], current_range.file_entry.checksum[2], current_range.file_entry.checksum[3], current_range.file_entry.checksum[4], current_range.file_entry.checksum[5], current_range.file_entry.checksum[6], current_range.file_entry.checksum[7], current_range.file_entry.checksum[8], current_range.file_entry.checksum[9], current_range.file_entry.checksum[10], current_range.file_entry.checksum[11], current_range.file_entry.checksum[12], current_range.file_entry.checksum[13], current_range.file_entry.checksum[14], current_range.file_entry.checksum[15], current_range.file_entry.checksum[16], current_range.file_entry.checksum[17], current_range.file_entry.checksum[18], current_range.file_entry.checksum[19], current_range.file_entry.checksum[20], current_range.file_entry.checksum[21], current_range.file_entry.checksum[22], current_range.file_entry.checksum[23], current_range.file_entry.checksum[24], current_range.file_entry.checksum[25], current_range.file_entry.checksum[26], current_range.file_entry.checksum[27], current_range.file_entry.checksum[28], current_range.file_entry.checksum[29], current_range.file_entry.checksum[30], current_range.file_entry.checksum[31], checksum[0], checksum[1], checksum[2], checksum[3], checksum[4], checksum[5], checksum[6], checksum[7], checksum[8], checksum[9], checksum[10], checksum[11], checksum[12], checksum[13], checksum[14], checksum[15], checksum[16], checksum[17], checksum[18], checksum[19], checksum[20], checksum[21], checksum[22], checksum[23], checksum[24], checksum[25], checksum[26], checksum[27], checksum[28], checksum[29], checksum[30], checksum[31]);
                state->result = KPKG_HASH_ERROR;
                return CURL_WRITEFUNC_ERROR;
            }
            
            close(state->cur_file);
            ZSTD_freeDStream(state->cur_zstream);
            EVP_MD_CTX_free(state->cur_evp_ctx);
            state->cur_file = -1;
            state->cur_zstream = NULL;
            state->cur_evp_ctx = NULL;
            state->cur_range_idx++;
        }
        return realsize;
    case DOWNLOAD_MULTIRANGE:
        state->result = KPKG_NOTIMPL_ERROR;
        return CURL_WRITEFUNC_ERROR; // Lower priority since single-range request overhead is minimal + AWS S3 buckets don't't support this
    }
}

KPKG_Result KPKG3_HandleRangeMap(KPKG_File *file, uint32_t range_count, KPKG3_DownloadRange *ranges, KPMIO *kpmio)
{
    if (file->kpkg3_payload.mapped_size != -1)
    {
        for (int i=0; i < range_count; i++)
        {
            KPKG3_DownloadRange range = ranges[i];
            kpmio->log(KPM_VERBOSITY_DEBUG, "Decompressing %s to %s", KPKG3_GetString(&file->kpkg3_payload, range.file_entry.path), range.out_path);
            int fd = open(range.out_path, O_RDWR|O_CREAT, range.file_entry.file_mode);
            if (fd == -1)
                return KPKG_FILESYSTEM_ERROR;
            if (ftruncate(fd, range.file_entry.installed_size) == -1)
                return KPKG_FILESYSTEM_ERROR;
            
            void *mapped_file = mmap(NULL, range.file_entry.installed_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
            close(fd);
            if (mapped_file == MAP_FAILED)
            {
                kpmio->log(KPM_VERBOSITY_ERROR, "Could not map file (%s)", strerror(errno));
                return KPKG_MMAP_ERROR;
            }
            unsigned int zstd_error = ZSTD_isError(
                ZSTD_decompress(
                    mapped_file, 
                    range.file_entry.installed_size, 
                    file->kpkg3_payload.internal_file + range.start,
                    range.end - range.start
                )
            );
            munmap(mapped_file, range.file_entry.installed_size);

            if (zstd_error)
                return KPKG_ZSTD_ERROR;
        }
        return KPKG_OK;
    }

    for (int i=0; i < range_count; i++)
    {
        kpmio->log(KPM_VERBOSITY_DEBUG, "Downloading %s", KPKG3_GetString(&file->kpkg3_payload, file->kpkg3_payload.files[i].path));
        KPKG3_DownloadRange range = ranges[i];
        
        KPKG3_DownloadState state = {
            .type = DOWNLOAD_SINGLERANGE,
            .result = KPKG_OK,
            .range_count = 1,
            .ranges = &range,
            .out_buf = malloc(ZSTD_DStreamOutSize()),
            .out_buf_size = ZSTD_DStreamOutSize(),
            .cur_range_idx = 0,
            .cur_evp_ctx = NULL,
            .total_downloaded = 0,
            .total_size = 0,
            .cur_file = -1,
            .cur_zstream = NULL,
            .kpmio = kpmio
        };

        char *ranges_str;
        if (range_count == file->kpkg3_payload.header->file_count)
        {
            state.type = DOWNLOAD_FULL;
            state.range_count = range_count;
            state.ranges = ranges;
            ranges_str = asprintf_hd("%lu-%lu", 0L, ranges[range_count-1].end);
        }
        else
        {
            ranges_str = asprintf_hd("%lu-%lu", range.start, range.end - 1);
        }
            

        CURL *curl = curl_easy_init();
        KPM_SET_CURL_OPTIONS(curl);
        curl_easy_setopt(curl, CURLOPT_URL, file->location);
        curl_easy_setopt(curl, CURLOPT_RANGE, ranges_str);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, KPKG3_CurlWriteFunction);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&state);
        CURLcode result = curl_easy_perform(curl);
        free(ranges_str);
        free(state.out_buf);
        if (state.cur_file != -1)
            close(state.cur_file);
        ZSTD_freeDStream(state.cur_zstream);
        EVP_MD_CTX_free(state.cur_evp_ctx);
        curl_easy_cleanup(curl);
        if (result != CURLE_OK && state.result == KPKG_OK)
            return KPKG_GENERIC_ERROR;
        if (state.result != KPKG_OK)
            return state.result;

        if (range_count == file->kpkg3_payload.header->file_count)
            return state.result;
    }

    return KPKG_OK;
}

KPKG_Result KPKG3_Unpack(KPKG_File *file, const char *out_path, const char *cmp_path, KPMIO *kpmio)
{
    assert(file->kpkg_version != KPKG_VERSION_1 && file->kpkg_version != KPKG_VERSION_2);
    uint64_t file_section_off = sizeof(KPKG3_Header);
    file_section_off += sizeof(KPKG3_DependencyEntry) * file->kpkg3_payload.header->dependency_count;
    file_section_off += sizeof(KPKG3_FileEntry) * file->kpkg3_payload.header->file_count;
    file_section_off += file->kpkg3_payload.header->strings_size;

    kpmio->log(KPM_VERBOSITY_DEBUG, "Generating KPKG3 ranges...");
    KPKG3_DownloadRange *ranges = malloc(sizeof(KPKG3_DownloadRange) * file->kpkg3_payload.header->file_count);
    uint32_t range_count = 0;
    for (uint32_t i=0; i < file->kpkg3_payload.header->file_count; i++)
    {
        KPKG3_FileEntry file_entry = file->kpkg3_payload.files[i];
        char *entry_cmp_path = asprintf_hd("%s/%s", cmp_path, KPKG3_GetString(&file->kpkg3_payload, file_entry.path));
        FILE *cmp_file = fopen(entry_cmp_path, "r");
        free(entry_cmp_path);

        unsigned char installed_checksum[EVP_MAX_MD_SIZE];
        unsigned int checksum_len = 0;
        if (cmp_file)
        {
            kpmio->log(KPM_VERBOSITY_DEBUG, "Calculating hash for %s", KPKG3_GetString(&file->kpkg3_payload, file_entry.path));
            EVP_MD_CTX *ctx = EVP_MD_CTX_new();
            if (!EVP_DigestInit_ex2(ctx, EVP_sha256(), NULL))
            {
                EVP_MD_CTX_free(ctx);
                fclose(cmp_file);
                KPKG3_FreeDownloadRanges(range_count, ranges);
                return KPKG_OPENSSL_ERROR;
            }

            char buf[8192];
            size_t buf_size = 0;
            while ((buf_size = fread(buf, 1, 8192, cmp_file)) > 0)
            {
                if (!EVP_DigestUpdate(ctx, buf, buf_size))
                {
                    EVP_MD_CTX_free(ctx);
                    fclose(cmp_file);
                    KPKG3_FreeDownloadRanges(range_count, ranges);
                    return KPKG_OPENSSL_ERROR;
                }
            }

            if (!feof(cmp_file))
            {
                EVP_MD_CTX_free(ctx);
                fclose(cmp_file);
                KPKG3_FreeDownloadRanges(range_count, ranges);
                return KPKG_FILESYSTEM_ERROR;
            }

            if (!EVP_DigestFinal_ex(ctx, installed_checksum, &checksum_len) || checksum_len != KPKG_CHECKSUM_SIZE)
            {
                EVP_MD_CTX_free(ctx);
                fclose(cmp_file);
                KPKG3_FreeDownloadRanges(range_count, ranges);
                return KPKG_OPENSSL_ERROR;
            }
            EVP_MD_CTX_free(ctx);
            fclose(cmp_file);
        }

        char *out_file_path = asprintf_hd("%s/%s", out_path, KPKG3_GetString(&file->kpkg3_payload, file_entry.path));
        char *dir_src_path = strdup(out_file_path);
        char *dir_path = dirname(dir_src_path);
        if (access(dir_path, R_OK))
            mkdir_r(dir_path, 0755);
        free(dir_src_path);

        if (checksum_len == 0 || memcmp(file_entry.installed_checksum, installed_checksum, KPKG_CHECKSUM_SIZE))
        {
            if (checksum_len != 0)
                kpmio->log(KPM_VERBOSITY_DEBUG, "Hash mismatch for %s, expected %s got %s, marking for upgrade", KPKG3_GetString(&file->kpkg3_payload, file_entry.path), file_entry.installed_checksum, installed_checksum);
            else
                kpmio->log(KPM_VERBOSITY_DEBUG, "File %s not found, marking for upgrade", KPKG3_GetString(&file->kpkg3_payload, file_entry.path));

            ranges[range_count++] = (KPKG3_DownloadRange) {
                .file_entry = file_entry,
                .start = file_entry.offset + file_section_off,
                .end = file_entry.offset + file_section_off + file_entry.size,
                .out_path = out_file_path
            };
            continue;
        }

        // Copy unchanged file
        kpmio->log(KPM_VERBOSITY_DEBUG, "File matches, copying");
        char *og_path = asprintf_hd("%s/%s", cmp_path, KPKG3_GetString(&file->kpkg3_payload, file_entry.path));
        FILE *og_file = fopen(og_path, "r");
        free(og_path);
        if (!og_file)
        {
            KPKG3_FreeDownloadRanges(range_count, ranges);
            return KPKG_FILESYSTEM_ERROR;
        }
        FILE *out_file = fopen(out_file_path, "w");
        free(out_file_path);
        if (!out_file)
        {
            fclose(og_file);
            KPKG3_FreeDownloadRanges(range_count, ranges);
            return KPKG_FILESYSTEM_ERROR;
        }
        struct stat stats;
        fstat(fileno(og_file), &stats);
        if (sendfile(fileno(out_file), fileno(og_file), 0, stats.st_size) != stats.st_size)
        {
            fclose(og_file);
            fclose(out_file);
            KPKG3_FreeDownloadRanges(range_count, ranges);
            return KPKG_FILESYSTEM_ERROR;
        }
    }

    KPKG_Result ret = KPKG3_HandleRangeMap(file, range_count, ranges, kpmio);
    KPKG3_FreeDownloadRanges(range_count, ranges);
    return ret;
}