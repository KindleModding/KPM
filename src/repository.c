#include <stdlib.h>
#include <string.h>

#include "cjson/cJSON.h"

#include "kpm/kpm.h"
#include "simpleGET.h"

/**
 * @brief Free a repository object's properties (WILL NOT FREE THE POINTER ITSELF)
 * 
 * @param repository 
 */
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

/**
 * @brief Free an allocated repository list returned by KPM_ListRepositories
 * 
 * @param repositoryCount The number of repositories in the list
 * @param repositories The list of repositories
 */
void KPM_FreeRepositoryList(size_t repositoryCount, struct Repository* repositories)
{
    for (size_t i=0; i < repositoryCount; i++)
    {
        KPM_FreeRepository(&repositories[i]);
    }
    free(repositories);
}

/**
 * @brief List indexed repositories
 * 
 * @param kpm The KPM object
 * @param repositoryCount A pointer to store the number of repositories indexed
 * @param repositories A pointer to allocate and store store the indexed repository objects in (must be freed with KPM_FreeRepositoryList) - can be NULL to only get a count
 * @return enum KPMResult 
 */
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

/**
 * @brief Get a single indexed repository from its Id
 * 
 * @param kpm The KPM object
 * @param repositoryId The Id of the repository to get
 * @param repository A pointer to return the repository object (Values will be NULL if the repository could not be fetched)
 * @return enum KPMResult 
 */
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

/**
 * @brief Index a repository from a URL
 * 
 * @param kpm The KPM object
 * @param url The URL to the repository manifest file
 * @param repository A pointer to return the indexed repository object
 * @return enum KPMResult 
 */
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

/**
 * @brief Remove an indexed repository by its Id
 * 
 * @param kpm The KPM object
 * @param repositoryId The Id of the repository to remove
 * @return enum KPMResult 
 */
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

/**
 * @brief Return a list of indexed packages under a repository
 * 
 * @param kpm The KPM object
 * @param repositoryId The Id of the repository to get packages for
 * @param packageCount A pointer to return the number of packages indexed
 * @param packages A pointer to allocate and return an array of packages - Must be freed with KPM_FreeIndexedPackageList - Can be NULL to return only a count
 * @return enum KPMResult 
 */
enum KPMResult KPM_ListRepositoryPackages(struct KPM* kpm, char* repositoryId, size_t* packageCount, struct IndexedPackage** packages)
{
    if (packages != NULL)
    {
        *packages = NULL;
    }
    
    const char* zSQL = "SELECT COUNT(id), * FROM packages WHERE repository=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, strlen(repositoryId), SQLITE_STATIC);

    int status;
    for (*packageCount=0; (status = sqlite3_step(statement)) == SQLITE_ROW; (*packageCount)++)
    {
        if (packages != NULL)
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
    }

    if (status != SQLITE_DONE)
    {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}