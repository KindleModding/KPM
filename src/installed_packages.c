#include "kpm/kpm.h"
#include "sqlite3.h"
#include <stdlib.h>
#include <string.h>

void KPM_FreeInstalledPackage(KPM_InstalledPackage *package)
{
    free(package->id);
    free(package->repository);
    free(package->name);
    free(package->author);
    free(package->description);
    
    package->id = NULL;
    package->repository = NULL;
    package->name = NULL;
    package->author = NULL;
    package->description = NULL;
}

void KPM_FreeInstalledPackageList(size_t package_count, KPM_InstalledPackage *packages)
{
    for (size_t i=0; i < package_count; i++)
    {
        KPM_FreeInstalledPackage(&packages[i]);
    }
    free(packages);
}

KPM_Result KPM_GetInstalledPackage(KPM *kpm, const char *package_id, KPM_InstalledPackage *package)
{
    if (package != NULL)
    {
        package->id = NULL;
        package->repository = NULL;
        package->name = NULL;
        package->author = NULL;
        package->description = NULL;
    }

    sqlite3_stmt *statement;
    const char *zSQL = "SELECT id, repository, name, author, description, version_major, version_minor, version_patch, installed_as_dependency FROM installed_packages WHERE id=? LIMIT 1;";
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, package_id, -1, SQLITE_STATIC);

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        if (package != NULL)
        {
            package->id = strdup((const char*) sqlite3_column_text(statement, 0));
            if (sqlite3_column_type(statement, 1) != SQLITE_NULL)
                package->repository = strdup((const char*) sqlite3_column_text(statement, 1));
            else
                package->repository = NULL;
            package->name = strdup((const char*) sqlite3_column_text(statement, 2));
            package->author = strdup((const char*) sqlite3_column_text(statement, 3));
            package->description = strdup((const char*) sqlite3_column_text(statement, 4));
            package->version.major = sqlite3_column_int(statement, 5);
            package->version.minor = sqlite3_column_int(statement, 6);
            package->version.patch = sqlite3_column_int(statement, 7);
            package->installed_as_dependency = sqlite3_column_int(statement, 8);
        }
    } else {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

KPM_Result KPM_ListInstalledPackages(KPM *kpm, size_t *package_count, KPM_InstalledPackage **packages)
{
    *package_count = 0;
    if (packages != NULL)
        *packages = NULL;

    const char *zSQL = "SELECT (SELECT COUNT() FROM installed_packages), id, repository, name, author, description, version_major, version_minor, version_patch, installed_as_dependency FROM installed_packages;";
    sqlite3_stmt *statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);

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
                *packages = malloc(*package_count * sizeof(KPM_InstalledPackage));
            }

            (*packages)[i].id = strdup((const char*) sqlite3_column_text(statement, 1));
            if (sqlite3_column_type(statement, 2) != SQLITE_NULL)
                (*packages)[i].repository = strdup((const char*) sqlite3_column_text(statement, 2));
            else
                (*packages)[i].repository = NULL;
            (*packages)[i].name = strdup((const char*) sqlite3_column_text(statement, 3));
            (*packages)[i].author = strdup((const char*) sqlite3_column_text(statement, 4));
            (*packages)[i].description = strdup((const char*) sqlite3_column_text(statement, 5));
            (*packages)[i].version.major = sqlite3_column_int(statement, 6);
            (*packages)[i].version.minor = sqlite3_column_int(statement, 7);
            (*packages)[i].version.patch = sqlite3_column_int(statement, 8);
            (*packages)[i].installed_as_dependency = sqlite3_column_int(statement, 9);
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


KPM_Result KPM_ListInstalledPackageDependencies(KPM *kpm, const char *id, size_t *dependency_count, KPM_Dependency **dependencies)
{
    *dependency_count = 0;
    if (dependencies != NULL)
        *dependencies = NULL;
    
    const char *zSQL = "SELECT (SELECT COUNT() FROM current_dependencies WHERE dependent=?), dependency_id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch FROM current_dependencies WHERE dependent=?;";
    sqlite3_stmt *statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, id, -1, SQLITE_STATIC);

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *dependency_count = sqlite3_column_int64(statement, 0);
        }
        
        if (dependencies != NULL && *dependency_count > 0)
        {
            if (!*dependencies)
            {
                *dependencies = malloc(*dependency_count * sizeof(KPM_Dependency));
            }

            (*dependencies)[i].id = strdup((const char*) sqlite3_column_text(statement, 1));
            (*dependencies)[i].min_version.major = sqlite3_column_int(statement, 2);
            (*dependencies)[i].min_version.minor = sqlite3_column_int(statement, 3);
            (*dependencies)[i].min_version.patch = sqlite3_column_int(statement,4);
            (*dependencies)[i].max_version.major = sqlite3_column_int(statement, 5);
            (*dependencies)[i].max_version.minor = sqlite3_column_int(statement, 6);
            (*dependencies)[i].max_version.patch = sqlite3_column_int(statement,7);
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

KPM_Result KPM_ListInstalledPackageDependents(KPM *kpm, const char *id, size_t *dependent_count, KPM_Dependency **dependents)
{ // @TODO: Rework
    *dependent_count = 0;
    if (dependents != NULL)
        *dependents = NULL;
    
    const char *zSQL = "SELECT (SELECT COUNT() FROM current_dependencies WHERE dependancy_id=?), dependent, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch FROM current_dependencies WHERE dependency_id=?;";
    sqlite3_stmt *statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, id, -1, SQLITE_STATIC);

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *dependent_count = sqlite3_column_int64(statement, 0);
        }
        
        if (dependents != NULL && *dependent_count > 0)
        {
            if (!*dependents)
            {
                *dependents = malloc(*dependent_count * sizeof(KPM_Dependency));
            }

            (*dependents)[i].id = strdup((const char*) sqlite3_column_text(statement, 1));
            (*dependents)[i].min_version.major = sqlite3_column_int(statement, 2);
            (*dependents)[i].min_version.minor = sqlite3_column_int(statement, 3);
            (*dependents)[i].min_version.patch = sqlite3_column_int(statement,4);
            (*dependents)[i].max_version.major = sqlite3_column_int(statement, 5);
            (*dependents)[i].max_version.minor = sqlite3_column_int(statement, 6);
            (*dependents)[i].max_version.patch = sqlite3_column_int(statement,7);
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