#include "curl_helpers.h"
#include "kpm/kpkg.h"
#include "openssl/evp.h"
#include "openssl/types.h"

#include "archive.h"
#include "archive_entry.h"
#include "cjson/cJSON.h"
#include "kpm/kpm.h"
#include <assert.h>
#include <errno.h>
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
#include "dependencies.h"
#include "internal_utils.h"
#include "uninstall.h"


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
KPM_Result Internal_DownloadGraphItems(KPM *kpm, DependencyGraph *graph, size_t deduplicatedPackageCount, NodeIdx_t *deduplicatedPackages, KPMIO *kpm_io)
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

KPM_Result KPM_InstallPackages(KPM *kpm, size_t target_count, InstallTarget *targets, KPMIO *kpm_io)
{
    if (kpm_io == NULL)
        kpm_io = &dummyKPMStub;

    return 0;
}
