#include "kpm/kpkg.h"
#include "curl/curl.h"
#include "simpleGET.h"
#include "internal_utils.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "zstd.h"

KPKG_File* KPKG_LoadFile(const char* location)
{
    char* path = get_realpath(location);
    if (path)
    {
        int fd = open(path, O_RDONLY);
        if (!fd)
        {
            free(path);
            return NULL;
        }

        struct stat buf;
        fstat(fd, &buf);

        KPKG_File* file = malloc(sizeof(KPKG_File));
        *file = (KPKG_File) {
            .location = path,
            .map_size = buf.st_size,
            .internal_file = mmap(NULL, file->map_size, PROT_READ, MAP_SHARED, fd, 0)
        };
        
        close(fd);
        if (!file->internal_file)
        {
            free(file);
            free(path);
            return NULL;
        }

        file->header = file->internal_file;
        file->dependencies = file->internal_file + sizeof(KPKG_Header);
        file->files = file->internal_file + sizeof(KPKG_Header) +
                        (sizeof(KPKG_DependencyEntry) * file->header->dependency_count);
        file->strings = file->internal_file + sizeof(KPKG_Header) +
                        (sizeof(KPKG_DependencyEntry) * file->header->dependency_count) +
                        (sizeof(KPKG_FileEntry) * file->header->file_count);

        // Validate dependency, file and string sections do not OOB
        if (
            sizeof(KPKG_Header) +
            (sizeof(KPKG_DependencyEntry) * file->header->dependency_count) +
            (sizeof(KPKG_FileEntry) * file->header->file_count) +
            file->header->strings_size > buf.st_size
            ||
            // Validate the final file entry doesn't OOB
            file->files[file->header->file_count-1].offset > buf.st_size
        )
        {
            close(fd);
            free(path);
            free(file);
            return NULL;
        }

        return file;
    }
    else
    {
        // This is a URL

        // Fetch the first part of the header
        SimpleGETRequest* request = SimpleGET_Initialise(location);
        request->max_size = sizeof(KPKG_Header);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, asprintf_hd("Range: bytes=0-%zi", sizeof(KPKG_Header)+1));
        curl_easy_setopt(request->curl, CURLOPT_HTTPHEADER, headers);

        SimpleGET_Perform(request);
        curl_slist_free_all(headers);
        if (request->response_code != 206 || request->size != sizeof(KPKG_Header)) // @TODO: Handle stupid webservers that can't handle range requests, honestly we should be running our own CURL callback here
        {
            SimpleGET_Cleanup(request);
            return NULL;
        }

        KPKG_Header* header = (KPKG_Header*) request->buffer; // Steal the request buffer instead of doing memcpy shennanigans
        
        request->buffer = NULL; // Do not free buffer
        SimpleGET_Cleanup(request);
        if (
            strncmp(header->magic, KPKG_MAGIC, KPKG_MAGIC_LENGTH) != 0 ||
            header->version != KPKG_VERSION // @TODO: We can even validate the platform here, and we SHOULD!
        )
        {
            free(header);
            return NULL;
        }

            
        // Fetch the second half of the header
        request = SimpleGET_Initialise(location);
        request->max_size = header->dependency_count * sizeof(KPKG_DependencyEntry) +
                                header->file_count * sizeof(KPKG_FileEntry) +
                                header->strings_size;

        headers = NULL;
        headers = curl_slist_append(headers, asprintf_hd("Range: bytes=%zi-%zi",
            sizeof(KPKG_Header) + 1,
            header->dependency_count * sizeof(KPKG_DependencyEntry) +
                header->file_count * sizeof(KPKG_FileEntry) +
                header->strings_size + 1
        ));
        curl_easy_setopt(request->curl, CURLOPT_HTTPHEADER, headers);

        SimpleGET_Perform(request);
        curl_slist_free_all(headers);
        if (
            request->response_code != 206 ||
            request->size != header->dependency_count * sizeof(KPKG_DependencyEntry) +
                                header->file_count * sizeof(KPKG_FileEntry) +
                                header->strings_size
        )
        {
            free(header);
            SimpleGET_Cleanup(request);
            return NULL;
        }

        
        // Construct file from response
        KPKG_File* file = malloc(sizeof(KPKG_File));
        *file = (KPKG_File){
            .location = strdup(location),
            .internal_file = NULL,
            .map_size = 0,
            .header = header,
            .dependencies = (KPKG_DependencyEntry*) request->buffer,
            .files = (KPKG_FileEntry*) (request->buffer + header->dependency_count * sizeof(KPKG_DependencyEntry)),
            .strings = (char*) (request->buffer + header->dependency_count * sizeof(KPKG_DependencyEntry) + header->file_count * sizeof(KPKG_FileEntry))
        };

        request->buffer = NULL; // Do not free buffer
        SimpleGET_Cleanup(request);

        return file;
    }
}

void KPKG_CloseFile(KPKG_File* file)
{
    free(file->location);
    if (file->internal_file)
    {
        munmap(file->internal_file, file->map_size);
    }
    else
    {
        free(file->header);
        free(file->dependencies);
        free(file->files);
        free(file->strings);
    }
    free(file);
}

bool KPKG_ValidateFile(KPKG_File* kpkg_file)
{
    // Does a few further checks that LoadFile doesn't
    if (kpkg_file->strings[kpkg_file->header->strings_size - 1] != 0)
        return false; // String section not null-terminated

    for (int i=0; i < kpkg_file->header->dependency_count; i++)
        if (kpkg_file->dependencies[i].id >= kpkg_file->header->strings_size)
            return false; // Dependency id OOB

    for (int i=0; i < kpkg_file->header->file_count; i++)
        if (kpkg_file->files[i].path >= kpkg_file->header->strings_size)
            return false; // File path OOB

    if (kpkg_file->internal_file)
    {
        __off_t file_section_size = kpkg_file->map_size - (sizeof(KPKG_Header) +
                                    kpkg_file->header->dependency_count * sizeof(KPKG_DependencyEntry) +
                                    kpkg_file->header->file_count * sizeof(KPKG_FileEntry) +
                                    kpkg_file->header->strings_size);

        uint64_t last_offset = 0;
        for (int i=0; i < kpkg_file->header->file_count; last_offset = kpkg_file->files[i].offset, i++)
            if (kpkg_file->files[i].offset >= file_section_size || kpkg_file->files[i].offset <= last_offset)
                return false; // File content OOB, or offsets invalid
    }

    return true;
}

char* KPKG_GetString(KPKG_File* kpkg_file, uint64_t offset)
{
    return kpkg_file->strings + offset;
}

KPKG_DependencyEntry* KPKG_GetDependencies(KPKG_File* kpkg_file, size_t* count)
{
    *count = kpkg_file->header->dependency_count;
    return kpkg_file->dependencies;
}

KPKG_FileEntry* KPKG_GetFileEntries(KPKG_File* kpkg_file, size_t* count)
{
    *count = kpkg_file->header->file_count;
    return kpkg_file->files;
}

KPKG_Result KPKG_ExtractData(void* ptr, uint64_t size, uint64_t out_size, char* out_path)
{
    int fd = open(out_path, O_WRONLY);
    AssertOrReturn(fd != 0, 0, KPKG_FS_ERROR);
    AssertOrReturn(
        ftruncate(fd, out_size),
        0,
        KPKG_FS_ERROR
    )
    void* file = mmap(NULL, out_size, PROT_WRITE, MAP_PRIVATE, fd, 0);
    AssertOrReturn(file != NULL, 0, KPKG_MMAP_ERROR);
    AssertOrReturn(
        ZSTD_isError(
            ZSTD_decompress(
                file, 
                out_size, 
                ptr + out_size,
                size
            )
        ),
        0,
        KPKG_ZSTD_ERROR
    );
    AssertOrReturn(munmap(file, out_size), 0, KPKG_MMAP_ERROR);
    return KPKG_OK;
}