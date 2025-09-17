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
#include "dependencies.h"
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



enum KPMResult KPM_InstallPackage(struct KPM* kpm, struct InstallTarget* target, KPMStatusCallback* statusCallback)
{
    if (statusCallback == NULL)
    {
        statusCallback = dummyCallback;
    }

    struct IndexedArtifact artifact;
    if (strncmp(target->id, "file://", strlen("file://")) == 0)
    {
        // We must index this artifact under the localhost repository
        // We do this because it simplifies so much logic
        char* outBuffer = NULL;
        int status;
        if ((status = Internal_GetManifest(target->id, &outBuffer, statusCallback)) != KPM_OK)
        {
            free(outBuffer);
            return status;
        }
    }
    else
    {
        if (target->version != NULL)
        {
            if (KPM_GetArtifact(kpm, target->repository, target->id, *target->version, &artifact) != KPM_OK)
            {
                statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not find artifact for given target.");
                return KPM_GENERIC_ERROR;
            }
        }
        else
        {
            size_t artifactCount;
            struct IndexedArtifact* artifacts;
            if (KPM_ListPackageArtifacts(kpm, target->repository, target->id, &artifactCount, &artifacts) != KPM_OK || artifactCount == 0)
            {
                statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not find artifact for given target.");
                return KPM_GENERIC_ERROR;
            }

            artifact.id = strdup(artifacts[0].id);
            artifact.repository = strdup(artifacts[0].repository);
            artifact.url = strdup(artifacts[0].url);
            artifact.version = artifacts[0].version;
            KPM_FreeIndexedArtifactList(artifactCount, artifacts);
        }
    }

    // Construct a graph
    struct DependencyGraph graph;
    CreateDependencyGraph(&graph, 0);

    struct DependencyNode rootNode = {
        .type = NODE_ARTIFACT,
        .connected = NULL,
        .connectedCount = 0,
        .id = strdup(""),
        .repository = strdup(""),
    };

    int rootId = AddNode(&graph, rootNode);

    // Add installed packages to the graph
    size_t installedPackageCount;
    struct InstalledPackage *installedPackages;
    KPM_ListInstalledPackages(kpm, &installedPackageCount, &installedPackages);

    for (size_t i=0; i < installedPackageCount; i++)
    {
        struct DependencyNode depNode = {
            .type = NODE_DEPENDENCY,
            .connected = NULL,
            .connectedCount = 0,
            .id = strdup(installedPackages[i].id),
            .repository = strdup("")
        };
        int depId = AddNode(&graph, depNode);
        AddEdge(&graph, rootId, depId);

        struct IndexedArtifact fakeArtifact = {
            .id = strdup(installedPackages[i].id),
            .repository = strdup("localhost"),
            .url = strdup(installedPackages[i].id),
            .version = installedPackages[i].version
        };

        size_t constructedId = Internal_ConstructGraphFromArtifact(kpm, &graph, &fakeArtifact);
        KPM_FreeIndexedArtifact(&fakeArtifact);
        AddEdge(&graph, depId, constructedId);
    }

    // Add the target to the graph
    struct DependencyNode depNode = {
        .type = NODE_DEPENDENCY,
        .connected = NULL,
        .connectedCount = 0,
        .id = strdup(target->id),
        .repository = strdup(target->repository)
    };
    int depId = AddNode(&graph, depNode);
    AddEdge(&graph, rootId, depId);

    size_t constructedId = Internal_ConstructGraphFromArtifact(kpm, &graph, &artifact);
    AddEdge(&graph, depId, constructedId);

    // Ok now everything is done...
    // It's time
    // Deploy the [thingamajjig]
    size_t traversedNodeCount;
    size_t* traversedNodes;
    if (Internal_ResolveDependencyGraph(&graph, rootId, &traversedNodeCount, &traversedNodes, statusCallback) != KPM_OK)
    {
        statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not resolve dependency graph.");
        statusCallback(KPM_VERBOSITY_ERROR, 0, "If you believe this is a mistake - please submit an issue");
        return KPM_GENERIC_ERROR;
    }

    // Now we have a flattened list of artifacts to install
    // We must first download them ALL



    return KPM_OK;
}
