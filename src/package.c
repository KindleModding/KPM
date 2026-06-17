#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal_utils.h"

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

void KPM_FreeIndexedPackageList(size_t packageCount, struct IndexedPackage* packages)
{
    for (size_t i=0; i < packageCount; i++)
        KPM_FreeIndexedPackage(&packages[i]);
        
    free(packages);
}

enum KPMResult KPM_GetPackage(struct KPM* kpm, const char* repository, const char* id, struct IndexedPackage* package)
{
    package->repository = NULL;
    package->id = NULL;
    package->name = NULL;
    package->description = NULL;
    package->author = NULL;

    sqlite3_stmt* statement;
    if (repository == NULL)
    {
        const char* zSQL = "SELECT repository, id, name, author, description FROM packages WHERE AND id=? LIMIT 1;";
        sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    }
    else {
        const char* zSQL = "SELECT repository, id, name, author, description FROM packages WHERE repository=? AND id=? LIMIT 1;";
        sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, repository, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, id, -1, SQLITE_STATIC);
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

enum KPMResult KPM_GetPackages(struct KPM* kpm, const char* id, size_t* packageCount, struct IndexedPackage** packages)
{
    *packageCount = 0;
    if (packages != NULL)
        *packages = NULL;

    const char* zSQL = "SELECT (SELECT COUNT() FROM packages WHERE id=?), repository, id, name, author, description FROM packages WHERE id=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, id, -1, SQLITE_STATIC);

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

enum KPMResult KPM_SearchPackages(struct KPM* kpm, const char* query, size_t* packageCount, struct IndexedPackage** packages)
{
    *packageCount = 0;
    if (packages != NULL)
        *packages = NULL;

    char* paddedQuery = asprintf_hd("%%%s%%", query);    

    const char* zSQL = "SELECT (SELECT COUNT() FROM packages WHERE id LIKE ? OR name LIKE ?), repository, id, name, author, description FROM packages WHERE id LIKE ? OR name LIKE ?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, paddedQuery, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, paddedQuery, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, paddedQuery, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, paddedQuery, -1, SQLITE_STATIC);

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

enum KPMResult KPM_ListPackageArtifacts(struct KPM* kpm, const char* repositoryId, const char* packageId, size_t* artifactCount, struct IndexedArtifact** artifacts)
{
    *artifactCount = 0;
    if (artifacts != NULL)
        *artifacts = NULL;
    
    const char* zSQL;
    if (repositoryId == NULL)
    {
        zSQL = "SELECT (SELECT COUNT() FROM artifacts WHERE id=?), url, repository, id, version_major, version_minor, version_patch FROM artifacts WHERE id=? ORDER BY version_major DESC, version_minor DESC;";
    }
    else
    {
        zSQL = "SELECT (SELECT COUNT() FROM artifacts WHERE repository=? AND id=?), url, repository, id, version_major, version_minor, version_patch FROM artifacts WHERE repository=? AND id=? ORDER BY version_major DESC, version_minor DESC;";
    }
    
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);

    if (repositoryId == NULL)
    {
        sqlite3_bind_text(statement, 1, packageId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, packageId, -1, SQLITE_STATIC);
    }
    else
    {
        sqlite3_bind_text(statement, 1, repositoryId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, packageId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 3, repositoryId, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 4, packageId, -1, SQLITE_STATIC);
    }

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
