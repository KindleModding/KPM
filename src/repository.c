#include <stdlib.h>
#include <string.h>

#include "cjson/cJSON.h"

#include "kpm/kpm.h"
#include "simpleGET.h"

void KPM_FreeRepository(struct Repository *repository)
{
    free(repository->id);
    free(repository->url);
    free(repository->name);
    free(repository->description);
    repository->id = NULL;
    repository->url = NULL;
    repository->name = NULL;
    repository->description = NULL;
}

enum KPMResult KPM_ListRepositories(struct KPM* kpm, size_t* repositoryCount, struct Repository** repositories)
{
    if (repositories != NULL)
    {
        *repositories = NULL;
    }
    
    const char* zSQL = "SELECT COUNT(), id, url, name, description FROM repositories;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
  
    int status;
    for (*repositoryCount = 0; (status = sqlite3_step(statement)) == SQLITE_ROW; (*repositoryCount)++)
    {
        if (repositories != NULL)
        {
            if (*repositories == NULL)
            {
                *repositories = malloc(sqlite3_column_int64(statement, 0) * sizeof(struct Repository));
            }

            (*repositories)[*repositoryCount].id = strdup((char*) sqlite3_column_text(statement, 1));
            (*repositories)[*repositoryCount].url = strdup((char*) sqlite3_column_text(statement, 2));
            (*repositories)[*repositoryCount].name = strdup((char*) sqlite3_column_text(statement,3));
            (*repositories)[*repositoryCount].description = strdup((char*) sqlite3_column_text(statement, 4));
        }
    }

    sqlite3_finalize(statement);

    if (status != SQLITE_DONE)
    {
        return KPM_SQLITE_ERROR;
    }

    return KPM_OK;
}

enum KPMResult KPM_GetRepository(struct KPM *kpm, const char *repositoryId, struct Repository* repository)
{
    repository->id = NULL;
    repository->url = NULL;
    repository->name = NULL;
    repository->description = NULL;

    const char* zSQL = "SELECT * FROM repositories WHERE id=? LIMIT=1;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, strlen(repositoryId), SQLITE_STATIC);

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        repository->id = strdup((const char*) sqlite3_column_text(statement, 0));
        repository->url = strdup((const char*) sqlite3_column_text(statement, 1));
        repository->name = strdup((const char*) sqlite3_column_text(statement,2));
        repository->description = strdup((const char*) sqlite3_column_text(statement, 3));
    } else {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

enum KPMResult KPM_AddRepository(struct KPM *kpm, const char *url, struct Repository* repository)
{
    struct SimpleGETRequest request;
    SimpleGET_Initialise(&request, url);
    if (SimpleGET_Perform(&request) != CURLE_OK)
    {
        return KPM_CURL_ERROR;
    }

    if (request.response_code != 200)
    {
        return KPM_INVALID_RESPONSE_CODE;
    }


    cJSON* json = cJSON_Parse(request.buffer);
    

    if (
        !cJSON_IsString(cJSON_GetObjectItem(json, "id")) ||
        !cJSON_IsString(cJSON_GetObjectItem(json, "name")) ||
        !cJSON_IsString(cJSON_GetObjectItem(json, "description")) ||
        !cJSON_IsArray(cJSON_GetObjectItem(json, "packages"))
    )
    {
        cJSON_Delete(json);
        SimpleGET_Cleanup(&request);
        return KPM_INVALID_RESPONSE_CONTENT;
    }

    const char* zSQL = "INSERT INTO repositories (id, url, name, description) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, cJSON_GetStringValue(cJSON_GetObjectItem(json, "id")), strlen(cJSON_GetStringValue(cJSON_GetObjectItem(json, "id"))), SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, url, strlen(url), SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, cJSON_GetStringValue(cJSON_GetObjectItem(json, "name")), strlen(cJSON_GetStringValue(cJSON_GetObjectItem(json, "name"))), SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, cJSON_GetStringValue(cJSON_GetObjectItem(json, "description")), strlen(cJSON_GetStringValue(cJSON_GetObjectItem(json, "description"))), SQLITE_STATIC);

    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        cJSON_Delete(json);
        SimpleGET_Cleanup(&request);
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    if (repository != NULL)
    {
        repository->id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "id"))),
        repository->name = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "name")));
        repository->description = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(json, "description")));
        repository->url = strdup(url);
    }
    cJSON_Delete(json);
    SimpleGET_Cleanup(&request);
    return KPM_OK;
}

enum KPMResult KPM_RemoveRepository(struct KPM *kpm, const char* repositoryId)
{
    const char* zSQL = "DELETE FROM repositories WHERE id=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, strlen(repositoryId), SQLITE_STATIC);

    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

enum KPMResult KPM_ListRepositoryPackages(struct KPM* kpm, char* repositoryId, size_t* packageCount, struct IndexedPackage** packages)
{
    *packages = NULL;
    const char* zSQL = "SELECT COUNT(id), * FROM packages WHERE repository=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, strlen(repositoryId), SQLITE_STATIC);

    int status;
    for (*packageCount=0; (status = sqlite3_step(statement)) == SQLITE_ROW; (*packageCount)++)
    {
        if (!*packages)
        {
            *packages = malloc(sqlite3_column_int64(statement, 0) * sizeof(struct IndexedPackage));
        }

        (*packages)[*packageCount].repository = strdup((const char*) sqlite3_column_text(statement, 0));
        (*packages)[*packageCount].id = strdup((const char*) sqlite3_column_text(statement, 1));
        (*packages)[*packageCount].name = strdup((const char*) sqlite3_column_text(statement, 2));
        (*packages)[*packageCount].description = strdup((const char*) sqlite3_column_text(statement, 3));
        (*packages)[*packageCount].author = strdup((const char*) sqlite3_column_text(statement, 4));
        (*packages)[*packageCount].icon = strdup((const char*) sqlite3_column_text(statement, 5));
    }

    if (status != SQLITE_DONE)
    {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}