#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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

void KPM_FreeArtifactDependency(struct ArtifactDependency* dependency)
{
    free(dependency->artifact_repository);
    free(dependency->artifact_id);
    free(dependency->artifact_url);
    free(dependency->repository);
    free(dependency->id);

    dependency->repository = NULL;
    dependency->artifact_repository = NULL;
    dependency->artifact_id = NULL;
    dependency->artifact_url = NULL;
    dependency->id = NULL;
}

void KPM_FreeArtifactDependencyList(size_t dependencyCount, struct ArtifactDependency* dependencies)
{
    for (size_t i=0; i < dependencyCount; i++)
    {
        KPM_FreeArtifactDependency(dependencies + i);
    }
}

enum KPMResult KPM_GetArtifact(struct KPM* kpm, const char* repositoryId, const char* packageId, struct SemVer version, struct IndexedArtifact* artifact)
{   
    const char* zSQL;
    zSQL = "SELECT url, repository, id, version_major, version_minor, version_patch FROM artifacts WHERE repository=? AND id=? AND version_major=? AND version_minor=? AND version_patch=?;";


     
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, repositoryId, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, packageId, -1, SQLITE_STATIC);
    sqlite3_bind_int(statement, 3, version.major);
    sqlite3_bind_int(statement, 4, version.minor);
    sqlite3_bind_int(statement, 5, version.patch);

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        artifact->url = strdup((const char*) sqlite3_column_text(statement, 0));
        artifact->repository = strdup((const char*) sqlite3_column_text(statement, 1));
        artifact->id = strdup((const char*) sqlite3_column_text(statement, 2));
        artifact->version.major = sqlite3_column_int(statement, 3);
        artifact->version.minor = sqlite3_column_int(statement, 4);
        artifact->version.patch = sqlite3_column_int(statement, 5);
    } else {
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

enum KPMResult KPM_ListArtifactDependencies(struct KPM* kpm, char* repository, char* id, char* url, size_t* dependencyCount, struct ArtifactDependency** dependencies)
{
    *dependencyCount = 0;
    if (dependencies != NULL)
    {
        *dependencies = NULL;
    }
    
    const char* zSQL = "SELECT (SELECT COUNT() FROM artifact_dependencies WHERE artifact_repository=? AND artifact_id=? AND artifact_url=?), artifact_repository, artifact_id, artifact_url, repository, id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch FROM artifact_dependencies WHERE artifact_repository=? AND artifact_id=? AND artifact_url=?;";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, repository, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, url, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, repository, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 6, url, -1, SQLITE_STATIC);

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

            (*dependencies)[i].artifact_repository = strdup((const char*) sqlite3_column_text(statement, 1));
            (*dependencies)[i].artifact_id = strdup((const char*) sqlite3_column_text(statement, 2));
            (*dependencies)[i].artifact_url = strdup((const char*) sqlite3_column_text(statement, 3));
            (*dependencies)[i].repository = strdup((const char*) sqlite3_column_text(statement, 4));
            (*dependencies)[i].id = strdup((const char*) sqlite3_column_text(statement, 5));
            (*dependencies)[i].min_version.major = sqlite3_column_int(statement, 6);
            (*dependencies)[i].min_version.minor = sqlite3_column_int(statement, 7);
            (*dependencies)[i].min_version.patch = sqlite3_column_int(statement,8);
            (*dependencies)[i].max_version.major = sqlite3_column_int(statement, 9);
            (*dependencies)[i].max_version.minor = sqlite3_column_int(statement, 10);
            (*dependencies)[i].max_version.patch = sqlite3_column_int(statement,11);
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
