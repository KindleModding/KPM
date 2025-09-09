#include "cjson/cJSON.h"
#include "kpm/kpm.h"
#include "simpleGET.h"
#include <string.h>
#include <stdbool.h>

void dummyCallback(enum Verbosity verbosity, uint progress, char* details, ...)
{
    return;
}

bool indexDependency(struct KPM* kpm, char* artifactURL, cJSON* dependency, KPMStatusCallback* statusCallback)
{
    const char* zSQL = "INSERT INTO artifact_dependencies (artifact, repository, id, type, version_major, version_minor, version_patch) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, artifactURL, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, cJSON_GetStringValue(cJSON_GetObjectItem(dependency, "id")), -1, SQLITE_STATIC);

    cJSON* repositoryObject = cJSON_GetObjectItem(dependency, "repository");
    if (repositoryObject != NULL && !cJSON_IsNull(repositoryObject))
    {
        sqlite3_bind_text(statement, 2, cJSON_GetStringValue(repositoryObject), -1, SQLITE_STATIC);
    }
    else {
        sqlite3_bind_null(statement, 2);
    }

    cJSON* typeObject = cJSON_GetObjectItem(dependency, "type");
    if (typeObject != NULL && !cJSON_IsNull(typeObject))
    {
        char* typeStr = cJSON_GetStringValue(typeObject);
        enum DependencyType type = KPM_DT_EQUAL_TO;
        if (strcmp(typeStr, "==") == 0) { // Sinful same-line bracket frfr
            type=KPM_DT_EQUAL_TO;
        } else if (strcmp(typeStr, ">") == 0) {
            type=KPM_DT_GREATER_THAN;
        } else if (strcmp(typeStr, "<") == 0) {
            type=KPM_DT_LESS_THAN;
        } else if (strcmp(typeStr, ">=") == 0) {
            type=KPM_DT_GREATER_THAN_OR_EQUAL_TO;
        } else if (strcmp(typeStr, "<=") == 0) {
            type=KPM_DT_LESS_THAN_OR_EQUAL_TO;
        } else {
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Invalid dependency found.");
            sqlite3_finalize(statement);
            return false;
        }

        sqlite3_bind_int(statement, 4, type);
        sqlite3_bind_int(statement, 5, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "version"), 0)));
        sqlite3_bind_int(statement, 6, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "version"), 1)));
        sqlite3_bind_int(statement, 7, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "version"), 2)));
    }
    else {
        typeObject = NULL; // In case it's "JSON NULL" but not proper NULL (trigger dependency type "failure" case)
    }
    
    if (typeObject == NULL) { // No dependency type
        sqlite3_bind_null(statement, 4);
        sqlite3_bind_null(statement, 5);
        sqlite3_bind_null(statement, 6);
        sqlite3_bind_null(statement, 7);
    }

    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool indexArtifact(struct KPM* kpm, char* repositoryId, char* packageId, cJSON* artifact, KPMStatusCallback* statusCallback)
{
    const char* zSQL = "INSERT INTO artifacts (url, repository, id, version_major, version_minor, version_patch) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "url")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, repositoryId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, packageId, -1, SQLITE_STATIC);
    sqlite3_bind_int64(statement, 4, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 0)));
    sqlite3_bind_int64(statement, 5, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 1)));
    sqlite3_bind_int64(statement, 6, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 2)));
    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);

    // Index this artifact's dependencies
    cJSON* dependency;
    cJSON_ArrayForEach(dependency, cJSON_GetObjectItem(artifact, "dependencies"))
    {
        if (!indexDependency(kpm, cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "url")), dependency, statusCallback))
        {
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not index artifact dependencies for [%s] (%s)", packageId, cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "url")));
            return false;
        }
    }

    return true;
}

bool indexPackage(struct KPM* kpm, char* repositoryId, cJSON* package, KPMStatusCallback* statusCallback)
{
    // Ensure the package is well-formed
    if (cJSON_GetObjectItem(package, "name") == NULL ||
    cJSON_GetObjectItem(package, "author") == NULL ||
    cJSON_GetObjectItem(package, "description") == NULL ||
    cJSON_GetObjectItem(package, "artifacts") == NULL ||
    cJSON_GetArraySize(cJSON_GetObjectItem(package, "artifacts")) == 0)
    {
        statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not parse package %s", package->string);
        return false;
    }

    // INSERT that package INTO the DATABASE (deltarune reference???)
    const char* zSQL = "INSERT INTO packages (repository, id, name, author, description) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, package->string, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, cJSON_GetStringValue(cJSON_GetObjectItem(package, "name")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, cJSON_GetStringValue(cJSON_GetObjectItem(package, "author")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, cJSON_GetStringValue(cJSON_GetObjectItem(package, "description")), -1, SQLITE_STATIC);
    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        statusCallback(KPM_VERBOSITY_ERROR, 0, "SQLite error encountered when indexing package %s", package->string);
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);



    // Now we index artifacts
    cJSON* artifact;
    cJSON_ArrayForEach(artifact, cJSON_GetObjectItem(package, "artifacts"))
    {
        if (!indexArtifact(kpm, repositoryId, package->string, artifact, statusCallback)) // Dependency failed
        {
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not index artifact for [%s]", package->string);
            return false; // Stop adding packages if one fails
        }
    }

    return true;
}

/**
 * @brief Update the local index of package by downloading repository manifests
 * 
 * @param kpm The KPM object
 * @param statusCallback A callback for progress information
 * @return enum KPMResult 
 */
enum KPMResult KPM_UpdateIndex(struct KPM *kpm, KPMStatusCallback* statusCallback)
{
    if (statusCallback == NULL)
    {
        statusCallback = &dummyCallback;
    }

    statusCallback(KPM_VERBOSITY_INFO, 0, "Getting repositories...");

    size_t repositoryCount;
    struct Repository* repositories;
    enum KPMResult result;
    if ((result = KPM_ListRepositories(kpm, &repositoryCount, &repositories)) != KPM_OK)
    {
        statusCallback(KPM_VERBOSITY_ERROR, 0, "Unable to list KPM repositories");
        return result;
    }

    struct SimpleGETRequest request;
    for (size_t i=0; i < repositoryCount; i++)
    {
        statusCallback(KPM_VERBOSITY_INFO, 0, "Downloading index [%s]", repositories[i].url);
        SimpleGET_Initialise(&request, repositories[i].url);
        SimpleGET_Perform(&request);

        // Start a transaction for every repository
        sqlite3_exec(kpm->db, "BEGIN", NULL, NULL, NULL); // "But Chudda what if?" IT WONT FAIL



        // Clear indexed packages
        statusCallback(KPM_VERBOSITY_DEBUG, 0, "Clearing index");
        const char* zSQL = "DELETE FROM packages WHERE repository=?;";
        sqlite3_stmt* statement;
        sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
        sqlite3_bind_text(statement, 1, repositories[i].id, strlen(repositories[i].id), SQLITE_STATIC);
        
        int status = -1;
        while (status != SQLITE_DONE && status != SQLITE_ERROR)
        {
            status = sqlite3_step(statement);
        }
        
        sqlite3_finalize(statement);
        if (status == SQLITE_ERROR)
        {
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not clear packages for %s", repositories[i].id);
            SimpleGET_Cleanup(&request);
            sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
            break; // Move onto next repo
        }


        cJSON* json = cJSON_Parse(request.buffer);
        if (json == NULL)
        {
            SimpleGET_Cleanup(&request);
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not parse manifest");
            sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
            continue; // Move onto next repo
        }


        // Index packages & artifacts
        // We COULD give progress status here... but nah - HD
        bool success = true;
        cJSON* package;
        cJSON_ArrayForEach(package, cJSON_GetObjectItem(json, "packages"))
        {
            if (!indexPackage(kpm, repositories[i].id, package, statusCallback))
            {
                success = false;
                break;
            }
        }

        if (!success) // Repository failed
        {
            sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not index repository [%s]", repositories[i].id);
        }

        sqlite3_exec(kpm->db, "COMMIT", NULL, NULL, NULL);
        cJSON_Delete(json);
        SimpleGET_Cleanup(&request);
    }

    return KPM_OK;
}
