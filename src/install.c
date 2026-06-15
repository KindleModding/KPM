#include "archive.h"
#include "archive_entry.h"
#include "cjson/cJSON.h"
#include "kpm/kpm.h"
#include <assert.h>
#include <curl/curl.h>
#include <curl/multi.h>
#include <curl/typecheck-gcc.h>
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
#include "sqlite3.h"
#include "install.h"
#include "internal_utils.h"

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

/**
 * @brief Internal function to extract an archive to an output path
 * 
 * @param path 
 * @param out 
 * @param kpmLogging 
 * @return enum KPMResult 
 */
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

        char* entryPath = asprintf_hd("%s/%s", out, archive_entry_pathname(entry));
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

/**
 * @brief Get the manifest of a KPKG package
 * 
 * @param path 
 * @param outBuffer 
 * @param kpmLogging 
 * @return enum KPMResult 
 */
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

        *outBuffer = malloc(archive_entry_size(entry) + 1);
        memset(*outBuffer, 0, archive_entry_size(entry) + 1);
        const void* blockBuffer;
        size_t blockSize = 0;
        la_int64_t offset = 0;
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

/**
 * @brief Traverse installed dependencies
 * 
 * @param graph 
 * @param node 
 * @param traversedNodeCount 
 * @param traversedNodes 
 * @param installedCount 
 * @param installed 
 */
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

/**
 * @brief Download specified packages from a dependency graph
 * 
 * @param kpm 
 * @param graph 
 * @param deduplicatedPackageCount 
 * @param deduplicatedPackages 
 * @return enum KPMResult 
 */
enum KPMResult Internal_DownloadGraphItems(struct KPM* kpm, struct DependencyGraph* graph, size_t deduplicatedPackageCount, NodeIndex_t* deduplicatedPackages, struct KPMLogging* kpmLogging)
{
    for (size_t i=0; i < deduplicatedPackageCount; i++)
    {
        struct IndexedArtifact artifact;
        KPM_GetArtifact(kpm, graph->nodes[deduplicatedPackages[i]].repository, graph->nodes[deduplicatedPackages[i]].id, graph->nodes[deduplicatedPackages[i]].min_version, &artifact);
        struct Repository repository;
        KPM_GetRepository(kpm, artifact.repository, &repository);

        char* filename = artifact.url + strlen(artifact.url)-1;
        while (*filename != '/' && filename >= artifact.url)
        {
            filename--;
        }
        filename++;

        kpmLogging->log(KPM_VERBOSITY_INFO, "Downloading %s", artifact.id);
        char* path = malloc(strlen(kpm->pkgPath) + strlen("/tmp/") + strlen(filename) + 1);
        sprintf(path, "%s/tmp/", kpm->pkgPath);
        mkdir_r(path, 0775);
        sprintf(path, "%s/tmp/%s", kpm->pkgPath, filename);
        kpmLogging->log(KPM_VERBOSITY_DEBUG, "Downloading to %s", path);
        int fd = open(path, O_CREAT|O_SYNC|O_TRUNC|O_WRONLY);
        free(path);
        if (fd == -1)
        {
            KPM_FreeIndexedArtifact(&artifact);
            return KPM_FILE_SYSTEM_ERROR; // @TODO
        }

        CURL* curl = curl_easy_init();
        char* repo_url = strdup(repository.url);
        int last_slash = 0;
        for (int i = 0; i < strlen(repo_url); i++)
        {
            if (repo_url[i] == '/')
                last_slash = i;
        }
        repo_url[last_slash] = 0;
        char* target_url = asprintf_hd("%s/%s", repo_url, artifact.url);
        free(repo_url);
        for (int i = 0; i < strlen(artifact.url); i++)
        {
            if (artifact.url[i] == ':')
            {
                free(target_url);
                target_url = strdup(artifact.url);
                break;
            }
        }
        kpmLogging->log(KPM_VERBOSITY_DEBUG, "Downloading url %s", target_url);
        curl_easy_setopt(curl, CURLOPT_URL, target_url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Internal_DownloadCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fd); // This _should_ work
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); //@FIXME @TODO: Wth?
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
        KPM_FreeRepository(&repository);

        if (response_code != 200)
        {
            if (strncmp("file://", target_url, 7) == 0)
            {
                free(target_url);
                kpmLogging->log(KPM_VERBOSITY_DEBUG, "artifact is a file:// URL, ignoring error checking"); // @TODO
            }
            else
            {
                free(target_url);
                kpmLogging->log(KPM_VERBOSITY_ERROR, "Curl received an invalid response code: %i", response_code);
                return KPM_CURL_ERROR;
            }
        }
    }

    return KPM_OK;
}

/**
 * @brief Internally used to install a given kpkg package - DOES NOT CHECK DEPENDENCIES + ASSUMES PACKAGE IS NOT ALREADY PRESENT
 * 
 * @param kpm 
 * @param path 
 * @param kpmLogging 
 * @return true 
 * @return false 
 */
bool Internal_InstallItem(struct KPM* kpm, char* repository, char* path, bool installed_as_dependency, struct KPMLogging* kpmLogging)
{
    char* manifest;
    kpmLogging->log(KPM_VERBOSITY_DEBUG, "Reading manifest from %s", path);

    enum KPMResult result;
    if ((result = Internal_GetManifest(path, &manifest, kpmLogging)) != KPM_OK)
    {
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not load manifest from %s (%i)", path, result);
        return false;
    }

    if (manifest == NULL)
    {
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not load manifest from %s", path);
        return false;
    }

    kpmLogging->log(KPM_VERBOSITY_DEBUG, "%s", manifest);
    cJSON* json = cJSON_Parse(manifest);

    char* id = cJSON_GetStringValue(cJSON_GetObjectItem(json, "id"));
    kpmLogging->log(KPM_VERBOSITY_DEBUG, "Installing item with id: %s", id);

    char* outPath = asprintf_hd("%s/%s/", kpm->pkgPath, id);

    // First unpack the .kpkg file
    Internal_ExtractArchive(path, outPath, kpmLogging);

    char* installScriptPath = asprintf_hd( "%sinstall.sh", outPath);

    // Check if an install.sh file exists
    // If so, run it
    if (access(installScriptPath, R_OK) == 0)
    {
        kpmLogging->log(KPM_VERBOSITY_DEBUG, "Running install script for [%s]", id);
        // Run install script
        int result = -1;
        char* installCommand = asprintf_hd("sh %s", installScriptPath);
        chdir(outPath);
        free(outPath);
        free(installScriptPath);
        FILE* stream = popen(installCommand, "r");
        free(installCommand);
        if (stream != NULL)
        {
            int c;
            while ((c = fgetc(stream)) != EOF)
            {
                kpmLogging->stream(fgetc(stream));
            }
            result = pclose(stream);
        }
        else
        {
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not run script - POPEN FAILURE");
        }

        if (result != 0)
        {
            // The install hook failed
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not execute install hook for [%s]", id);
            free(manifest);
            cJSON_Delete(json);
            return false;
        }
    }

    // Add installed item to the database
    const char* zSQL = "INSERT INTO installed_packages (id, repository, name, author, description, version_major, version_minor, version_patch, installed_as_dependency) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, repository, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, cJSON_GetStringValue(cJSON_GetObjectItem(json, "name")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, cJSON_GetStringValue(cJSON_GetObjectItem(json, "author")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, cJSON_GetStringValue(cJSON_GetObjectItem(json, "description")), -1, SQLITE_STATIC);
    sqlite3_bind_int(statement, 6, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "version"), 0)));
    sqlite3_bind_int(statement, 7, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "version"), 1)));
    sqlite3_bind_int(statement, 8, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(json, "version"), 2)));
    sqlite3_bind_int(statement, 9, installed_as_dependency);
    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        free(manifest);
        cJSON_Delete(json);
        return false; // Failure with adding it to the database - @TODO: This could be bad, we may need better error handling
    }

    sqlite3_finalize(statement);
    free(manifest);
    cJSON_Delete(json);
    return true;
}

enum KPMResult KPM_InstallPackage(struct KPM* kpm, struct InstallTarget* target, struct KPMLogging* kpmLogging)
{
    if (kpmLogging == NULL)
        kpmLogging = &dummyKPMStub;
    
    if (target->version == NULL)
        kpmLogging->log(KPM_VERBOSITY_INFO, "Installing package %s/%s (ANY)", target->repository, target->id);
    else
        kpmLogging->log(KPM_VERBOSITY_INFO, "Installing package %s/%s (%u.%u.%u)", target->repository, target->id, target->version->major, target->version->minor, target->version->patch);

    struct IndexedArtifact artifact;
    if (strncmp(target->id, "file://", strlen("file://")) == 0)
    {
        char* outBuffer = NULL;
        int status;
        if ((status = Internal_GetManifest(target->id + strlen("file://"), &outBuffer, kpmLogging)) != KPM_OK)
        {
            free(outBuffer);
            return status;
        }

        //cJSON* json = cJSON_Parse(outBuffer);
        return KPM_GENERIC_ERROR; // @TODO: Come back to this later - install local package files
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
    kpmLogging->log(KPM_VERBOSITY_DEBUG, "Constructing dependency graph...");
    struct DependencyGraph graph;
    CreateDependencyGraph(&graph, 0);

    // Fake rootnode is important so that we can handle previously installed packages
    // (See the rendered graph)
    struct DependencyNode rootNode = {
        .type = NODE_ARTIFACT,
        .connected = NULL,
        .connectedCount = 0,
        .id = strdup(""),
        .repository = strdup(""),
    };

    NodeIndex_t rootId = AddNode(&graph, rootNode);

    // Add installed packages to the graph
    kpmLogging->log(KPM_VERBOSITY_DEBUG, "Adding installed packages to graph:");
    size_t installedPackageCount;
    struct InstalledPackage *installedPackages;
    KPM_ListInstalledPackages(kpm, &installedPackageCount, &installedPackages);

    size_t traversedNodeCount;
    NodeIndex_t* traversedNodes = NULL;
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
        depNode.max_version.patch += 1; // @TODO: Allow listing alternative package to upgrade

        kpmLogging->log(KPM_VERBOSITY_DEBUG, "\t- %s/%s (%u.%u.%u)", installedPackages[i].repository, installedPackages[i].id, installedPackages[i].version.major, installedPackages[i].version.minor, installedPackages[i].version.patch);

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

    kpmLogging->log(KPM_VERBOSITY_DEBUG, "Adding target to graph");
    // Add the target to the graph
    struct DependencyNode depNode = {
        .type = NODE_DEPENDENCY,
        .connected = NULL,
        .connectedCount = 0,
        .id = strdup(target->id),
        .repository = NULL,
        .min_version = {
            .major = 0,
            .minor = 0,
            .patch = 0
        },
        .max_version = {
            .major = 0,
            .minor = 0,
            .patch = 0
        }
    };
    int depId = AddNode(&graph, depNode);
    AddEdge(&graph, rootId, depId);

    NodeIndex_t constructedId = Internal_ConstructGraphFromArtifact(kpm, &graph, &artifact);
    AddEdge(&graph, depId, constructedId);

    // Render out the graph and log the data for debugging
    char* rendered;
    RenderGraph(&graph, &rendered);
    kpmLogging->log(KPM_VERBOSITY_DEBUG, "Constructed dependency graph:\n\n%s\n\n", rendered);

    /**
     * Write out a state file for debugging purposes
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
    char* string = asprintf_hd("Current State:\nInstalling Target: (%s/)%s (%u.%u.%u)\nArtifact Found: (%s/)%s (%u.%u.%u) [%s]\n\nGenerated Graph:\n\n```", target->repository, target->id, target->version->major, target->version->minor, target->version->patch, artifact.repository, artifact.id, artifact.version.major, artifact.version.minor, artifact.version.patch, artifact.url);
    KPM_FreeIndexedArtifact(&artifact);
    fwrite(string, strlen(string), 1, file);
    free(string);

    fwrite(rendered, strlen(rendered), 1, file);
    free(rendered);

    fwrite("```", strlen("```"), 1, file);
    fclose(file);

    // @TODO: Validate that it's starting from the right point when handling already-installed artifacts
    if (!Internal_ResolveDependencyGraph(&graph, rootId, &traversedNodeCount, &traversedNodes, kpmLogging))
    {
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not resolve dependency graph.");
        kpmLogging->log(KPM_VERBOSITY_ERROR, "If you believe this is a bug - please submit an issue");
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Include the state file at /tmp/kpm_state.md");

        FreeDependencyGraph(&graph);
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
    free(traversedNodes);
    free(packageDepth);

    // @TODO: Ensure downgrades are checked
    size_t updateCount = 0;
    NodeIndex_t* update = NULL;
    size_t installCount = 0;
    NodeIndex_t* install = NULL;
    for (size_t i=1; i < deduplicatedPackageCount; i++) // 1 to skip the dummy root
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
        kpmLogging->log(KPM_VERBOSITY_INFO, "- %s (%u.%u.%u)", graph.nodes[update[i]].id, graph.nodes[update[i]].min_version.major, graph.nodes[update[i]].min_version.minor, graph.nodes[update[i]].min_version.patch);
    }


    kpmLogging->log(KPM_VERBOSITY_INFO, "Preparing to install %i packages", installCount);
    for (size_t i=0; i < installCount; i++)
    {
        kpmLogging->log(KPM_VERBOSITY_INFO, "- %s (%u.%u.%u)", graph.nodes[install[i]].id, graph.nodes[install[i]].min_version.major, graph.nodes[install[i]].min_version.minor, graph.nodes[install[i]].min_version.patch);
    }
    free(update);
    free(install);

    if (kpm->confirmInstall)
    {
        if (!kpmLogging->getInput("Would you like to proceed?"))
        {
            kpmLogging->log(KPM_VERBOSITY_INFO, "Aborted.");
            free(deduplicatedPackages);
            FreeDependencyGraph(&graph);
            return KPM_OK; // @TODO
        }
    }
    
    enum KPMResult result = Internal_DownloadGraphItems(kpm, &graph, deduplicatedPackageCount, deduplicatedPackages, kpmLogging);
    if (result != KPM_OK)
    {
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Error: Could not download dependency graph (%i)", result);
        free(deduplicatedPackages);
        FreeDependencyGraph(&graph);
        return result;
    }

    for (size_t i=0; i < deduplicatedPackageCount; i++) // 1 to skip the dummy root
    {
        struct IndexedArtifact artifact;
        KPM_GetArtifact(kpm, graph.nodes[deduplicatedPackages[i]].repository, graph.nodes[deduplicatedPackages[i]].id, graph.nodes[deduplicatedPackages[i]].min_version, &artifact);

        char* filename = artifact.url + strlen(artifact.url)-1;
        while (*filename != '/' && filename >= artifact.url)
        {
            filename--;
        }
        filename++;

        char* path = asprintf_hd("%s/tmp/%s", kpm->pkgPath, filename);

        if (!Internal_InstallItem(kpm, graph.nodes[deduplicatedPackages[i]].repository, path, i != deduplicatedPackageCount-1, kpmLogging)) // The last package installed is our target hence the value of installed_as_dependency
        {
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not install %s", artifact.id);
            KPM_FreeIndexedArtifact(&artifact);
            free(path);
            return KPM_GENERIC_ERROR; // @TODO: Implement atomic installation old package if upgrading
        }

        KPM_FreeIndexedArtifact(&artifact);
        free(path);

        // If this package is installed so are its dependencies
        for (int j=0; j < graph.nodes[deduplicatedPackages[i]].connectedCount; j++)
        {
            struct DependencyNode dependency = graph.nodes[graph.nodes[deduplicatedPackages[i]].connected[j]];
            assert(dependency.type == NODE_DEPENDENCY);

            const char* zSQL = "INSERT INTO current_dependencies (dependent, dependency_id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
            sqlite3_stmt* statement;
            sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
            sqlite3_bind_text(statement, 1, graph.nodes[deduplicatedPackages[i]].id, -1, SQLITE_STATIC);
            sqlite3_bind_text(statement, 2, dependency.id, -1, SQLITE_STATIC);
            sqlite3_bind_int(statement, 3, dependency.min_version.major);
            sqlite3_bind_int(statement, 4, dependency.min_version.minor);
            sqlite3_bind_int(statement, 5, dependency.min_version.patch);
            sqlite3_bind_int(statement, 6, dependency.max_version.major);
            sqlite3_bind_int(statement, 7, dependency.max_version.minor);
            sqlite3_bind_int(statement, 8, dependency.max_version.patch);
            if (sqlite3_step(statement) != SQLITE_DONE)
            {
                sqlite3_finalize(statement);
                return KPM_SQLITE_ERROR; // Failure with adding it to the database - @TODO: This could be bad, we may need better error handling
            }
        }
    }

    free(deduplicatedPackages);
    FreeDependencyGraph(&graph);

    return KPM_OK;
}
