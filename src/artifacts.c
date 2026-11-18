#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void KPM_FreeArtifact(KPM_Artifact *artifact)
{
     free(artifact->repository);
     free(artifact->id);
     free(artifact->url);
     free(artifact->digest);
 
     artifact->repository = NULL;
     artifact->id = NULL;
     artifact->url = NULL;
     artifact->digest = NULL;
}
 
void KPM_FreeArtifactList(size_t artifact_count, KPM_Artifact *artifacts)
{
     for (size_t i=0; i < artifact_count; i++)
        KPM_FreeArtifact(&artifacts[i]);

     free(artifacts);
}

KPM_Result KPM_GetArtifact(KPM *kpm, const char *repository_id, const char *package_id, SemVer version, KPM_Artifact *artifact)
{
    const char *zSQL;
    sqlite3_stmt *statement;

    if (repository_id != NULL)
    {
        zSQL = "SELECT repository, id, url, installed_size, digest, version_major, version_minor, version_patch FROM artifacts WHERE repository=? AND id=? AND version_major=? AND version_minor=? AND version_patch=? LIMIT 1;";
        sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, repository_id, -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 2, package_id, -1, SQLITE_STATIC);
        sqlite3_bind_int64(statement, 3, version.major);
        sqlite3_bind_int64(statement, 4, version.minor);
        sqlite3_bind_int64(statement, 5, version.patch);
    }
    else
    {
        zSQL = "SELECT repository, id, url, installed_size, digest, version_major, version_minor, version_patch FROM artifacts WHERE id=? AND version_major=? AND version_minor=? AND version_patch=? LIMIT 1;";
        sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, package_id, -1, SQLITE_STATIC);
        sqlite3_bind_int64(statement, 2, version.major);
        sqlite3_bind_int64(statement, 3, version.minor);
        sqlite3_bind_int64(statement, 4, version.patch);
    }

    if (sqlite3_step(statement) == SQLITE_ROW)
    {
        artifact->repository = strdup((const char*) sqlite3_column_text(statement, 0));
        artifact->id = strdup((const char*) sqlite3_column_text(statement, 1));
        artifact->url = strdup((const char*) sqlite3_column_text(statement, 2));

        if (sqlite3_column_type(statement, 3) != SQLITE_NULL)
            artifact->installed_size = sqlite3_column_int64(statement, 3);
        else
            artifact->installed_size = 0;

        if (sqlite3_column_type(statement, 4) != SQLITE_NULL)
            artifact->digest = strdup((const char*) sqlite3_column_text(statement, 4));
        else
            artifact->digest = NULL;

        artifact->version.major = sqlite3_column_int(statement, 5);
        artifact->version.minor = sqlite3_column_int(statement, 6);
        artifact->version.patch = sqlite3_column_int(statement, 7);
    } else {
        sqlite3_finalize(statement);
        return KPM_SQLITE_ERROR;
    }

    sqlite3_finalize(statement);
    return KPM_OK;
}

KPM_Result KPM_ListArtifactDependencies(KPM *kpm, const char *repository, const char *id, const char *url, size_t *dependency_count, KPM_Dependency **dependencies)
{
    *dependency_count = 0;
    if (dependencies != NULL)
        *dependencies = NULL;
    
    const char *zSQL = "SELECT (SELECT COUNT() FROM artifact_dependencies WHERE artifact_repository=? AND artifact_id=? AND artifact_url=?), id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch FROM artifact_dependencies WHERE artifact_repository=? AND artifact_id=? AND artifact_url=?;";
    sqlite3_stmt *statement;
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
            *dependency_count = sqlite3_column_int64(statement, 0);
        
        if (dependencies != NULL && *dependency_count > 0)
        {
            if (!*dependencies)
                *dependencies = malloc(*dependency_count * sizeof(KPM_Dependency));
            
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
