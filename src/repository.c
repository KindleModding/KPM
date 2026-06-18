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

void KPM_FreeRepositoryList(size_t repositoryCount, struct Repository* repositories)
{
    for (size_t i=0; i < repositoryCount; i++)
        KPM_FreeRepository(&repositories[i]);

    free(repositories);
}

enum KPMResult KPM_ListRepositories(struct KPM* kpm, size_t* repositoryCount, struct Repository** repositories)
{
    *repositoryCount = 0;
    if (repositories != NULL)
        *repositories = NULL;
    
    const char* zSQL = "SELECT (SELECT COUNT() FROM repositories), id, url, name, description FROM repositories;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
  
    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *repositoryCount = sqlite3_column_int64(statement, 0);
        }

        if (repositories != NULL && *repositoryCount > 0)
        {
            if (*repositories == NULL)
            {
                *repositories = malloc(*repositoryCount * sizeof(struct Repository));
            }

            (*repositories)[i].id = strdup((char*) sqlite3_column_text(statement, 1));
            (*repositories)[i].url = strdup((char*) sqlite3_column_text(statement, 2));
            (*repositories)[i].name = strdup((char*) sqlite3_column_text(statement,3));
            (*repositories)[i].description = strdup((char*) sqlite3_column_text(statement, 4));
        }
    }

    sqlite3_finalize(statement);

    if (status != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return KPM_SQLITE_ERROR;
    }

    return KPM_OK;
}

enum KPMResult KPM_GetRepository(struct KPM *kpm, const char *repositoryId, struct Repository* repository)
{
    if (repository != NULL)
    {
        repository->id = NULL;
        repository->url = NULL;
        repository->name = NULL;
        repository->description = NULL;
    }

    const char* zSQL = "SELECT id, url, name, description FROM repositories WHERE id=? LIMIT 1;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, -1, SQLITE_STATIC);

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        if (repository != NULL)
        {
            repository->id = strdup((const char*) sqlite3_column_text(statement, 0));
            repository->url = strdup((const char*) sqlite3_column_text(statement, 1));
            repository->name = strdup((const char*) sqlite3_column_text(statement,2));
            repository->description = strdup((const char*) sqlite3_column_text(statement, 3));
        }
    } else {
        sqlite3_finalize(statement);
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

enum KPMResult KPM_AddRepository(struct KPM *kpm, const char *url, struct Repository* repository, struct KPMIO* kpm_io)
{
    struct SimpleGETRequest request;
    SimpleGET_Initialise(&request, url);
    if (SimpleGET_Perform(&request) != CURLE_OK)
    {
        return KPM_CURL_ERROR;
    }

    if (request.response_code >= 400)
    {
        return KPM_INVALID_RESPONSE_CODE;
    }


    kpm_io->log(KPM_VERBOSITY_DEBUG, "Got JSON:\n%s", request.buffer);
    cJSON* json = cJSON_Parse(request.buffer);

    if (
        !cJSON_IsString(cJSON_GetObjectItem(json, "id")) ||
        !cJSON_IsString(cJSON_GetObjectItem(json, "name")) ||
        !cJSON_IsString(cJSON_GetObjectItem(json, "description")) ||
        !cJSON_IsObject(cJSON_GetObjectItem(json, "packages"))
    )
    {
        cJSON_Delete(json);
        SimpleGET_Cleanup(&request);
        return KPM_INVALID_RESPONSE_CONTENT;
    }

    kpm_io->log(KPM_VERBOSITY_DEBUG, "ID: %s", cJSON_GetStringValue(cJSON_GetObjectItem(json, "id")));
    kpm_io->log(KPM_VERBOSITY_DEBUG, "URL: %s", url);
    kpm_io->log(KPM_VERBOSITY_DEBUG, "NAME: %s", cJSON_GetStringValue(cJSON_GetObjectItem(json, "name")));
    kpm_io->log(KPM_VERBOSITY_DEBUG, "DESC: %s", cJSON_GetStringValue(cJSON_GetObjectItem(json, "description")));

    sqlite3_exec(kpm->db, "BEGIN", NULL, NULL, NULL);
    const char* zSQL = "INSERT INTO repositories (id, url, name, description) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, cJSON_GetStringValue(cJSON_GetObjectItem(json, "id")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, url, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, cJSON_GetStringValue(cJSON_GetObjectItem(json, "name")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, cJSON_GetStringValue(cJSON_GetObjectItem(json, "description")), -1, SQLITE_STATIC);

    int e = 0;
    if ((e = sqlite3_step(statement)) != SQLITE_DONE)
    {
        kpm_io->log(KPM_VERBOSITY_ERROR, "SQLite error: %i", e);
        sqlite3_finalize(statement);
        cJSON_Delete(json);
        SimpleGET_Cleanup(&request);
        sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
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

    sqlite3_exec(kpm->db, "COMMIT", NULL, NULL, NULL);
    return KPM_OK;
}

enum KPMResult KPM_RemoveRepository(struct KPM *kpm, const char* repositoryId)
{
    sqlite3_exec(kpm->db, "BEGIN", NULL, NULL, NULL);
    const char* zSQL = "DELETE FROM repositories WHERE id=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, -1, SQLITE_STATIC);

    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    sqlite3_exec(kpm->db, "COMMIT", NULL, NULL, NULL);
    return KPM_OK;
}

enum KPMResult KPM_ListRepositoryPackages(struct KPM* kpm, const char* repositoryId, size_t* packageCount, struct IndexedPackage** packages)
{
    *packageCount = 0;
    if (packages != NULL)
        *packages = NULL;
    
    const char* zSQL = "SELECT (SELECT COUNT() FROM packages WHERE repository=?), repository, id, name, author, description FROM packages WHERE repository=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, repositoryId, -1, SQLITE_STATIC);

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *packageCount = sqlite3_column_int64(statement, 0);
        }

        if (packages != NULL && *packageCount > 0)
        {
            if (!*packages)
            {
                *packages = malloc(*packageCount * sizeof(struct IndexedPackage));
            }

            (*packages)[i].repository = strdup((const char*) sqlite3_column_text(statement, 1));
            (*packages)[i].id = strdup((const char*) sqlite3_column_text(statement, 2));
            (*packages)[i].name = strdup((const char*) sqlite3_column_text(statement, 3));
            (*packages)[i].author = strdup((const char*) sqlite3_column_text(statement, 4));
            (*packages)[i].description = strdup((const char*) sqlite3_column_text(statement, 5));
        }
    }

    if (status != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}