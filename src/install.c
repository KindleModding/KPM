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
 * @param kpmIO 
 * @return enum KPMResult 
 */
enum KPMResult Internal_ExtractArchive(char* path, char* out, struct KPMIO* kpmIO)
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
        kpmIO->log(KPM_VERBOSITY_ERROR, "Cold not open file %s", path);
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

        char* entryPath = asprintf_hd("%s/%s", out, archive_entry_pathname(entry));
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
 * @param kpmIO 
 * @return enum KPMResult 
 */
enum KPMResult Internal_GetManifest(char* path, char** outBuffer, struct KPMIO* kpmIO)
{
    if (kpmIO == NULL)
    {
        kpmIO = &dummyKPMStub;
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
        kpmIO->log(KPM_VERBOSITY_ERROR, "Cold not open file %s", path);
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
            kpmIO->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
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
            kpmIO->log(KPM_VERBOSITY_ERROR, "Libarchive error: %s", archive_error_string(a));
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
enum KPMResult Internal_DownloadGraphItems(struct KPM* kpm, struct DependencyGraph* graph, size_t deduplicatedPackageCount, NodeIndex_t* deduplicatedPackages, struct KPMIO* kpmIO)
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

        kpmIO->log(KPM_VERBOSITY_INFO, "Downloading %s", artifact.id);
        char* path = malloc(strlen(kpm->pkgPath) + strlen("/tmp/") + strlen(filename) + 1);
        sprintf(path, "%s/tmp/", kpm->pkgPath);
        mkdir_r(path, 0775);
        sprintf(path, "%s/tmp/%s", kpm->pkgPath, filename);
        kpmIO->log(KPM_VERBOSITY_DEBUG, "Downloading to %s", path);
        int fd = open(path, O_CREAT|O_SYNC|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
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
        kpmIO->log(KPM_VERBOSITY_DEBUG, "Downloading url %s", target_url);
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
                kpmIO->log(KPM_VERBOSITY_DEBUG, "artifact is a file:// URL, ignoring error checking"); // @TODO
            }
            else
            {
                free(target_url);
                kpmIO->log(KPM_VERBOSITY_ERROR, "Curl received an invalid response code: %zu", response_code);
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
 * @param kpmIO 
 * @return true 
 * @return false 
 */
bool Internal_InstallItem(struct KPM* kpm, char* repository, char* path, bool installed_as_dependency, struct KPMIO* kpmIO)
{
    char* manifest;
    kpmIO->log(KPM_VERBOSITY_DEBUG, "Reading manifest from %s", path);

    enum KPMResult result;
    if ((result = Internal_GetManifest(path, &manifest, kpmIO)) != KPM_OK)
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "Could not load manifest from %s (%i)", path, result);
        return false;
    }

    if (manifest == NULL)
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "Could not load manifest from %s", path);
        return false;
    }

    kpmIO->log(KPM_VERBOSITY_DEBUG, "%s", manifest);
    cJSON* json = cJSON_Parse(manifest);

    if (cJSON_IsNull(cJSON_GetObjectItem(json, "manifest_version")) || cJSON_GetObjectItem(json, "manifest_version") == NULL)
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "Could not get manifest version.");
        free(manifest);
        cJSON_Delete(json);
        return false;
    }

    if (cJSON_GetNumberValue(cJSON_GetObjectItem(json, "manifest_version")) < KPM_MANIFEST_VERSION)
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "Invalid manifest version, got %.0f, expected %i", cJSON_GetNumberValue(cJSON_GetObjectItem(json, "manifest_version")), KPM_MANIFEST_VERSION);
        free(manifest);
        cJSON_Delete(json);
        return false;
    }

    char* id = cJSON_GetStringValue(cJSON_GetObjectItem(json, "id"));
    kpmIO->log(KPM_VERBOSITY_DEBUG, "Installing item with id: %s", id);

    char* outPath = asprintf_hd("%s/%s/", kpm->pkgPath, id);

    // First unpack the .kpkg file
    kpmIO->log(KPM_VERBOSITY_INFO, "Extracting archive");
    Internal_ExtractArchive(path, outPath, kpmIO);

    char* installScriptPath = asprintf_hd( "%sinstall.sh", outPath);

    // Check if an install.sh file exists
    // If so, run it
    if (access(installScriptPath, R_OK) == 0)
    {
        kpmIO->log(KPM_VERBOSITY_DEBUG, "Running install script for [%s]", id);
        // Run install script
        int result = -1;
        char* installCommand = asprintf_hd("sh %s 2>&1", installScriptPath);
        chdir(outPath);
        free(installScriptPath);
        kpmIO->log(KPM_VERBOSITY_INFO, "Running install hooks for %s", id);
        FILE* stream = popen(installCommand, "r");
        free(installCommand);
        if (stream != NULL)
        {
            int c;
            while ((c = fgetc(stream)) != EOF)
            {
                kpmIO->stream((char) c);
            }
            result = pclose(stream);
        }
        else
        {
            kpmIO->log(KPM_VERBOSITY_ERROR, "Could not run script - POPEN FAILURE");
        }

        if (result != 0)
        {
            // The install hook failed
            kpmIO->log(KPM_VERBOSITY_ERROR, "Could not execute install hook for [%s]", id);
            rmdir_r(outPath);
            free(outPath);
            free(manifest);
            cJSON_Delete(json);
            return false;
        }
        free(outPath);
    }

    // Add installed item to the database
    const char* zSQL = "INSERT OR REPLACE INTO installed_packages (id, repository, name, author, description, version_major, version_minor, version_patch, installed_as_dependency) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
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

enum KPMResult KPM_InstallPackages(struct KPM* kpm, size_t targetCount, struct InstallTarget* targets, struct KPMIO* kpmIO)
{
    if (kpmIO == NULL)
        kpmIO = &dummyKPMStub;

    // Construct a graph
    kpmIO->log(KPM_VERBOSITY_DEBUG, "Constructing dependency graph...");
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
    kpmIO->log(KPM_VERBOSITY_DEBUG, "Adding installed packages to graph:");
    size_t installedPackageCount;
    struct InstalledPackage *installedPackages;
    KPM_ListInstalledPackages(kpm, &installedPackageCount, &installedPackages);

    size_t traversedNodeCount;
    NodeIndex_t* traversedNodes = NULL;
    for (size_t i=0; i < installedPackageCount; i++)
    {
        bool installing = false;
        for (size_t j=0; j < targetCount; j++)
        {
            if (strcmp(installedPackages[i].id, targets[j].id) == 0)
            {
                installing = true;
                break;
            }
        }

        if (installing)
            continue;

        struct DependencyNode depNode = {
            .type = NODE_DEPENDENCY,
            .connected = NULL,
            .connectedCount = 0,
            .id = strdup(installedPackages[i].id),
            .repository = strdup(installedPackages[i].repository),
            .min_version = installedPackages[i].version,
            .max_version = SEMVER_MAX
        };

        kpmIO->log(KPM_VERBOSITY_DEBUG, "\t- %s/%s (%u.%u.%u)", installedPackages[i].repository, installedPackages[i].id, installedPackages[i].version.major, installedPackages[i].version.minor, installedPackages[i].version.patch);

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

        // Add node to traversal
        Internal_TraverseInstalledNode(&graph, constructedId, &traversedNodeCount, &traversedNodes, installedPackageCount, installedPackages);

        // @TODO: Add valid upgradable package versions
    }
    
    for (int i=0; i < targetCount; i++)
    {
        struct InstallTarget target = targets[i];
        if (target.repository == NULL)
        {
            if (target.version == NULL)
                kpmIO->log(KPM_VERBOSITY_INFO, "Preparing to install %s", target.id);
            else
                kpmIO->log(KPM_VERBOSITY_INFO, "Preparing to install %s (%u.%u.%u)", target.id, target.version->major, target.version->minor, target.version->patch);
        }
        else
        {
            if (target.version == NULL)
                kpmIO->log(KPM_VERBOSITY_INFO, "Preparing to install %s/%s", target.repository, target.id);
            else
                kpmIO->log(KPM_VERBOSITY_INFO, "Preparing to install %s/%s (%u.%u.%u)", target.repository, target.id, target.version->major, target.version->minor, target.version->patch);
        }

        struct IndexedArtifact artifact;
        if (strncmp(target.id, "file://", strlen("file://")) == 0)
        {
            char* outBuffer = NULL;
            int status;
            if ((status = Internal_GetManifest(target.id + strlen("file://"), &outBuffer, kpmIO)) != KPM_OK)
            {
                FreeDependencyGraph(&graph);
                free(outBuffer);
                return status;
            }

            //cJSON* json = cJSON_Parse(outBuffer);
            FreeDependencyGraph(&graph);
            return KPM_GENERIC_ERROR; // @TODO: Come back to this later - install local package files
        }
        else
        {
            if (target.version != NULL)
            {
                if (KPM_GetArtifact(kpm, target.repository, target.id, *target.version, &artifact) != KPM_OK)
                {
                    kpmIO->log(KPM_VERBOSITY_ERROR, "Could not find artifact for given target.");
                    FreeDependencyGraph(&graph);
                    return KPM_GENERIC_ERROR;
                }
            }
            else
            {
                size_t artifactCount;
                struct IndexedArtifact* artifacts;
                if (KPM_ListPackageArtifacts(kpm, target.repository, target.id, &artifactCount, &artifacts) != KPM_OK || artifactCount == 0)
                {
                    kpmIO->log(KPM_VERBOSITY_ERROR, "Could not find artifact for given target.");
                    FreeDependencyGraph(&graph);
                    return KPM_GENERIC_ERROR;
                }

                artifact.id = strdup(artifacts[0].id);
                artifact.repository = strdup(artifacts[0].repository);
                artifact.url = strdup(artifacts[0].url);
                artifact.version = artifacts[0].version;
                KPM_FreeIndexedArtifactList(artifactCount, artifacts);
            }
        }

        kpmIO->log(KPM_VERBOSITY_DEBUG, "Adding target to graph");
        // Add the target to the graph
        struct DependencyNode depNode = {
            .type = NODE_DEPENDENCY,
            .connected = NULL,
            .connectedCount = 0,
            .id = strdup(target.id),
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
        KPM_FreeIndexedArtifact(&artifact);
    }

    // Render out the graph and log the data for debugging
    char* rendered;
    RenderGraph(&graph, &rendered);
    kpmIO->log(KPM_VERBOSITY_DEBUG, "Constructed dependency graph:\n\n%s\n\n", rendered);

    /**
     * Write out a state file for debugging purposes
     */
    FILE* file = fopen("/tmp/kpm_state.md", "w");
    if (file == NULL)
        kpmIO->log(KPM_VERBOSITY_WARN, "Could not open file '/tmp/kpm_state.md'");
    else
    {
        char* string = asprintf_hd("Generated Graph:\n\n```");
        fwrite(string, strlen(string), 1, file);
        free(string);

        fwrite(rendered, strlen(rendered), 1, file);
        free(rendered);

        fwrite("```", strlen("```"), 1, file);
        fclose(file);
    }

    // @TODO: Validate that it's starting from the right point when handling already-installed artifacts
    if (!Internal_ResolveDependencyGraph(&graph, rootId, &traversedNodeCount, &traversedNodes, kpmIO))
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "Could not resolve dependency graph.");
        kpmIO->log(KPM_VERBOSITY_ERROR, "If you believe this is a bug - please submit an issue");
        kpmIO->log(KPM_VERBOSITY_ERROR, "Include the state file at /tmp/kpm_state.md");

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

    size_t upgradeCount = 0;
    NodeIndex_t* upgrade = NULL;
    size_t downgradeCount = 0;
    NodeIndex_t* downgrade = NULL;
    size_t installCount = 0;
    NodeIndex_t* install = NULL;
    for (size_t i=0; i < deduplicatedPackageCount; i++)
    {
        bool installed = false; // Already installed
        for (size_t j=0; j < installedPackageCount; j++)
        {
            if (strcmp(graph.nodes[deduplicatedPackages[i]].id, installedPackages[j].id))
            {
                installed = true;
                if (SemVerCmp(graph.nodes[deduplicatedPackages[i]].min_version, installedPackages[j].version) < 0)
                    Internal_ArrayAddNode(&upgradeCount, &upgrade, deduplicatedPackages[i]);
                else if (SemVerCmp(graph.nodes[deduplicatedPackages[i]].min_version, installedPackages[j].version) > 0)
                    Internal_ArrayAddNode(&downgradeCount, &downgrade, deduplicatedPackages[i]);
                break;
            }
        }
        
        if (!installed)
            Internal_ArrayAddNode(&installCount, &install, deduplicatedPackages[i]);
    }

    // @TODO: Ensure downgrades are checked
    kpmIO->log(KPM_VERBOSITY_INFO, "Preparing to downgrade %zu packages", upgradeCount);
    for (size_t i=0; i < downgradeCount; i++)
    {
        kpmIO->log(KPM_VERBOSITY_INFO, "- %s (%u.%u.%u)", graph.nodes[downgrade[i]].id, graph.nodes[downgrade[i]].min_version.major, graph.nodes[downgrade[i]].min_version.minor, graph.nodes[downgrade[i]].min_version.patch);
    }

    kpmIO->log(KPM_VERBOSITY_INFO, "Preparing to upgrade %zu packages", upgradeCount);
    for (size_t i=0; i < upgradeCount; i++)
    {
        kpmIO->log(KPM_VERBOSITY_INFO, "- %s (%u.%u.%u)", graph.nodes[upgrade[i]].id, graph.nodes[upgrade[i]].min_version.major, graph.nodes[upgrade[i]].min_version.minor, graph.nodes[upgrade[i]].min_version.patch);
    }

    kpmIO->log(KPM_VERBOSITY_INFO, "Preparing to install %zu packages", installCount);
    for (size_t i=0; i < installCount; i++)
    {
        kpmIO->log(KPM_VERBOSITY_INFO, "- %s (%u.%u.%u)", graph.nodes[install[i]].id, graph.nodes[install[i]].min_version.major, graph.nodes[install[i]].min_version.minor, graph.nodes[install[i]].min_version.patch);
    }
    free(downgrade);
    free(upgrade);
    free(install);

    if (!kpmIO->getInput("Would you like to proceed?"))
    {
        kpmIO->log(KPM_VERBOSITY_INFO, "Aborted.");
        free(deduplicatedPackages);
        FreeDependencyGraph(&graph);
        return KPM_ABORTED; // @TODO
    }
    
    enum KPMResult result = Internal_DownloadGraphItems(kpm, &graph, deduplicatedPackageCount, deduplicatedPackages, kpmIO);
    if (result != KPM_OK)
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "Error: Could not download dependency graph (%i)", result);
        free(deduplicatedPackages);
        FreeDependencyGraph(&graph);
        return result;
    }

    bool retval = KPM_OK;
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

        kpmIO->log(KPM_VERBOSITY_INFO, "Installing %s (%u.%u.%u)", artifact.id, artifact.version.major, artifact.version.minor, artifact.version.patch);
        if (!Internal_InstallItem(kpm, graph.nodes[deduplicatedPackages[i]].repository, path, i != deduplicatedPackageCount-1, kpmIO)) // The last package installed is our target hence the value of installed_as_dependency
        {
            kpmIO->log(KPM_VERBOSITY_ERROR, "Could not install %s", artifact.id);
            KPM_FreeIndexedArtifact(&artifact);
            free(path);
            retval = KPM_GENERIC_ERROR;
            continue;
        }

        KPM_FreeIndexedArtifact(&artifact);
        free(path);

        // If this package is installed so are its dependencies
        for (int j=0; j < graph.nodes[deduplicatedPackages[i]].connectedCount; j++)
        {
            struct DependencyNode dependency = graph.nodes[graph.nodes[deduplicatedPackages[i]].connected[j]];
            assert(dependency.type == NODE_DEPENDENCY);

            const char* zSQL = "INSERT OR REPLACE INTO current_dependencies (dependent, dependency_id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
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
                free(deduplicatedPackages);
                sqlite3_finalize(statement);
                FreeDependencyGraph(&graph);
                system("lipc-set-prop com.lab126.scanner doFullScan 1");
                return KPM_SQLITE_ERROR; // Failure with adding it to the database - @TODO: This could be bad, we may need better error handling
            }
        }
    }

    free(deduplicatedPackages);
    FreeDependencyGraph(&graph);
    system("lipc-set-prop com.lab126.scanner doFullScan 1");
    return retval;
}
