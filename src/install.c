#include "archive.h"
#include "archive_entry.h"
#include "cjson/cJSON.h"
#include "kpm/kpm.h"
#include <assert.h>
#include <curl/multi.h>
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

enum KPMResult Internal_ExtractArchive(char* path, char* out, struct KPMLogging* kpmLogging)
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
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Cold not open file %s", path);
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
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
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
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
        }
        else if (archive_entry_size(entry) > 0)
        {
            r = copy_data(a, ext);
            if (r < ARCHIVE_OK)
            {
                kpmLogging->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
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

enum KPMResult Internal_GetManifest(char* path, char** outBuffer, struct KPMLogging* kpmLogging)
{
    if (kpmLogging == NULL)
    {
        kpmLogging = &dummyKPMStub;
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
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Cold not open file %s", path);
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
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
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
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
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

void Internal_TraverseInstalledNode(struct DependencyGraph* graph, NodeIndex_t node, size_t* traversedNodeCount, NodeIndex_t** traversedNodes, size_t installedCount, struct InstalledPackage* installed)
{
    // Traverse the dependencies
    for (size_t i = 0; i < graph->nodes[node].connectedCount; i++)
    {
        // Get the first artifact of this dependency that is installed
        struct DependencyNode dependency = graph->nodes[graph->nodes[node].connected[i]];
        for (size_t j=0; j < dependency.connectedCount; j++)
        {
            struct DependencyNode candidateArtifact = graph->nodes[dependency.connected[j]];

            // Check if this node is installed
            for (size_t k=0; k < installedCount; k++)
            {
                if (strcmp(candidateArtifact.id, installed[k].id) &&
                    ((strlen(installed[k].repository) == 0 && strlen(candidateArtifact.repository) == 0) || strcmp(candidateArtifact.repository, installed[k].repository)) &&
                    SemVerCmp(candidateArtifact.min_version, installed[k].version) == 0)
                    {
                        // Found the node to traverse next
                        Internal_TraverseInstalledNode(graph, dependency.connected[j], traversedNodeCount, traversedNodes, installedCount, installed);
                        break;
                    }
            }
        }
    }
    Internal_ArrayAddNode(traversedNodeCount, traversedNodes, node);
}

size_t Internal_DownloadCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    return write(*((int*) userdata), ptr, size * nmemb);
}

enum KPMResult Internal_DownloadGraphItems(struct KPM* kpm, struct DependencyGraph* graph, size_t deduplicatedPackageCount, NodeIndex_t* deduplicatedPackages)
{
    for (size_t i=0; i < deduplicatedPackageCount; i++)
    {
        struct IndexedArtifact artifact;
        KPM_GetArtifact(kpm, graph->nodes[deduplicatedPackages[i]].repository, graph->nodes[deduplicatedPackages[i]].id, graph->nodes[deduplicatedPackages[i]].min_version, &artifact);

        char* filename = artifact.url + strlen(artifact.url)-1;
        while (*filename != '/' && filename >= artifact.url)
        {
            filename--;
        }
        filename++;

        char* path = malloc(strlen(kpm->pkgPath) + strlen("/tmp/") + strlen(filename));
        sprintf(path, "%s/tmp/%s", kpm->pkgPath, filename);
        int fd = open(path, O_CREAT|O_SYNC|O_TRUNC|O_WRONLY);
        free(path);
        if (fd == -1)
        {
            KPM_FreeIndexedArtifact(&artifact);
            return KPM_FILE_SYSTEM_ERROR; // @TODO
        }

        CURL* curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, artifact.url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal_DownloadCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fd); // This _should_ work
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); //@TODO: Wth?
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "kpm/1.0.0");
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
        curl_easy_setopt(curl, CURLOPT_FTP_SKIP_PASV_IP, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_perform(curl);
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        close(fd);
        curl_easy_cleanup(curl);

        KPM_FreeIndexedArtifact(&artifact);

        if (response_code != 200)
        {
            return KPM_CURL_ERROR;
        }
    }

    return KPM_OK;
}

bool Internal_InstallItem(struct KPM* kpm, char* path, struct KPMLogging* kpmLogging)
{
    char* manifest;
    Internal_GetManifest(path, &manifest, kpmLogging);
    cJSON* json = cJSON_Parse(manifest);

    char* id = cJSON_GetStringValue(cJSON_GetObjectItem(json, "id"));

    char* outPath = malloc(strlen(path) + 1 + strlen(id) + 2);
    sprintf(outPath, "%s/%s/", kpm->pkgPath, id);

    // First unpack the .kpkg file
    Internal_ExtractArchive(path, outPath, kpmLogging);

    char* installScriptPath = malloc(strlen(outPath) + strlen("install.sh") + 1);
    sprintf(outPath, "%sinstall.sh", outPath);

    // Check if an install.sh file exists
    // If so, run it
    if (access(installScriptPath, R_OK) == 0)
    {
        // Run install script
        if (system(installScriptPath) != 0)
        {
            // The install hook failed
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not execute install hook for [%s]", id);
            free(manifest);
            cJSON_Delete(json);
            free(outPath);
            free(installScriptPath);
            return false;
        }
    }

    free(manifest);
    cJSON_Delete(json);
    free(outPath);
    free(installScriptPath);
    return true;
}

enum KPMResult KPM_InstallPackage(struct KPM* kpm, struct InstallTarget* target, struct KPMLogging* kpmLogging)
{
    if (kpmLogging == NULL)
    {
        kpmLogging = &dummyKPMStub;
    }

    struct IndexedArtifact artifact;
    if (strncmp(target->id, "file://", strlen("file://")) == 0)
    {
        // We must index this artifact under the localhost repository
        // We do this because it simplifies so much logic
        char* outBuffer = NULL;
        int status;
        if ((status = Internal_GetManifest(target->id + strlen("file://"), &outBuffer, kpmLogging)) != KPM_OK)
        {
            free(outBuffer);
            return status;
        }

        //cJSON* json = cJSON_Parse(outBuffer);
        return KPM_GENERIC_ERROR; // @TODO: Come back to this later - index local packages
    }
    else
    {
        if (target->version != NULL)
        {
            if (KPM_GetArtifact(kpm, target->repository, target->id, *target->version, &artifact) != KPM_OK)
            {
                kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not find artifact for given target.");
                return KPM_GENERIC_ERROR;
            }
        }
        else
        {
            size_t artifactCount;
            struct IndexedArtifact* artifacts;
            if (KPM_ListPackageArtifacts(kpm, target->repository, target->id, &artifactCount, &artifacts) != KPM_OK || artifactCount == 0)
            {
                kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not find artifact for given target.");
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

    size_t traversedNodeCount;
    NodeIndex_t* traversedNodes;
    for (size_t i=0; i < installedPackageCount; i++)
    {
        struct DependencyNode depNode = {
            .type = NODE_DEPENDENCY,
            .connected = NULL,
            .connectedCount = 0,
            .id = strdup(installedPackages[i].id),
            .repository = strdup(installedPackages[i].repository),
            .min_version = installedPackages[i].version,
            .max_version = installedPackages[i].version
        };
        depNode.max_version.patch += 1;

        int depId = AddNode(&graph, depNode);
        AddEdge(&graph, rootId, depId);

        struct IndexedArtifact fakeArtifact = {
            .id = strdup(installedPackages[i].id),
            .repository = strdup(installedPackages[i].repository),
            .url = strdup(installedPackages[i].id),
            .version = installedPackages[i].version
        };

        NodeIndex_t constructedId = Internal_ConstructGraphFromArtifact(kpm, &graph, &fakeArtifact);
        KPM_FreeIndexedArtifact(&fakeArtifact);
        AddEdge(&graph, depId, constructedId);
        Internal_ArrayAddNode(&traversedNodeCount, &traversedNodes, constructedId);

        // Add nodes to traversal
        Internal_TraverseInstalledNode(&graph, constructedId, &traversedNodeCount, &traversedNodes, installedPackageCount, installedPackages);
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

    NodeIndex_t constructedId = Internal_ConstructGraphFromArtifact(kpm, &graph, &artifact);
    AddEdge(&graph, depId, constructedId);

    char* out;
    RenderGraph(&graph, &out);
    kpmLogging->log(KPM_VERBOSITY_DEBUG, "%s", out);

    /**
     * @brief Write out a state file for debugging purposes
     * 
     */
    FILE* file = fopen("/tmp/kpm_state.md", "w");
    if (target->version == NULL)
    {
        struct SemVer version = {
            .major = 0,
            .minor = 0,
            .patch = 0
        };
        target->version = &version;
    }
    int allocate = snprintf(NULL, 0, "Current State:\nInstalling Target: (%s/)%s (%u.%u.%u)\nArtifact Found: (%s/)%s (%u.%u.%u) [%s]\n\nGenerated Graph:\n\n```", target->repository, target->id, target->version->major, target->version->minor, target->version->patch, artifact.repository, artifact.id, artifact.version.major, artifact.version.minor, artifact.version.patch, artifact.url);
    char* string = malloc(allocate + 1);
    snprintf(string, allocate, "Current State:\nInstalling Target: (%s/)%s (%u.%u.%u)\nArtifact Found: (%s/)%s (%u.%u.%u) [%s]\n\nGenerated Graph:\n\n```", target->repository, target->id, target->version->major, target->version->minor, target->version->patch, artifact.repository, artifact.id, artifact.version.major, artifact.version.minor, artifact.version.patch, artifact.url);
    string[allocate] = '\0';
    fwrite(string, strlen(string), 1, file);

    char* rendered;
    RenderGraph(&graph, &rendered);
    fwrite(rendered, strlen(rendered), 1, file);
    fwrite("```", strlen("```"), 1, file);
    fclose(file);
    free(rendered);
    free(string);

    // @TODO: Validate that it's starting from the right point when handling already-installed artifacts
    if (!Internal_ResolveDependencyGraph(&graph, rootId, &traversedNodeCount, &traversedNodes, kpmLogging))
    {
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not resolve dependency graph.");
        kpmLogging->log(KPM_VERBOSITY_ERROR, "If you believe this is a mistake - please submit an issue");
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Include the state file at /tmp/kpm_state.md");

        return KPM_GENERIC_ERROR;
    }

    NodeIndex_t* deduplicatedPackages = NULL;
    size_t* packageDepth = NULL;
    size_t deduplicatedPackageCount = 0;

    size_t currentDepth=0;
    for (size_t i=0; i < traversedNodeCount; i++)
    {
        if (graph.nodes[traversedNodes[i]].type == NODE_DEPENDENCY)
        {
            continue; // Skip dependencies
        }
 
        if (traversedNodes[i] == rootId)
        {
            currentDepth = 0;
            continue;
        }
        currentDepth++;

        bool found = false;
        for (size_t j=0; j < deduplicatedPackageCount; j++)
        {
            if (deduplicatedPackages[j] == traversedNodes[i])
            {
                found = true;
                if (currentDepth > packageDepth[j])
                {
                    packageDepth[j] = currentDepth;
                    
                    // Ensure list is sorted by depth
                    size_t currentIndex = j-1;
                    NodeIndex_t currentPackage = deduplicatedPackages[j];
                    while (currentIndex > 0)
                    {
                        if (packageDepth[currentIndex-1] > currentDepth)
                        {
                            break;   
                        }

                        packageDepth[currentIndex] = packageDepth[currentIndex - 1];
                        deduplicatedPackages[currentIndex] = deduplicatedPackages[currentIndex - 1];
                        currentIndex--;
                    }

                    deduplicatedPackages[currentIndex] = currentPackage;
                    packageDepth[currentIndex] = currentDepth;
                }
                break;
            }
        }

        if (found)
        {
            continue;
        }

        deduplicatedPackageCount++;
        deduplicatedPackages = realloc(deduplicatedPackages, deduplicatedPackageCount * sizeof(NodeIndex_t));
        packageDepth = realloc(packageDepth, deduplicatedPackageCount * sizeof(size_t));
        
        // Ensure list is sorted by depth
        size_t currentIndex = deduplicatedPackageCount-1;
        while (currentIndex > 0)
        {
            if (packageDepth[currentIndex-1] > currentDepth)
            {
                break;
            }

            packageDepth[currentIndex] = packageDepth[currentIndex - 1];
            deduplicatedPackages[currentIndex] = deduplicatedPackages[currentIndex - 1];
            currentIndex--;
        }

        deduplicatedPackages[currentIndex] = traversedNodes[i];
        packageDepth[currentIndex] = currentDepth;
    }

    // @TODO: Ensure downgrades are checked
    size_t updateCount;
    NodeIndex_t* update;
    size_t installCount;
    NodeIndex_t* install;
    for (size_t i=0; i < deduplicatedPackageCount; i++) // 1 to skip the dummy root
    {
        bool installed = false; // Already installed
        bool installing = true; // New package
        for (size_t j=0; j < installedPackageCount; j++)
        {
            if (strcmp(graph.nodes[deduplicatedPackages[i]].id, installedPackages[j].id))
            {
                installed = true;
                if (SemVerCmp(graph.nodes[deduplicatedPackages[i]].min_version, installedPackages[j].version) <= 0)
                {
                    installing = false;
                }
                break;
            }
        }

        if (!installing)
        {
            continue;
        }

        if (installed)
        {
            Internal_ArrayAddNode(&updateCount, &update, deduplicatedPackages[i]);
        }
        else
        {
            Internal_ArrayAddNode(&installCount, &install, deduplicatedPackages[i]);
        }
    }

    kpmLogging->log(KPM_VERBOSITY_INFO, "Preparing to upgrade %i packages", updateCount);
    for (size_t i=0; i < updateCount; i++)
    {
        kpmLogging->log(KPM_VERBOSITY_INFO, "(d%i) - %s (%u.%u.%u)", packageDepth[i], graph.nodes[update[i]].id, graph.nodes[update[i]].min_version.major, graph.nodes[update[i]].min_version.minor, graph.nodes[update[i]].min_version.patch);
    }


    kpmLogging->log(KPM_VERBOSITY_INFO, "Preparing to install %i packages", installCount);
    for (size_t i=0; i < installCount; i++)
    {
        kpmLogging->log(KPM_VERBOSITY_INFO, "(d%i) - %s (%u.%u.%u)", packageDepth[i], graph.nodes[install[i]].id, graph.nodes[install[i]].min_version.major, graph.nodes[install[i]].min_version.minor, graph.nodes[install[i]].min_version.patch);
    }

    if (kpm->confirmInstall)
    {
        if (!kpmLogging->getInput("Would you like to proceed?"))
        {
            kpmLogging->log(KPM_VERBOSITY_INFO, "Aborted.");
            return KPM_OK; // @TODO
        }
    }
    
    Internal_DownloadGraphItems(kpm, &graph, deduplicatedPackageCount, deduplicatedPackages);

    for (size_t i=0; i < deduplicatedPackageCount; i++) // 1 to skip the dummy root
    {
        Internal_InstallItem(kpm, graph.nodes[deduplicatedPackages[i]]);
    }

    return KPM_OK;
}
