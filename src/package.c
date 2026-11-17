#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal_utils.h"

void KPM_FreeIndexedPackage(KPM_IndexedPackage *package)
{
    free(package->repository);
    free(package->id);
    free(package->name);
    free(package->description);
    free(package->category);
    free(package->subcategory);
    free(package->author);
    free(package->icon);

    package->repository = NULL;
    package->id = NULL;
    package->name = NULL;
    package->description = NULL;
    package->category = NULL;
    package->subcategory = NULL;
    package->author = NULL;
    package->icon = NULL;
}

void KPM_FreeIndexedPackageList(size_t package_count, KPM_IndexedPackage *packages)
{
    for (size_t i=0; i < package_count; i++)
        KPM_FreeIndexedPackage(&packages[i]);
        
    free(packages);
}

KPM_Result KPM_GetPackage(KPM *kpm, const char *repository, const char *id, KPM_IndexedPackage *package)
{
    package->repository = NULL;
    package->id = NULL;
    package->name = NULL;
    package->description = NULL;
    package->category = NULL;
    package->subcategory = NULL;
    package->author = NULL;
    package->icon = NULL;

    sqlite3_stmt *statement;
    if (repository == NULL)
    {
        const char *zSQL = "SELECT repository, id, name, author, description, category, subcategory, icon FROM packages WHERE AND id=? LIMIT 1;";
        sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    }
    else {
        const char *zSQL = "SELECT repository, id, name, author, description, category, subcategory, icon FROM packages WHERE repository=? AND id=? LIMIT 1;";
        sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, repository, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, id, -1, SQLITE_STATIC);
    }

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        package->repository = strdup((const char*) sqlite3_column_text(statement, 0));
        package->id = strdup((const char*) sqlite3_column_text(statement, 1));
        package->name = strdup((const char*) sqlite3_column_text(statement, 2));
        package->author = strdup((const char*) sqlite3_column_text(statement, 3));
        package->description = strdup((const char*) sqlite3_column_text(statement, 4));
        package->category = strdup((const char*) sqlite3_column_text(statement, 5));
        if (sqlite3_column_type(statement, 6) != SQLITE_NULL)
            package->subcategory = strdup((const char*) sqlite3_column_text(statement, 6));
        if (sqlite3_column_type(statement, 5) != SQLITE_NULL)
            package->icon = strdup((const char*) sqlite3_column_text(statement, 7));
    } else {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

KPM_Result KPM_GetPackages(KPM *kpm, const char *id, size_t *package_count, KPM_IndexedPackage **packages)
{
    *package_count = 0;
    if (packages != NULL)
        *packages = NULL;

    const char *zSQL = "SELECT (SELECT COUNT() FROM packages WHERE id=?), repository, id, name, author, description, category, subcategory, icon FROM packages WHERE id=?;";
    sqlite3_stmt *statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, id, -1, SQLITE_STATIC);

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *package_count = sqlite3_column_int64(statement, 0);
        }

        if (packages != NULL && *package_count > 0)
        {
            if (!*packages)
            {
                *packages = malloc(*package_count * sizeof(KPM_IndexedPackage));
            }

            (*packages)[i].repository = strdup((const char*) sqlite3_column_text(statement, 1));
            (*packages)[i].id = strdup((const char*) sqlite3_column_text(statement, 2));
            (*packages)[i].name = strdup((const char*) sqlite3_column_text(statement, 3));
            (*packages)[i].author = strdup((const char*) sqlite3_column_text(statement, 4));
            (*packages)[i].description = strdup((const char*) sqlite3_column_text(statement, 5));
            (*packages)[i].category = strdup((const char*) sqlite3_column_text(statement, 6));
            if (sqlite3_column_type(statement, 7) != SQLITE_NULL)
                (*packages)[i].subcategory = strdup((const char*) sqlite3_column_text(statement, 7));
            else
                (*packages)[i].subcategory = NULL;
            if (sqlite3_column_type(statement, 8) != SQLITE_NULL)
                (*packages)[i].icon = strdup((const char*) sqlite3_column_text(statement, 8));
            else
                (*packages)[i].icon = NULL;
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

KPM_Result KPM_SearchPackages(KPM *kpm, const char *query, size_t *package_count, KPM_IndexedPackage **packages)
{
    *package_count = 0;
    if (packages != NULL)
        *packages = NULL;

    char *paddedQuery = asprintf_hd("%%%s%%", query);    

    const char *zSQL = "SELECT (SELECT COUNT() FROM packages WHERE id LIKE ? OR name LIKE ?), repository, id, name, author, description, category, subcategory, icon FROM packages WHERE id LIKE ? OR name LIKE ?;";
    sqlite3_stmt *statement;
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
            *package_count = sqlite3_column_int64(statement, 0);
        }

        if (packages != NULL && *package_count > 0)
        {
            if (!*packages)
            {
                *packages = malloc(*package_count * sizeof(KPM_IndexedPackage));
            }

            (*packages)[i].repository = strdup((const char*) sqlite3_column_text(statement, 1));
            (*packages)[i].id = strdup((const char*) sqlite3_column_text(statement, 2));
            (*packages)[i].name = strdup((const char*) sqlite3_column_text(statement, 3));
            (*packages)[i].author = strdup((const char*) sqlite3_column_text(statement, 4));
            (*packages)[i].description = strdup((const char*) sqlite3_column_text(statement, 5));
            (*packages)[i].category = strdup((const char*) sqlite3_column_text(statement, 6));
            if (sqlite3_column_type(statement, 7) != SQLITE_NULL)
                (*packages)[i].subcategory = strdup((const char*) sqlite3_column_text(statement, 7));
            else
                (*packages)[i].subcategory = NULL;
            if (sqlite3_column_type(statement, 8) != SQLITE_NULL)
                (*packages)[i].icon = strdup((const char*) sqlite3_column_text(statement, 8));
            else
                (*packages)[i].icon = NULL;
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

KPM_Result KPM_ListPackageArtifacts(KPM *kpm, const char *repository_id, const char *package_id, size_t *artifact_count, KPM_Artifact **artifacts)
{
    *artifact_count = 0;
    if (artifacts != NULL)
        *artifacts = NULL;
    
    const char *zSQL;
    if (repository_id == NULL)
    {
        zSQL = "SELECT (SELECT COUNT() FROM artifacts WHERE id=?), repository, id, url, installed_size, digest, version_major, version_minor, version_patch FROM artifacts WHERE id=? ORDER BY version_major DESC, version_minor DESC, version_patch DESC;";
    }
    else
    {
        zSQL = "SELECT (SELECT COUNT() FROM artifacts WHERE repository=? AND id=?), repository, id, url, installed_size, digest, version_major, version_minor, version_patch FROM artifacts WHERE repository=? AND id=? ORDER BY version_major DESC, version_minor DESC, version_patch DESC;";
    }
    
    sqlite3_stmt *statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);

    if (repository_id == NULL)
    {
        sqlite3_bind_text(statement, 1, package_id, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, package_id, -1, SQLITE_STATIC);
    }
    else
    {
        sqlite3_bind_text(statement, 1, repository_id, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, package_id, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 3, repository_id, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 4, package_id, -1, SQLITE_STATIC);
    }

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *artifact_count = sqlite3_column_int64(statement, 0);
        }
        
        if (artifacts != NULL && *artifact_count > 0)
        {
            if (!*artifacts)
            {
                *artifacts = malloc(*artifact_count * sizeof(KPM_Artifact));
            }

            (*artifacts)[i].repository = strdup((const char*) sqlite3_column_text(statement, 1));
            (*artifacts)[i].id = strdup((const char*) sqlite3_column_text(statement, 2));
            (*artifacts)[i].url = strdup((const char*) sqlite3_column_text(statement, 3));

            if (sqlite3_column_type(statement, 4) != SQLITE_NULL)
                (*artifacts)[i].installed_size = sqlite3_column_int64(statement, 4);
            else
                (*artifacts)[i].installed_size = 0;

            if (sqlite3_column_type(statement, 5) != SQLITE_NULL)
                (*artifacts)[i].digest = strdup((const char*) sqlite3_column_text(statement, 5));
            else
                (*artifacts)[i].digest = NULL;

            (*artifacts)[i].version.major = sqlite3_column_int64(statement, 6);
            (*artifacts)[i].version.minor = sqlite3_column_int64(statement, 7);
            (*artifacts)[i].version.patch = sqlite3_column_int64(statement, 8);
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
