#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <assert.h>
#include <stdio.h>
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

    package->repository = NULL;
    package->id = NULL;
    package->name = NULL;
    package->description = NULL;
    package->author = NULL;
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
 * @brief Free the properties of a package - WILL NOT FREE THE POINTER ITSELF
 * 
 * @param package The package to free the properties of
 */
 void KPM_FreeIndexedArtifact(struct IndexedArtifact* artifact)
 {
     free(artifact->repository);
     free(artifact->id);
     free(artifact->url);
 
     artifact->repository = NULL;
     artifact->id = NULL;
     artifact->url = NULL;
 }
 
 /**
  * @brief Free an allocated list of packages - such as returned by KPM_ListRepositoryPackages
  * 
  * @param packageCount The number of packages in the array
  * @param packages The package array
  */
 void KPM_FreeIndexedArtifactList(size_t artifactCount, struct IndexedArtifact* artifacts)
 {
     for (size_t i=0; i < artifactCount; i++)
     {
         KPM_FreeIndexedArtifact(&artifacts[i]);
     }
     free(artifacts);
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

    sqlite3_stmt* statement;
    if (repositoryId == NULL)
    {
        const char* zSQL = "SELECT repository, id, name, author, description FROM packages WHERE AND id=? LIMIT 1;";
        sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
        sqlite3_bind_text(statement, 1, packageId, strlen(packageId), SQLITE_STATIC);
    }
    else {
        const char* zSQL = "SELECT repository, id, name, author, description FROM packages WHERE repository=? AND id=? LIMIT 1;";
        sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
        sqlite3_bind_text(statement, 1, repositoryId, strlen(repositoryId), SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, packageId, strlen(packageId), SQLITE_STATIC);
    }

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        package->repository = strdup((const char*) sqlite3_column_text(statement, 0));
        package->id = strdup((const char*) sqlite3_column_text(statement, 1));
        package->name = strdup((const char*) sqlite3_column_text(statement, 2));
        package->author = strdup((const char*) sqlite3_column_text(statement, 4));
        package->description = strdup((const char*) sqlite3_column_text(statement, 3));
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
    sprintf(paddedQuery, "%%%s%%", query);    

    const char* zSQL = "SELECT COUNT(), repository, id, name, author, description FROM packages WHERE id LIKE ? OR name LIKE ?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, paddedQuery, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, paddedQuery, strlen(paddedQuery), SQLITE_STATIC);

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

    free(paddedQuery);

    if (status != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

/**
 * @brief List the indexed artifacts of a package 
 * 
 * @param kpm The KPM object
 * @param repositoryId The repository ID of the package
 * @param packageId The package ID of the package
 * @param artifactCount Pointer to store the artifact count
 * @param artifacts Pointer to store the artifact array
 * @return enum KPMResult 
 */
enum KPMResult KPM_ListPackageArtifacts(struct KPM* kpm, const char* repositoryId, const char* packageId, size_t* artifactCount, struct IndexedArtifact** artifacts)
{
    *artifactCount = 0;
    if (artifacts != NULL)
    {
        *artifacts = NULL;
    }
    
    const char* zSQL = "SELECT COUNT(), url, repository, id, version_major, version_minor, version_patch FROM artifacts WHERE repository=? AND id=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, packageId, -1, SQLITE_STATIC);

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *artifactCount = sqlite3_column_int64(statement, 0);
        }
        
        if (artifacts != NULL && *artifactCount > 0)
        {
            if (!*artifacts)
            {
                *artifacts = malloc(*artifactCount * sizeof(struct IndexedArtifact));
            }

            (*artifacts)[i].url = strdup((const char*) sqlite3_column_text(statement, 1));
            (*artifacts)[i].repository = strdup((const char*) sqlite3_column_text(statement, 2));
            (*artifacts)[i].id = strdup((const char*) sqlite3_column_text(statement, 3));

            (*artifacts)[i].version.major = sqlite3_column_int64(statement, 4);
            (*artifacts)[i].version.minor = sqlite3_column_int64(statement, 5);
            (*artifacts)[i].version.patch = sqlite3_column_int64(statement, 6);
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