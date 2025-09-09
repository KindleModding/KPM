#include "archive.h"
#include "archive_entry.h"
#include "cjson/cJSON.h"
#include "kpm/kpm.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "callback.h"

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

enum KPMResult Internal_ExtractArchive(char* path, char* out, KPMStatusCallback* statusCallback)
{
    if (access(path, R_OK) != 0)
    {
        return KPM_FILE_SYSTEM_ERROR;
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

    if ((r = archive_read_open_filename(a, path, 10240)))
    {
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
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Libarchive error: %s", archive_error_string(a));
        }
        if (r < ARCHIVE_WARN)
        {
            goto libarchive_error;
        }

        char* entryPath = malloc(strlen(archive_entry_pathname(entry)) + strlen(out) + 1 + 1);
        sprintf(entryPath, "%s/%s", out, archive_entry_pathname(entry));
        archive_entry_set_pathname(entry, entryPath);
        r = archive_write_header(ext, entry);
        free(entryPath);

        if (r < ARCHIVE_OK)
        {
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Libarchive error: %s", archive_error_string(a));
        }
        else if (archive_entry_size(entry) > 0)
        {
            r = copy_data(a, ext);
            if (r < ARCHIVE_OK)
            {
                statusCallback(KPM_VERBOSITY_ERROR, 0, "Libarchive error: %s", archive_error_string(a));
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
    return KPM_OK;
libarchive_error:
    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);
    return KPM_LIBARCHIVE_ERROR;
}

void Internal_GetDependencies(struct InstallTarget* target, KPMStatusCallback *statusCallback)
{
    if (strncmp(target->id, "file://", strlen("file://")) == 0)
    {
        // It's a local file and we should die
        char* temporaryPath = "/tmp/kpm"; // @TODO: do a thing
        mkdir("/tmp/kpm", 0666); // @TODO: Do properly
        int extractResult = Internal_ExtractArchive(target->id+strlen("file://"), temporaryPath, statusCallback);
        printf("%i\n", extractResult);

        char* manifestPath = malloc(strlen(temporaryPath) + strlen("/manifest.json") + 1);
        sprintf(manifestPath, "%s/manifest.json", temporaryPath);

        struct stat statBuffer;
        stat(manifestPath, &statBuffer);
        int fd = open(manifestPath, O_RDONLY);
        char* file  = mmap(NULL, statBuffer.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        
        printf("%s\n", file);
        //cJSON* json = cJSON_Parse(file);

        close(fd);
        free(manifestPath);
    }

}

/**
 * @brief Algorithm plan
 *

 Do we need separate code-paths for local installation :think:

 1. Resolve manifest dependencies - We need to get a package object either by extracting or querying
 2. List dependencies recursively
 3. Once flat list of dependency objects is built (wait a second-) resolve the dependency boundaries
 4. Check if dependencies are already installed
 5. Create a list of install targets
 6. Run internal installer
 */

enum KPMResult KPM_InstallPackage(struct KPM* kpm, struct InstallTarget* target, KPMStatusCallback* statusCallback)
{
    if (statusCallback == NULL)
    {
        statusCallback = dummyCallback;
    }

    // Get the dependencies
    Internal_GetDependencies(target, statusCallback);

    return KPM_OK;
}