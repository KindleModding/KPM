#include "cjson/cJSON.h"
#include "curl/curl.h"
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include "simpleGET.h"
#include <limits.h>
#include <string.h>
#include "callback.h"

bool indexDependency(struct KPM* kpm, char* artifact_repository, char* artifact_id, char* artifact_url, cJSON* dependency, struct KPMIO* kpmIO)
{
    const char* zSQL = "INSERT INTO artifact_dependencies (artifact_repository, artifact_id, artifact_url, id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, artifact_repository, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, artifact_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, artifact_url, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, cJSON_GetStringValue(cJSON_GetObjectItem(dependency, "id")), -1, SQLITE_STATIC);
    
    if (cJSON_GetObjectItem(dependency, "min") != NULL)
    {
        sqlite3_bind_int(statement, 5, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 0)));
        sqlite3_bind_int(statement, 6, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 1)));
        sqlite3_bind_int(statement, 7, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 2)));
    }
    else
    {
        sqlite3_bind_int(statement, 5, 0);
        sqlite3_bind_int(statement, 6, 0);
        sqlite3_bind_int(statement, 7, 0);
    }

    if (cJSON_GetObjectItem(dependency, "max") != NULL)
    {
        sqlite3_bind_int(statement, 8, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 0)));
        sqlite3_bind_int(statement, 9, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 1)));
        sqlite3_bind_int(statement, 10, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 2)));
    }
    else
    {
        sqlite3_bind_int(statement, 8, VERSION_MAX);
        sqlite3_bind_int(statement, 9, VERSION_MAX);
        sqlite3_bind_int(statement, 10, VERSION_MAX);
    }

    int status;
    if ((status = sqlite3_step(statement)) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        kpmIO->log(KPM_VERBOSITY_ERROR, "SQLite error: (%i)", status);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool indexArtifact(struct KPM* kpm, char* repositoryId, char* packageId, cJSON* artifact, struct KPMIO* kpmIO)
{
    kpmIO->log(KPM_VERBOSITY_DEBUG, "  indexing artifact %s (%.0f.%.0f.%.0f)", packageId, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 0)), cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 1)), cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 2)));
    const char* zSQL = "INSERT INTO artifacts (url, repository, id, version_major, version_minor, version_patch) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "url")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, repositoryId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, packageId, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 4, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 0)));
    sqlite3_bind_int64(statement, 5, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 1)));
    sqlite3_bind_int64(statement, 6, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 2)));

    int status;
    if ((status = sqlite3_step(statement)) != SQLITE_DONE)
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "SQLite3 error (%i)", status);
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);

    // Index this artifact's dependencies
    cJSON* dependency;
    cJSON_ArrayForEach(dependency, cJSON_GetObjectItem(artifact, "dependencies"))
    {
        if (!indexDependency(kpm, repositoryId, packageId, cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "url")), dependency, kpmIO))
        {
            kpmIO->log(KPM_VERBOSITY_ERROR, "Could not index artifact dependencies for [%s] (%s)", packageId, cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "url")));
            return false;
        }
    }

    return true;
}

bool indexPackage(struct KPM* kpm, char* repositoryId, cJSON* package, struct KPMIO* kpmIO)
{
    // Ensure the package is well-formed
    if (cJSON_GetObjectItem(package, "name") == NULL ||
    cJSON_GetObjectItem(package, "author") == NULL ||
    cJSON_GetObjectItem(package, "description") == NULL ||
    cJSON_GetObjectItem(package, "artifacts") == NULL ||
    cJSON_GetArraySize(cJSON_GetObjectItem(package, "artifacts")) == 0)
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "Could not parse package %s", package->string);
        return false;
    }

    kpmIO->log(KPM_VERBOSITY_DEBUG, "indexing package [%s]", package->string);

    // INSERT that package INTO the DATABASE (deltarune reference???)
    const char* zSQL = "INSERT INTO packages (repository, id, name, author, description) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, package->string, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, cJSON_GetStringValue(cJSON_GetObjectItem(package, "name")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, cJSON_GetStringValue(cJSON_GetObjectItem(package, "author")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, cJSON_GetStringValue(cJSON_GetObjectItem(package, "description")), -1, SQLITE_STATIC);
    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "SQLite error encountered when indexing package %s", package->string);
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);



    // Now we index artifacts
    cJSON* artifact;
    cJSON_ArrayForEach(artifact, cJSON_GetObjectItem(package, "artifacts"))
    {
        // Check if this artifact's supported_platforms
        if (cJSON_IsArray(cJSON_GetObjectItem(artifact, "supported_platforms")))
        {
            bool supported=false;
            cJSON* supported_platform;
            cJSON_ArrayForEach(supported_platform, cJSON_GetObjectItem(artifact, "supported_platforms"))
            {
                if (strcmp(supported_platform->valuestring, KPM_PLATFORM) == 0)
                {
                    supported = true;
                    break;
                }
            }

            if (!supported)
                continue;
        }

        if (!indexArtifact(kpm, repositoryId, package->string, artifact, kpmIO)) // Dependency failed
        {
            kpmIO->log(KPM_VERBOSITY_ERROR, "Could not index artifact for [%s]", package->string);
            return false; // Stop adding packages if one fails
        }
    }

    return true;
}

enum KPMResult KPM_UpdateIndex(struct KPM *kpm, struct KPMIO* kpmIO)
{
    if (kpmIO == NULL)
    {
        kpmIO = &dummyKPMStub;
    }

    kpmIO->log(KPM_VERBOSITY_INFO, "Getting repositories...");

    size_t repositoryCount;
    struct Repository* repositories;
    enum KPMResult result;
    if ((result = KPM_ListRepositories(kpm, &repositoryCount, &repositories)) != KPM_OK)
    {
        kpmIO->log(KPM_VERBOSITY_ERROR, "Unable to list KPM repositories");
        return result;
    }

    struct SimpleGETRequest request;
    for (size_t i=0; i < repositoryCount; i++)
    {
        kpmIO->log(KPM_VERBOSITY_INFO, "Downloading index [%s]", repositories[i].url);
        SimpleGET_Initialise(&request, repositories[i].url);
        CURLcode curl_code = SimpleGET_Perform(&request);
        if (curl_code != CURLE_OK)
        {
            kpmIO->log(KPM_VERBOSITY_ERROR, "Could not fetch url [%s] - CURL Error: %i", curl_code);
            SimpleGET_Cleanup(&request);
            continue;
        }

        kpmIO->log(KPM_VERBOSITY_DEBUG, "Got response code: %i", request.response_code);
        if (request.response_code >= 400 && strncmp(repositories[i].url, "file://", strlen("file://")) != 0)
        {
            kpmIO->log(KPM_VERBOSITY_ERROR, "Could not fetch url [%s]", repositories[i].url);
            SimpleGET_Cleanup(&request);
            continue;
        }

        kpmIO->log(KPM_VERBOSITY_DEBUG, "Got manifest: %s", request.buffer);
        cJSON* json = cJSON_Parse(request.buffer);
        if (json == NULL)
        {
            kpmIO->log(KPM_VERBOSITY_ERROR, "Could not parse manifest");
            SimpleGET_Cleanup(&request);
            continue; // Move onto next repo
        }

        if (cJSON_GetNumberValue(cJSON_GetObjectItem(json, "manifest_version")) > KPM_MANIFEST_VERSION)
        {
            kpmIO->log(KPM_VERBOSITY_ERROR, "Invalid manifest version, got %.0f, expected %i", cJSON_GetNumberValue(cJSON_GetObjectItem(json, "manifest_version")), KPM_MANIFEST_VERSION);
            SimpleGET_Cleanup(&request);
            continue;
        }

        // Start a transaction for every repository
        sqlite3_exec(kpm->db, "BEGIN", NULL, NULL, NULL); // "But Chudda what if?" IT WONT FAIL

        // Clear indexed packages
        kpmIO->log(KPM_VERBOSITY_DEBUG, "Clearing index");
        const char* zSQL = "DELETE FROM packages WHERE repository=?;";
        sqlite3_stmt* statement;
        sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, repositories[i].id, -1, SQLITE_STATIC);
        
        int status = SQLITE_ROW;
        status = sqlite3_step(statement);
        sqlite3_finalize(statement);
        if (status != SQLITE_DONE)
        {
            KPM_FreeRepositoryList(repositoryCount, repositories);
            kpmIO->log(KPM_VERBOSITY_ERROR, "Could not clear packages for %s (%i)", repositories[i].id, status);
            SimpleGET_Cleanup(&request);
            sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
            break; // Move onto next repo
        }

        // Index packages & artifacts
        // We COULD give progress status here... but nah - HD
        cJSON* package;
        cJSON_ArrayForEach(package, cJSON_GetObjectItem(json, "packages"))
        {
            if (!indexPackage(kpm, repositories[i].id, package, kpmIO))
            {
                sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
                kpmIO->log(KPM_VERBOSITY_ERROR, "Could not index repository [%s]", repositories[i].id);
                break;
            }
        }

        sqlite3_exec(kpm->db, "COMMIT", NULL, NULL, NULL);
        cJSON_Delete(json);
        SimpleGET_Cleanup(&request);
    }

    KPM_FreeRepositoryList(repositoryCount, repositories);

    return KPM_OK;
}
