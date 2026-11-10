#include "operation.h"
#include "kpm/kpkg.h"
#include "kpm/kpmio.h"
#include "kpm/semver.h"
#include "openssl/evp.h"
#include "openssl/types.h"

#include "kpm/kpm.h"
#include "array.h"
#include <__stddef_unreachable.h>
#include <assert.h>
#include <curl/curl.h>
#include <curl/multi.h>
#include <curl/typecheck-gcc.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "callback.h"
#include "dependencies.h"
#include "internal_utils.h"
#include "uninstall.h"

bool operation_eq(Operation a, Operation b)
{
    return &a == &b;
}

#define OPERATION_NULL ((Operation){.current_version=0,.target={},.type=0});
create_array_c(Operation, OPERATION_NULL, operation_eq);

struct DownloadData
{
    KPMIO *kpm_io;
    long int total_size;
    long int downloaded;
    int reported_progress;
    EVP_MD_CTX *evp_ctx;
    int fd;
};

size_t Internal_DownloadWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    struct DownloadData *downloadData = (struct DownloadData*) userdata;
    if (downloadData->total_size != 0)
    {
        int calculated_progress = 100 * (float) (downloadData->downloaded)/((float) downloadData->total_size);
        if ((downloadData->downloaded + size * nmemb) == downloadData->total_size)
            calculated_progress = 100;
        if (calculated_progress >= downloadData->reported_progress+10  || (downloadData->downloaded + size * nmemb) == downloadData->total_size)
        {
            downloadData->reported_progress = (calculated_progress/10)*10;
            downloadData->kpm_io->log(KPM_VERBOSITY_INFO, "%i%% (%iMB/%iMB)", calculated_progress, (downloadData->downloaded + size*nmemb)/1000000, downloadData->total_size/1000000);
        }
    }

    downloadData->downloaded += size * nmemb;
    EVP_DigestUpdate(downloadData->evp_ctx, ptr, size*nmemb); // Update digest
    return write(downloadData->fd, ptr, size * nmemb);
}

size_t Internal_DownloadHeaderCallback(char *ptr, size_t size, size_t nitems, void *userdata)
{
    char *key = malloc(size*nitems + 1);
    char *value = malloc(size*nitems + 1);
    key[0] = 0;
    value[0] = 0;
    for (size_t i=0; i < size*nitems; i++)
    {
        if (ptr[i] == ':')
        {
            memcpy(key, ptr, i);
            key[i] = 0;
            memcpy(value, ptr+i+2, size*nitems - (i+2) - 1); // -1 to strip the newline
            value[size*nitems - (i+2) - 1] = 0;
        }
    }
    ((struct DownloadData*) userdata)->kpm_io->log(KPM_VERBOSITY_DEBUG, "H: %s:%s", key, value);

    if (strcmp(key, "content-length") == 0)
    {
        ((struct DownloadData*) userdata)->total_size = atol(value);
    }
    
    free(key);
    free(value);
    return size*nitems;
}

/**
 * @brief Download specified packages from a dependency graph
 * 
 * @param kpm 
 * @param graph 
 * @param deduplicatedPackageCount 
 * @param deduplicatedPackages 
 * @return KPMResult 
 */
KPM_Result Internal_DownloadGraphItems(KPM *kpm, DependencyGraph *graph, size_t deduplicatedPackageCount, NodeIdx *deduplicatedPackages, KPMIO *kpm_io)
{
    

    return KPM_OK;
}

/**
 * @brief Internally used to install a given kpkg package - DOES NOT CHECK DEPENDENCIES + ASSUMES PACKAGE IS NOT ALREADY PRESENT
 * 
 * @param kpm 
 * @param path 
 * @param kpm_io 
 * @return true 
 * @return false 
 */
bool Internal_InstallItem(KPM *kpm, char *repository, char *path, bool installed_as_dependency, bool upgrading, KPMIO *kpm_io)
{
    char *id = "";

    kpm_io->log(KPM_VERBOSITY_DEBUG, "Installing item with id: '%s'", id);

    char *outPath = asprintf_hd("%s/%s/", kpm->pkg_path, id);
    rmdir_r(outPath); // Just in case it exists

    // First unpack the .kpkg file
    kpm_io->log(KPM_VERBOSITY_INFO, "Extracting archive");
    //Internal_ExtractArchive(path, outPath, kpm_io);

    char *installScriptPath = asprintf_hd( "%sinstall.sh", outPath);

    // Check if an install.sh file exists
    // If so, run it
    if (access(installScriptPath, R_OK) == 0)
    {
        free(installScriptPath);
        kpm_io->log(KPM_VERBOSITY_DEBUG, "Running install script for [%s]", id);
        // Run install script
        int result = -1;
        char *installCommand;
        if (upgrading)
            installCommand = asprintf_hd("KPM_VERSION_MAJOR=%i KPM_VERSION_MINOR=%i KPM_VERSION_PATCH=%i KPM_PLATFORM=\"%s\" sh install.sh upgrade 2>&1", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH, KPM_PLATFORM);
        else
            installCommand = asprintf_hd("KPM_VERSION_MAJOR=%i KPM_VERSION_MINOR=%i KPM_VERSION_PATCH=%i KPM_PLATFORM=\"%s\" sh install.sh 2>&1", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH, KPM_PLATFORM);

        char *old_path = getcwd(NULL, 0);
        chdir(outPath);
        kpm_io->log(KPM_VERBOSITY_INFO, "Running install hooks for %s", id);
        kpm_io->log(KPM_VERBOSITY_DEBUG, "Running: %s", installCommand);
        FILE *stream = popen(installCommand, "r");
        free(installCommand);
        if (stream != NULL)
        {
            int c;
            while ((c = fgetc(stream)) != EOF)
            {
                kpm_io->stream((char) c);
            }
            result = pclose(stream);
        }
        else
        {
            kpm_io->log(KPM_VERBOSITY_ERROR, "Could not run script - POPEN FAILURE");
        }

        chdir(old_path);
        free(old_path);
        if (result != 0)
        {
            // The install hook failed
            kpm_io->log(KPM_VERBOSITY_ERROR, "Could not execute install hook for [%s]", id);
            Internal_RunUninstallHook(outPath, id, false, kpm_io);
            rmdir_r(outPath);
            free(outPath);
            return false;
        }
        free(outPath);
    }

    // Add installed item to the database
    /*sqlite3_stmt *statement;
    const char *zSQL = "INSERT OR REPLACE INTO installed_packages (id, repository, name, author, description, version_major, version_minor, version_patch, installed_as_dependency) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    if (repository != NULL)
        sqlite3_bind_text(statement, 2, repository, -1, SQLITE_STATIC);
    else
        sqlite3_bind_text(statement, 2, "", -1, SQLITE_STATIC);
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
    cJSON_Delete(json);*/
    return true;
}

KPM_Result AddInstalledPackagesToGraph(KPM *kpm, DependencyGraph *graph, NodeIdx root_idx, KPMIO *kpm_io)
{
    kpm_io->log(KPM_VERBOSITY_DEBUG, "Adding installed packages to graph");
    KPM_Result res;
    size_t installed_package_count;
    KPM_InstalledPackage *installed_packages;
    if ((res = KPM_ListInstalledPackages(kpm, &installed_package_count,&installed_packages)) != KPM_OK)
    {
        kpm_io->log(KPM_VERBOSITY_ERROR, "Could not list packages: (%i) %s", res, KPM_ErrorToString(res));
        KPM_FreeInstalledPackageList(installed_package_count, installed_packages);
        return res;
    }

    for (int i=0; i < installed_package_count; i++)
    {
        KPM_InstalledPackage installed_package = installed_packages[i];
        if (installed_package.installed_as_dependency)
            continue;

        kpm_io->log(KPM_VERBOSITY_DEBUG, "- Adding %s to graph", installed_package.id);
        NodeIdx dep_idx = DepGraph_CreateDependencyNode(graph, (KPM_Dependency) {
            .id = strdup(installed_package.id),
            .min_version = SEMVER_MIN,
            .max_version = SEMVER_MAX
        });
        DepGraph_AddArtifactDep(graph, root_idx, dep_idx);

        NodeIdx idx = DepGraph_CreateInstalledNode(graph, installed_package);
        DepGraph_AddDepTarget(graph, dep_idx, idx);
        if (!DepGraph_FillGraphFromTarget(kpm, graph, idx, kpm_io))
        {
            kpm_io->log(KPM_VERBOSITY_ERROR, "Could not fill graph for package.");
            KPM_FreeInstalledPackageList(installed_package_count, installed_packages);
            return KPM_GENERIC_ERROR;
        }

        // Add potential upgrade/downgrade paths
        kpm_io->log(KPM_VERBOSITY_DEBUG, "Adding alternate versions");
        KPM_Artifact *artifacts;
        size_t artifact_count;
        if (KPM_ListPackageArtifacts(kpm, NULL, installed_package.id, &artifact_count, &artifacts) != KPM_OK)
        {
            kpm_io->log(KPM_VERBOSITY_ERROR, "Could not construct graph for package.");
            KPM_FreeArtifactList(artifact_count, artifacts);
            KPM_FreeInstalledPackageList(installed_package_count, installed_packages);
            return KPM_GENERIC_ERROR;
        }

        for (int j=0; j < artifact_count; j++)
        {
            KPM_Artifact artifact = artifacts[j];
            kpm_io->log(KPM_VERBOSITY_DEBUG, "- %s/%s (%i.%i.%i)", artifact.repository, artifact.id, artifact.version.major, artifact.version.minor, artifact.version.patch);
            NodeIdx idx = DepGraph_CreateTargetNode(
                graph,
                (KPM_InstallTarget) {
                    .type = INSTALL_ARTIFACT,
                    .artifact = artifact
                },
                GT_PRIORITY_NONE
            );
            DepGraph_AddDepTarget(graph, dep_idx, idx);
            if (!DepGraph_FillGraphFromTarget(kpm, graph, idx, kpm_io))
            {
                kpm_io->log(KPM_VERBOSITY_ERROR, "Could not fill graph for package artifact.");
                KPM_FreeArtifactList(artifact_count, artifacts);
                KPM_FreeInstalledPackageList(installed_package_count, installed_packages);
                return KPM_GENERIC_ERROR;
            }
        }

        KPM_FreeArtifactList(artifact_count, artifacts);
    }
    KPM_FreeInstalledPackageList(installed_package_count, installed_packages);
    return KPM_OK;
}

bool DownloadOperationFiles(KPM *kpm, OperationArray *operations, KPMIO *kpm_io)
{
    Operation operation;
    OperationArray_ForEach(operations, operation)
    {
        if (operation.type == OP_UNINSTALL)
            continue;

        // Out path if we need to download the entire file for some reason
        char *out_path = asprintf_hd("%s/%s.bin", kpm->tmp_path, DepGraph_GetTargetId(operation.target));

        // Construct a KPKG_FILE from the operation...
        KPKG_File *file;
        switch (operation.target.type)
        {
        case GT_INSTALL_TARGET:
            if (operation.target.target.type == INSTALL_FILE)
            {
                file = operation.target.target.file;
            }
            else
            {
                KPKG_Result res;
                if ((res = KPKG_LoadFile(&file, operation.target.target.artifact.url, out_path, kpm_io)) != KPKG_OK)
                {
                    kpm_io->log(KPM_VERBOSITY_ERROR, "KPKGErr: %i - Could not load KPKG file from %s", res, operation.target.target.artifact.url);
                    return false;
                }
            }
            break;
        case GT_DEPENDENCY:
        case GT_INSTALLED:
            unreachable();
        };

        // Download the contents of the KPKG_FILE
        if (file->kpkg_version == KPKG_VERSION_1 || file->kpkg_version == KPKG_VERSION_2)
        {
            return 0; // @TODO
        }

        // KPKG3+
        if (operation.type == OP_INSTALL)
        {
            // Write the entire file to our tmp
            //KPKG_WriteFile(file, out_path);
            return KPM_OK;
        }
    }
}

KPM_Result KPM_InstallPackages(KPM *kpm, size_t target_count, KPM_InstallTarget *targets, KPMIO *kpm_io)
{
    if (kpm_io == NULL)
        kpm_io = &dummyKPMStub;

    kpm_io->log(KPM_VERBOSITY_DEBUG, "Creating dependency graph");
    DependencyGraph *graph = DepGraph_Create(target_count * 2);
    NodeIdx root_idx = DepGraph_AddNode(graph, (DepGraph_Node) {
        .connected = NodeIdxArray_Create(),
        .gt = {
            .type = GT_DEPENDENCY,
            .priority = GT_PRIORITY_EXPLICIT,
            .dependency = {
                .id = NULL,
                .min_version = { .major=0, .minor=0, .patch=0 },
                .max_version = { .major=0, .minor=0, .patch=0 }
            }
        }
    });

    KPM_Result res = AddInstalledPackagesToGraph(kpm, graph, root_idx, kpm_io);
    if (res != KPM_OK)
    {
        DepGraph_FreeGraph(graph);
        return res;
    }

    kpm_io->log(KPM_VERBOSITY_DEBUG, "Adding explicit targets to graph");
    for (int i=0; i < target_count; i++)
    {
        KPM_InstallTarget target = targets[i];
        kpm_io->log(KPM_VERBOSITY_DEBUG, "- Adding %s to graph", KPM_GetInstallTargetId(target));
        NodeIdx idx = DepGraph_CreateTargetNode(graph, target, GT_PRIORITY_EXPLICIT);
        DepGraph_AddDepTarget(graph, root_idx, idx);
        if (!DepGraph_FillGraphFromTarget(kpm, graph, idx, kpm_io))
        {
            kpm_io->log(KPM_VERBOSITY_ERROR, "Could not fill graph for package.");
            return KPM_GENERIC_ERROR;
        }
    }

    char *rendered;
    DepGraph_Render(graph, &rendered);
    if (!rendered)
    {
        kpm_io->log(KPM_VERBOSITY_ERROR, "Could not render graph.");
        DepGraph_FreeGraph(graph);
        return KPM_GENERIC_ERROR;
    }

    kpm_io->log(KPM_VERBOSITY_DEBUG, "Rendered dependency graph:\n%s", rendered);

    
    kpm_io->log(KPM_VERBOSITY_DEBUG, "Solving dependency graph...");
    Resolver_State *state = Resolver_Resolve(graph, root_idx, kpm_io);
    if (!state)
    {
        kpm_io->log(KPM_VERBOSITY_ERROR, "Could not resolve graph.");
        kpm_io->log(KPM_VERBOSITY_DEBUG, "Dumping state:");
        TraversedNode current;
        TraversedNodeArray_ForEach(state->traversed_nodes, current)
        {
            kpm_io->log(KPM_VERBOSITY_DEBUG, "%i, %i", current.node_idx, current.parent_traversed_idx);
        }
        DepGraph_FreeGraph(graph);
        return KPM_RESOLUTION_ERROR;
    }

    size_t installed_package_count;
    KPM_InstalledPackage *installed_packages;
    if ((res = KPM_ListInstalledPackages(kpm, &installed_package_count,&installed_packages)) != KPM_OK)
    {
        kpm_io->log(KPM_VERBOSITY_ERROR, "Could not list packages: (%i) %s", res, KPM_ErrorToString(res));
        Resolver_FreeState(state);
        KPM_FreeInstalledPackageList(installed_package_count, installed_packages);
        return res;
    }

    kpm_io->log(KPM_VERBOSITY_DEBUG, "Listing resolved packages");

    size_t installing = 0;
    size_t reinstalling = 0;
    size_t upgrading = 0;
    size_t downgrading = 0;

    OperationArray *operations = OperationArray_Create();
    for (int i = TraversedNodeArray_Size(state->traversed_nodes) - 1; i > 0; i--)
    {
        TraversedNode traversed_node = TraversedNodeArray_Get(state->traversed_nodes, i);
        DepGraph_Node node = state->graph->nodes[traversed_node.node_idx];
        if (node.gt.type == GT_DEPENDENCY)
            continue;
        if (node.gt.priority == GT_PRIORITY_INSTALLED)
            continue; // Skip already installed targets

        Operation operation = {
            .target = node.gt,
            .type = OP_INSTALL
        };
        for (int j=0; j < installed_package_count && operation.type == OP_INSTALL; j++)
        {
            if (!strcmp(DepGraph_GetTargetId(node.gt), installed_packages[j].id))
            {
                operation.current_version = installed_packages[j].version;
                // Note: We only reinstall if explicit
                if (node.gt.priority == GT_PRIORITY_EXPLICIT && SemVerCmp(DepGraph_GetTargetVersion(node.gt), installed_packages[j].version) == 0)
                {
                    reinstalling++;
                    operation.type = OP_REINSTALL;
                }
                else if (SemVerCmp(DepGraph_GetTargetVersion(node.gt), installed_packages[j].version) < 0)
                {
                    downgrading++;
                    operation.type = OP_DOWNGRADE;
                }
                else if (SemVerCmp(DepGraph_GetTargetVersion(node.gt), installed_packages[j].version) > 0)
                {
                    upgrading++;
                    operation.type = OP_UPGRADE;
                }
                break;
            }
        }

        if (operation.type == OP_INSTALL)
            installing++;

        kpm_io->log(KPM_VERBOSITY_DEBUG, "- %s %i.%i.%i T:%i", DepGraph_GetTargetId(node.gt), DepGraph_GetTargetVersion(node.gt).major, DepGraph_GetTargetVersion(node.gt).minor, DepGraph_GetTargetVersion(node.gt).patch, operation.type);
        
        // Add to corresponding list
        OperationArray_Add(operations, operation);
    }

    size_t uninstalling = 0;
    for (int i=0; i < installed_package_count; i++)
    {
        KPM_InstalledPackage installed_package = installed_packages[i];
        bool do_uninstall = true;
        TraversedNode current;
        TraversedNodeArray_ForEach(state->traversed_nodes, current)
        {
            if (!strcmp(installed_package.id, DepGraph_GetTargetId(state->graph->nodes[current.node_idx].gt)))
            {
                do_uninstall = false;
                break;
            }
        }

        if (!do_uninstall)
            continue;

        OperationArray_Add(operations, (Operation) {
            .target = (DepGraph_Target) {
                .priority = GT_PRIORITY_INSTALLED,
                .type = GT_INSTALLED,
                .installed = installed_package
            },
            .type = OP_UNINSTALL,
            .current_version = installed_package.version
        });
        uninstalling++;
    }
    
    Operation current_operation;
    if (installing > 0)
    {
        kpm_io->log(KPM_VERBOSITY_INFO, "Installing %i new packages.", installing);
        OperationArray_ForEach(operations, current_operation)
        {
            if (current_operation.type != OP_INSTALL)
                continue;
            kpm_io->log(KPM_VERBOSITY_DEBUG, "- %s (%i.%i.%i)", DepGraph_GetTargetId(current_operation.target), DepGraph_GetTargetVersion(current_operation.target).major, DepGraph_GetTargetVersion(current_operation.target).minor, DepGraph_GetTargetVersion(current_operation.target).patch);
        }
    }

    if (reinstalling > 0)
    {
        kpm_io->log(KPM_VERBOSITY_INFO, "Reinstalling %i packages.", reinstalling);
        OperationArray_ForEach(operations, current_operation)
        {
            if (current_operation.type != OP_REINSTALL)
                continue;
            kpm_io->log(KPM_VERBOSITY_DEBUG, "- %s (%i.%i.%i)", DepGraph_GetTargetId(current_operation.target), DepGraph_GetTargetVersion(current_operation.target).major, DepGraph_GetTargetVersion(current_operation.target).minor, DepGraph_GetTargetVersion(current_operation.target).patch);
        }
    }

    if (upgrading > 0)
    {
        kpm_io->log(KPM_VERBOSITY_INFO, "Upgrading %i packages.", upgrading);
        OperationArray_ForEach(operations, current_operation)
        {
            if (current_operation.type != OP_UPGRADE)
                continue;
            kpm_io->log(KPM_VERBOSITY_DEBUG, "- %s (%i.%i.%i -> %i.%i.%i)", current_operation.current_version.major, current_operation.current_version.minor, current_operation.current_version.patch, DepGraph_GetTargetId(current_operation.target), DepGraph_GetTargetVersion(current_operation.target).major, DepGraph_GetTargetVersion(current_operation.target).minor, DepGraph_GetTargetVersion(current_operation.target).patch);
        }
    }

    if (downgrading > 0)
    {
        kpm_io->log(KPM_VERBOSITY_INFO, "Downgrading %i packages.", downgrading);
        OperationArray_ForEach(operations, current_operation)
        {
            if (current_operation.type != OP_DOWNGRADE)
                continue;
            kpm_io->log(KPM_VERBOSITY_DEBUG, "- %s (%i.%i.%i -> %i.%i.%i)", current_operation.current_version.major, current_operation.current_version.minor, current_operation.current_version.patch, DepGraph_GetTargetId(current_operation.target), DepGraph_GetTargetVersion(current_operation.target).major, DepGraph_GetTargetVersion(current_operation.target).minor, DepGraph_GetTargetVersion(current_operation.target).patch);
        }
    }

    if (uninstalling > 0)
    {
        kpm_io->log(KPM_VERBOSITY_INFO, "Uninstalling %i packages.", uninstalling);
        OperationArray_ForEach(operations, current_operation)
        {
            if (current_operation.type != OP_UNINSTALL)
                continue;
            kpm_io->log(KPM_VERBOSITY_DEBUG, "- %s (%i.%i.%i)", DepGraph_GetTargetId(current_operation.target), DepGraph_GetTargetVersion(current_operation.target).major, DepGraph_GetTargetVersion(current_operation.target).minor, DepGraph_GetTargetVersion(current_operation.target).patch);
        }
    }

    if (!kpm_io->getInput("Proceed."))
    {
        OperationArray_Free(operations);
        Resolver_FreeState(state);
        return KPM_ABORTED;
    }

    kpm_io->log(KPM_VERBOSITY_DEBUG, "Downloading operation data");
    DownloadOperationFiles(kpm, operations, kpm_io);

    OperationArray_Free(operations);
    Resolver_FreeState(state);
    return KPM_OK;
}
