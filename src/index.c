#include "cjson/cJSON.h"
#include "kpm/kpm.h"
#include "simpleGET.h"
#include <string.h>

void dummyCallback(enum Verbosity verbosity, uint progress, char* details, ...)
{
    return;
}

enum KPMResult KPM_UpdateIndex(struct KPM *kpm, void (*statusCallback)(enum Verbosity verbosity, uint progress, char* details, ...))
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
        if (sqlite3_step(statement) != SQLITE_DONE)
        {
            statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not clear packages for %s", repositories[i].id);
            sqlite3_finalize(statement);
            SimpleGET_Cleanup(&request);
            sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
            continue; // Move onto next repo
        }
        sqlite3_finalize(statement);



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
        int success = 0;
        const cJSON* package;
        cJSON_ArrayForEach(package, cJSON_GetObjectItem(json, "packages"))
        {
            // Ensure the package is well-formed
            if (cJSON_GetObjectItem(package, "name") == NULL ||
            cJSON_GetObjectItem(package, "author") == NULL ||
            cJSON_GetObjectItem(package, "description") == NULL ||
            cJSON_GetObjectItem(package, "artifacts") == NULL ||
            cJSON_GetArraySize(cJSON_GetObjectItem(package, "artifacts")) == 0)
            {
                statusCallback(KPM_VERBOSITY_ERROR, 0, "Could not parse package %s", json->string);
                success = 0; // Every package must be good
                break;
            }

            // INSERT that package INTO the DATABASE (deltarune reference???)
            const char* zSQL = "INSERT INTO packages (repository, id, name, author, description) VALUES (?, ?, ?, ?, ?);";
            sqlite3_stmt* statement;
            sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
            sqlite3_bind_text(statement, 1, repositories[i].id, strlen(repositories[i].id), SQLITE_STATIC);
            sqlite3_bind_text(statement, 2, package->string, strlen(package->string), SQLITE_STATIC);
            sqlite3_bind_text(statement, 3, cJSON_GetStringValue(cJSON_GetObjectItem(package, "name")), strlen(cJSON_GetStringValue(cJSON_GetObjectItem(package, "name"))), SQLITE_STATIC);
            sqlite3_bind_text(statement, 4, cJSON_GetStringValue(cJSON_GetObjectItem(package, "author")), strlen(cJSON_GetStringValue(cJSON_GetObjectItem(package, "author"))), SQLITE_STATIC);
            sqlite3_bind_text(statement, 5, cJSON_GetStringValue(cJSON_GetObjectItem(package, "description")), strlen(cJSON_GetStringValue(cJSON_GetObjectItem(package, "description"))), SQLITE_STATIC);
            if (sqlite3_step(statement) != SQLITE_DONE)
            {
                sqlite3_finalize(statement);
                success = 0; // Every 
                break;
            }
            sqlite3_finalize(statement);



            // Now we index artifacts
            success = 1;
            const cJSON* artifact;
            cJSON_ArrayForEach(artifact, cJSON_GetObjectItem(package, "artifacts"))
            {
                const char* zSQL = "INSERT INTO artifacts (url, repository, id, version_major, version_minor, version_patch) VALUES (?, ?, ?, ?, ?, ?);";
                sqlite3_stmt* statement;
                sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
                sqlite3_bind_text(statement, 1, cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "url")), strlen(cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "url"))), SQLITE_STATIC);
                sqlite3_bind_text(statement, 2, repositories[i].id, strlen(repositories[i].id), SQLITE_STATIC);
                sqlite3_bind_text(statement, 3, package->string, strlen(package->string), SQLITE_STATIC);
                sqlite3_bind_int64(statement, 4, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 0)));
                sqlite3_bind_int64(statement, 5, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 1)));
                sqlite3_bind_int64(statement, 6, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 2)));
                if (sqlite3_step(statement) != SQLITE_DONE)
                {
                    sqlite3_finalize(statement);
                    success = 0;
                    break; // If ANY package fails then we don't index this repo - simple as that
                }
                sqlite3_finalize(statement);
            }

            if (!success)
            {
                break; // Stop adding packages if one fails
            }
        }

        cJSON_Delete(json);
        SimpleGET_Cleanup(&request);

        if (!success)
        {
            sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
            return KPM_GENERIC_ERROR;
        }

        sqlite3_exec(kpm->db, "COMMIT", NULL, NULL, NULL);
        return KPM_OK;
    }
}