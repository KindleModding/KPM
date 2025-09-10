#include "kpm/kpm.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void KPM_FreeArtifactDependency(struct ArtifactDependency* dependency)
{
    free(dependency->artifact);
    free(dependency->repository);
    free(dependency->id);

    dependency->repository = NULL;
    dependency->artifact = NULL;
    dependency->id = NULL;
}

void KPM_FreeArtifactDependencyList(size_t dependencyCount, struct ArtifactDependency* dependencies)
{
    for (size_t i=0; i < dependencyCount; i++)
    {
        KPM_FreeArtifactDependency(dependencies + i);
    }
}


enum KPMResult KPM_ListArtifactDependencies(struct KPM* kpm, const char* artifact, size_t* dependencyCount, struct ArtifactDependency** dependencies)
{
    *dependencyCount = 0;
    if (dependencies != NULL)
    {
        *dependencies = NULL;
    }
    
    const char* zSQL = "SELECT COUNT(), artifact, repository, id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch FROM dependencies WHERE artifact=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, strlen(zSQL), &statement, NULL);
    sqlite3_bind_text(statement, 1, artifact, -1, SQLITE_STATIC);

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
                *dependencies = malloc(*dependencyCount * sizeof(struct IndexedArtifact));
            }

            (*dependencies)[i].artifact = strdup((const char*) sqlite3_column_text(statement, 1));
            (*dependencies)[i].repository = strdup((const char*) sqlite3_column_text(statement, 2));
            (*dependencies)[i].id = strdup((const char*) sqlite3_column_text(statement, 3));
            (*dependencies)[i].min_version.major = sqlite3_column_int(statement, 4);
            (*dependencies)[i].min_version.minor = sqlite3_column_int(statement, 5);
            (*dependencies)[i].min_version.patch = sqlite3_column_int(statement,6);
            (*dependencies)[i].max_version.major = sqlite3_column_int(statement, 7);
            (*dependencies)[i].max_version.minor = sqlite3_column_int(statement, 8);
            (*dependencies)[i].max_version.patch = sqlite3_column_int(statement,9);
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
