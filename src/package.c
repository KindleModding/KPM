#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Free the properties of a package - WILL NOT FREE THE POINTER ITSELF
 * 
 * @param package The package to free the properties of
 */
void KPM_FreeIndexedPackage(struct IndexedPackage* package)
{
    free(package->repository);
    free(package->id);
    free(package->name);
    free(package->description);
    free(package->author);
    free(package->icon);

    package->repository = NULL;
    package->id = NULL;
    package->name = NULL;
    package->description = NULL;
    package->author = NULL;
    package->icon = NULL;
}

/**
 * @brief Free an allocated list of packages - such as returned by KPM_ListRepositoryPackages
 * 
 * @param packageCount The number of packages in the array
 * @param packages The package array
 */
void KPM_FreeIndexedPackageList(size_t packageCount, struct IndexedPackage* packages)
{
    for (size_t i=0; i < packageCount; i++)
    {
        KPM_FreeIndexedPackage(&packages[i]);
    }
    free(packages);
}

/**
 * @brief Get a package given a repositoryId (optional) and a packageId
 * 
 * @param kpm The KPM object
 * @param repositoryId The id of the repository to get the package from (or NULL)
 * @param packageId The id of the package to get
 * @param package A pointer to write the returned package info
 * @return enum KPMResult 
 */
enum KPMResult KPM_GetPackage(struct KPM* kpm, const char* repositoryId, const char* packageId, struct IndexedPackage* package)
{
    package->repository = NULL;
    package->id = NULL;
    package->name = NULL;
    package->description = NULL;
    package->author = NULL;
    package->icon = NULL;

    sqlite3_stmt* statement;
    if (repositoryId == NULL)
    {
        const char* zSQL = "SELECT * FROM packages WHERE AND id=? LIMIT=1;";
        sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
        sqlite3_bind_text(statement, 1, packageId, strlen(packageId), SQLITE_STATIC);
    }
    else {
        const char* zSQL = "SELECT * FROM packages WHERE repository=? AND id=? LIMIT=1;";
        sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
        sqlite3_bind_text(statement, 1, repositoryId, strlen(repositoryId), SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, packageId, strlen(packageId), SQLITE_STATIC);
    }

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        package->repository = strdup((const char*) sqlite3_column_text(statement, 0));
        package->id = strdup((const char*) sqlite3_column_text(statement, 1));
        package->name = strdup((const char*) sqlite3_column_text(statement, 2));
        package->description = strdup((const char*) sqlite3_column_text(statement, 3));
        package->author = strdup((const char*) sqlite3_column_text(statement, 4));
        package->icon = strdup((const char*) sqlite3_column_text(statement, 5));
    } else {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

/**
 * @brief Return a list of packages where either the name or id contain the query
 * 
 * @param kpm 
 * @param query 
 * @param packageCount 
 * @param packages 
 * @return enum KPMResult 
 */
enum KPMResult KPM_SearchPackages(struct KPM* kpm, const char* query, size_t* packageCount, struct IndexedPackage** packages)
{
    if (packages != NULL)
    {
        *packages = NULL;
    }

    char* paddedQuery = malloc(strlen(query) + 2 + 1);
    paddedQuery[0] = '%';
    strcpy(&paddedQuery[1], query);
    paddedQuery[strlen(query) + 1] = '%';
    paddedQuery[strlen(query) + 2] = '\0';

    const char* zSQL = "SELECT COUNT(), repository, id, name, description, author, icon FROM packages WHERE id LIKE ? OR name LIKE ?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, paddedQuery, strlen(paddedQuery), SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, paddedQuery, strlen(paddedQuery), SQLITE_STATIC);

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *packageCount = sqlite3_column_int64(statement, 0);
        }

        if (packages != NULL)
        {
            if (!*packages)
            {
                *packages = malloc(*packageCount * sizeof(struct IndexedPackage));
            }

            (*packages)[i].repository = strdup((const char*) sqlite3_column_text(statement, 0));
            (*packages)[i].id = strdup((const char*) sqlite3_column_text(statement, 1));
            (*packages)[i].name = strdup((const char*) sqlite3_column_text(statement, 2));
            (*packages)[i].description = strdup((const char*) sqlite3_column_text(statement, 3));
            (*packages)[i].author = strdup((const char*) sqlite3_column_text(statement, 4));
            (*packages)[i].icon = strdup((const char*) sqlite3_column_text(statement, 5));
        }
    }

    free(paddedQuery);

    if (status != SQLITE_DONE)
    {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}


enum KPMResult KPM_GetPackageArtifacts(struct KPM* kpm, const char* repositoryId, const char* packageId, size_t* artifactCount, struct IndexedArtifact** artifacts)
{
    if (artifacts != NULL)
    {
        *artifacts = NULL;
    }
    
    const char* zSQL = "SELECT COUNT(), url, repository, id, version_major, version_minor, version_patch, supported_arch, supported_kindles FROM packages WHERE repository=? AND package=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, strlen(repositoryId), SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, packageId, strlen(repositoryId), SQLITE_STATIC);

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *artifactCount = sqlite3_column_int64(statement, 0);
        }
        
        if (artifacts != NULL)
        {
            if (!*artifacts)
            {
                *artifacts = malloc(*artifactCount * sizeof(struct IndexedPackage));
            }

            (*artifacts)[i].url = strdup((const char*) sqlite3_column_text(statement, 0));
            (*artifacts)[i].repository = strdup((const char*) sqlite3_column_text(statement, 1));
            (*artifacts)[i].id = strdup((const char*) sqlite3_column_text(statement, 2));

            (*artifacts)[i].version.major = sqlite3_column_int64(statement, 3);
            (*artifacts)[i].version.minor = sqlite3_column_int64(statement, 4);
            (*artifacts)[i].version.patch = sqlite3_column_int64(statement, 5);
            
            //(*artifacts)[i].supported_arch = strdup((const char*) sqlite3_column_text(statement, 4));
            //(*artifacts)[i].supported_kindles = strdup((const char*) sqlite3_column_text(statement, 5));
        }
    }

    if (status != SQLITE_DONE)
    {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}