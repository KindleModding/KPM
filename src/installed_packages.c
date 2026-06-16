#include "kpm/kpm.h"
#include <stdlib.h>
#include <string.h>

void KPM_FreeInstalledPackage(struct InstalledPackage* package)
{
    free(package->id);
    free(package->name);
    free(package->author);
    free(package->description);
    
    package->id = NULL;
    package->name = NULL;
    package->author = NULL;
    package->description = NULL;
}


void KPM_FreeInstalledPackageList(size_t packageCount, struct InstalledPackage* packages)
{
    for (size_t i=0; i < packageCount; i++)
    {
        KPM_FreeInstalledPackage(&packages[i]);
    }
    free(packages);
}

enum KPMResult KPM_GetInstalledPackage(struct KPM* kpm, const char* packageId, struct InstalledPackage* package)
{
    package->id = NULL;
    package->name = NULL;
    package->author = NULL;
    package->description = NULL;

    sqlite3_stmt* statement;
    const char* zSQL = "SELECT id, repository, name, author, description, version_major, version_minor, version_patch, installed_as_dependency FROM installed_packages WHERE AND id=? LIMIT 1;";
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, packageId, -1, SQLITE_STATIC);

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        package->id = strdup((const char*) sqlite3_column_text(statement, 0));
        package->repository = strdup((const char*) sqlite3_column_text(statement, 1));
        package->name = strdup((const char*) sqlite3_column_text(statement, 2));
        package->author = strdup((const char*) sqlite3_column_text(statement, 3));
        package->description = strdup((const char*) sqlite3_column_text(statement, 4));
        package->version.major = sqlite3_column_int(statement, 5);
        package->version.minor = sqlite3_column_int(statement, 6);
        package->version.patch = sqlite3_column_int(statement, 7);
        package->installed_as_dependency = sqlite3_column_int(statement, 8);
    } else {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

enum KPMResult KPM_ListInstalledPackages(struct KPM* kpm, size_t* packageCount, struct InstalledPackage** packages)
{
    *packageCount = 0;
    if (packages != NULL)
    {
        *packages = NULL;
    }

    const char* zSQL = "SELECT (SELECT COUNT() FROM installed_packages), id, repository, name, author, description, version_major, version_minor, version_patch, installed_as_dependency FROM installed_packages;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);

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
                *packages = malloc(*packageCount * sizeof(struct InstalledPackage));
            }

            (*packages)[i].id = strdup((const char*) sqlite3_column_text(statement, 1));
            (*packages)[i].repository = strdup((const char*) sqlite3_column_text(statement, 2));
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

void KPM_FreeInstalledPackageDependency(struct InstalledDependency* dependency)
{
    free(dependency->dependency_id);
    free(dependency->dependent);

    dependency->dependency_id = NULL;
    dependency->dependent = NULL;
}

void KPM_FreeInstalledPackageDependencyList(size_t dependencyCount, struct InstalledDependency* dependencies)
{
    for (size_t i=0; i < dependencyCount; i++)
    {
        KPM_FreeInstalledPackageDependency(dependencies + i);
    }
    free(dependencies);
}


enum KPMResult KPM_ListInstalledPackageDependencies(struct KPM* kpm, const char* id, size_t* dependencyCount, struct InstalledDependency** dependencies)
{
    *dependencyCount = 0;
    if (dependencies != NULL)
    {
        *dependencies = NULL;
    }
    
    const char* zSQL = "SELECT (SELECT COUNT() FROM current_dependencies WHERE dependent=?), dependent, dependency_id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch FROM current_dependencies WHERE dependent=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, id, -1, SQLITE_STATIC);

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *dependencyCount = sqlite3_column_int64(statement, 0);
        }
        
        if (dependencies != NULL && *dependencyCount > 0)
        {
            if (!*dependencies)
            {
                *dependencies = malloc(*dependencyCount * sizeof(struct ArtifactDependency));
            }

            (*dependencies)[i].dependent = strdup((const char*) sqlite3_column_text(statement, 1));
            (*dependencies)[i].dependency_id = strdup((const char*) sqlite3_column_text(statement, 2));
            (*dependencies)[i].min_version.major = sqlite3_column_int(statement, 3);
            (*dependencies)[i].min_version.minor = sqlite3_column_int(statement, 4);
            (*dependencies)[i].min_version.patch = sqlite3_column_int(statement,5);
            (*dependencies)[i].max_version.major = sqlite3_column_int(statement, 6);
            (*dependencies)[i].max_version.minor = sqlite3_column_int(statement, 7);
            (*dependencies)[i].max_version.patch = sqlite3_column_int(statement,8);
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

enum KPMResult KPM_ListInstalledPackageDependents(struct KPM* kpm, const char* id, size_t* dependentCount, struct InstalledDependency** dependents)
{
    *dependentCount = 0;
    if (dependents != NULL)
    {
        *dependents = NULL;
    }
    
    const char* zSQL = "SELECT (SELECT COUNT() FROM current_dependencies WHERE dependancy=?), dependent, dependency_id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch FROM current_dependencies WHERE dependency=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, id, -1, SQLITE_STATIC);

    int status;
    for (int i=0; (status = sqlite3_step(statement)) == SQLITE_ROW; i++)
    {
        if (i == 0)
        {
            *dependentCount = sqlite3_column_int64(statement, 0);
        }
        
        if (dependents != NULL && *dependentCount > 0)
        {
            if (!*dependents)
            {
                *dependents = malloc(*dependentCount * sizeof(struct InstalledPackage));
            }

            (*dependents)[i].dependent = strdup((const char*) sqlite3_column_text(statement, 1));
            (*dependents)[i].dependency_id = strdup((const char*) sqlite3_column_text(statement, 2));
            (*dependents)[i].min_version.major = sqlite3_column_int(statement, 3);
            (*dependents)[i].min_version.minor = sqlite3_column_int(statement, 4);
            (*dependents)[i].min_version.patch = sqlite3_column_int(statement,5);
            (*dependents)[i].max_version.major = sqlite3_column_int(statement, 6);
            (*dependents)[i].max_version.minor = sqlite3_column_int(statement, 7);
            (*dependents)[i].max_version.patch = sqlite3_column_int(statement,8);
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