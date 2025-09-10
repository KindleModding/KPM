#include "archive.h"
#include "archive_entry.h"
#include "cjson/cJSON.h"
#include "kpm/kpm.h"
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "callback.h"
#include "kpm/semver.h"
#include "install.h"

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
        statusCallback(KPM_VERBOSITY_ERROR, 0, "Cold not open file %s", path);
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

enum KPMResult Internal_GetManifest(char* path, char** outBuffer, KPMStatusCallback* statusCallback)
{
    if (statusCallback == NULL)
    {
        statusCallback = dummyCallback;
    }

    *outBuffer = NULL;
    if (access(path, R_OK) != 0)
    {
        return KPM_FILE_SYSTEM_ERROR;
    }

    // Read the file
    struct archive_entry* entry;

    struct archive* a = archive_read_new();
    int r;
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);

    if ((r = archive_read_open_filename(a, path, 10240)))
    {
        statusCallback(KPM_VERBOSITY_ERROR, 0, "Cold not open file %s", path);
        archive_read_close(a);
        archive_read_free(a);
        return KPM_LIBARCHIVE_ERROR;
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
            archive_read_close(a);
            archive_read_free(a);
            return KPM_LIBARCHIVE_ERROR;
        }

        if (strcmp(archive_entry_pathname(entry), "manifest.json") != 0 && strcmp(archive_entry_pathname(entry), "/manifest.json") != 0)
        {
            continue;
        }

        *outBuffer = malloc(archive_entry_size(entry));
        const void* blockBuffer;
        size_t blockSize;
        la_int64_t offset;
        int r = 0;
        for (;;)
        {
            r = archive_read_data_block(a, &blockBuffer, &blockSize, &offset);
            memcpy((*outBuffer)+offset, blockBuffer, blockSize);
            if (r == ARCHIVE_EOF || r < ARCHIVE_OK)
            {
                break;
            }
        }

        if (r < ARCHIVE_OK)
        {
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Libarchive error: %s", archive_error_string(a));
            free(*outBuffer);
            *outBuffer = NULL; // Triggers error case
        }
        break;
    }

    archive_read_close(a);
    archive_read_free(a);

    if (*outBuffer == NULL)
    {
        return KPM_LIBARCHIVE_ERROR;
    }
        
    return KPM_OK;
}

/**
 * @brief Alters the current dependency to be restricted so that the targetDependency is ALSO met (if possible)
 * 
 * @param currentDependency 
 * @param targetDependency 
 * @return enum KPMResult 
 */
bool Internal_NarrowDependency(struct ArtifactDependency* currentDependency, struct ArtifactDependency* targetDependency)
{
    if ((currentDependency->min_version != NULL && SemVerCmp(*currentDependency->min_version, *targetDependency->min_version) > 0) ||
        (currentDependency->max_version != NULL && SemVerCmp(*currentDependency->max_version, *targetDependency->max_version) <= 0))
    {
        return false;
    }
    else
    {
        currentDependency->min_version = targetDependency->min_version;
        currentDependency->max_version = targetDependency->max_version;
        return true;
    }
}

/**
 * @brief Add a package and its dependency to a series of flattened artifacts
 * 
 * @param target 
 * @param dependencyCount 
 * @param flattenedDependencies 
 * @param statusCallback 
 * @return enum KPMResult 
 */
enum KPMResult Internal_AddInstallTarget(struct KPM *kpm, struct InstallTarget* target, size_t* flattenedDependencyCount, struct ArtifactDependency** flattenedDependencies, KPMStatusCallback *statusCallback)
{
    size_t targetDependencyCount = 0;
    struct ArtifactDependency* targetDependencies;

    if (strncmp(target->id, "file://", strlen("file://")) == 0)
    {
        char* manifestData;
        enum KPMResult result = Internal_GetManifest(target->id+strlen("file://"), &manifestData, NULL);
        if (result != KPM_OK)
        {
            free(manifestData);
            return result;
        }

        cJSON* json = cJSON_Parse(manifestData);
        if (!cJSON_IsArray(cJSON_GetObjectItem(json, "dependencies")) ||
            cJSON_GetArraySize(cJSON_GetObjectItem(json, "dependencies")) == 0)
        {
            free(manifestData);
            cJSON_Delete(json);

            if (cJSON_GetObjectItem(json, "dependencies") == NULL || cJSON_IsArray(cJSON_GetObjectItem(json, "dependencies")))
            {
                return KPM_OK;
            }
            else {
                return KPM_PARSE_ERROR;
            }
        }

        targetDependencyCount = cJSON_GetArraySize(cJSON_GetObjectItem(json, "dependencies"));
        targetDependencies = malloc(targetDependencyCount * sizeof(struct ArtifactDependency));
        for (size_t i=0; i < targetDependencyCount; i++)
        {
            cJSON* dependencyJSON = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "dependencies"), i);
            if (!cJSON_IsString(cJSON_GetObjectItem(dependencyJSON, "id")))
            {
                free(*flattenedDependencies);
                free(manifestData);
                cJSON_Delete(json);
                KPM_FreeArtifactDependencyList(targetDependencyCount, targetDependencies);
                return KPM_PARSE_ERROR;
            }

            targetDependencies[i].artifact = target->id;
            targetDependencies[i].id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "id"))); // 90% sure I don't need to free this
            targetDependencies[i].repository = NULL;

            if (cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "repository")) != NULL)
            {
                targetDependencies[i].repository = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "repository")));
            }

            cJSON* min = cJSON_GetObjectItem(dependencyJSON, "min");
            cJSON* max = cJSON_GetObjectItem(dependencyJSON, "max");
            if (min != NULL && cJSON_GetArraySize(min) == 3)
            {
                targetDependencies[i].min_version->major = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 0));
                targetDependencies[i].min_version->minor = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 1));
                targetDependencies[i].min_version->patch = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 2));
            }
            else
            {
                targetDependencies[i].min_version = NULL;
            }

            if (max != NULL && cJSON_GetArraySize(max) == 3)
            {
                targetDependencies[i].max_version->major = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 0));
                targetDependencies[i].max_version->minor = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 1));
                targetDependencies[i].max_version->patch = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 2));
            }
            else
            {
                targetDependencies[i].max_version = NULL;
            }
        }

        free(manifestData);
        cJSON_Delete(json);
    }
    else
    {
        struct IndexedArtifact artifact;
        KPM_GetArtifact(kpm, target->repository, target->id, target->version, &artifact);
        KPM_ListArtifactDependencies(kpm, artifact.url, &targetDependencyCount, &targetDependencies);
    }

    // Parse the deps
    if (*flattenedDependencies == NULL)
    {
        *flattenedDependencies = malloc(targetDependencyCount * sizeof(struct ArtifactDependency));
    }
    else {
        *flattenedDependencies = realloc(*flattenedDependencies, *flattenedDependencyCount + targetDependencyCount * sizeof(struct ArtifactDependency));
    }

    for (size_t i=0; i < targetDependencyCount; i++)
    {
        bool found = false;
        for (size_t j=0; j < *flattenedDependencyCount; j++)
        {
            if (strcmp(targetDependencies[i].id, (*flattenedDependencies)[j].id) == 0)
            {
                if (targetDependencies[i].repository != NULL && flattenedDependencies[j]->repository != NULL && strcmp(targetDependencies[i].repository, flattenedDependencies[j]->repository) != 0)
                {
                    statusCallback(KPM_VERBOSITY_ERROR, 0, "Dependency [%s] is required from repository %s but is also being requested from repository %s", targetDependencies[i].id, targetDependencies[i].repository, flattenedDependencies[i]->repository);
                    KPM_FreeArtifactDependencyList(targetDependencyCount, targetDependencies);
                    return KPM_GENERIC_ERROR;
                }

                if (!Internal_NarrowDependency(&(*flattenedDependencies)[j], &targetDependencies[i]))
                {
                    // @TODO: Track which packages needed a dependency so we can say which package is causing this
                    statusCallback(KPM_VERBOSITY_ERROR, 0, "Dependency [%s] referenced by package [%s] interferes with a dependency required by another package", targetDependencies[i].id, target->id);
                    KPM_FreeArtifactDependencyList(targetDependencyCount, targetDependencies);
                    return KPM_GENERIC_ERROR;
                }
                found = true;
                break;
            }
        }

        if (!found)
        {
            flattenedDependencies[*flattenedDependencyCount + i]->artifact = strdup(targetDependencies[i].artifact);
            flattenedDependencies[*flattenedDependencyCount + i]->repository = strdup(targetDependencies[i].repository);
            flattenedDependencies[*flattenedDependencyCount + i]->id = strdup(targetDependencies[i].id);
            memcpy(flattenedDependencies[*flattenedDependencyCount + i]->min_version, targetDependencies[i].min_version, sizeof(struct SemVer));
            memcpy(flattenedDependencies[*flattenedDependencyCount + i]->max_version, targetDependencies[i].max_version, sizeof(struct SemVer));
        }
    }

    KPM_FreeArtifactDependencyList(targetDependencyCount, targetDependencies);
    return KPM_OK;
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
    size_t dependencyCount = 0;
    struct InstallTarget* dependencies = NULL;
    enum KPMResult result = Internal_GetRecursiveDependencies(target, &dependencyCount, &dependencies, statusCallback);
    if (result != KPM_OK)
    {
        return result;
    }

    return KPM_OK;
}
