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
        file->location = path;
        file->map_size = buf.st_size;
        file->internal_file = mmap(NULL, file->map_size, PROT_READ, MAP_SHARED, fd, 0);
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

        // Validate dependency, file and string sections exist
        if (
            sizeof(KPKG_Header) +
            (sizeof(KPKG_DependencyEntry) * file->header->dependency_count) +
            (sizeof(KPKG_FileEntry) * file->header->file_count) +
            file->header->strings_size < buf.st_size
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

        // Fetch the first half of the header
        SimpleGETRequest* request = SimpleGET_Initialise(location);
        request->max_size = sizeof(KPKG_Header);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, asprintf_hd("Range: bytes=0-%zi", sizeof(KPKG_Header)+1));
        curl_easy_setopt(request->curl, CURLOPT_HTTPHEADER, headers);

        SimpleGET_Perform(request);
        curl_slist_free_all(headers);
        if (request->response_code != 206) // @TODO: Handle stupid webservers that can't handle range requests, honestly we should be running our own CURL callback here
        {
            SimpleGET_Cleanup(request);
            return NULL;
        }

        KPKG_Header* header = (KPKG_Header*) request->buffer; // Steal the request buffer instead of doing memcpy shennanigans
        request->buffer = NULL;
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
        file->location = strdup(location);
        file->internal_file = NULL;
        file->map_size = 0;
        file->header = header;
        file->dependencies = NULL;
        file->files = NULL;
        file->strings = NULL;

        file->dependencies = malloc(header->dependency_count * sizeof(KPKG_DependencyEntry));
        memcpy(
            file->dependencies, 
            request->buffer, 
            header->dependency_count * sizeof(KPKG_DependencyEntry)
        );

        file->files = malloc(header->file_count * sizeof(KPKG_FileEntry));
        memcpy(
            file->files, 
            request->buffer + 
                header->dependency_count * sizeof(KPKG_DependencyEntry), 
            header->file_count * sizeof(KPKG_FileEntry)
        );

        file->strings = malloc(header->strings_size);
        memcpy(
            file->strings, 
            request->buffer +
                header->dependency_count * sizeof(KPKG_DependencyEntry) +
                header->file_count * sizeof(KPKG_FileEntry),
            header->strings_size
        );
        
        request->buffer = NULL;
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
                            
        for (int i=0; i < kpkg_file->header->file_count; i++)
            if (kpkg_file->files[i].offset >= file_section_size)
                return false; // File content OOB
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
