#include "cjson/cJSON.h"
#include "curl/curl.h"
#include "kpm/kpm.h"
#include "kpm/kpmio.h"
#include "kpm/semver.h"
#include "simpleGET.h"
#include <assert.h>
#include <limits.h>
#include <string.h>
#include "callback.h"
#include "sqlite3.h"

bool index_dependency(KPM *kpm, uint32_t repository_version, char *dependency_id, char *artifact_repository, char *artifact_id, char *artifact_url, cJSON *dependency, KPMIO *kpmio)
{
    assert(repository_version < 3 || dependency_id != NULL);
    char *actual_dependency_id = dependency_id;
    if (actual_dependency_id == NULL)
        actual_dependency_id = cJSON_GetStringValue(cJSON_GetObjectItem(dependency, "id"));
    
    if (actual_dependency_id == NULL || strlen(actual_dependency_id) < KPM_MIN_ID_LEN)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Dependency ID too short");
        return false;
    }
    kpmio->log(KPM_VERBOSITY_DEBUG, "    - D: %s", actual_dependency_id);

    if (!strcmp(actual_dependency_id, artifact_id))
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Dependency cannot have same ID as dependent package");
        return false;
    }
    
    const char *zSQL = "INSERT INTO artifact_dependencies (artifact_repository, artifact_id, artifact_url, id, min_version_major, min_version_minor, min_version_patch, max_version_major, max_version_minor, max_version_patch) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, artifact_repository, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, artifact_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, artifact_url, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, actual_dependency_id, -1, SQLITE_STATIC);
    
    if (cJSON_GetObjectItem(dependency, "min") != NULL)
    {
        if (
            cJSON_GetArraySize(cJSON_GetObjectItem(dependency, "min")) != 3 ||
            !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 0)) ||
            !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 1)) ||
            !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 2)) ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 0)) < 0 ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 1)) < 0 ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 2)) < 0 ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 0)) > SEMVER_VERSION_MAX ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 1)) > SEMVER_VERSION_MAX ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 2)) > SEMVER_VERSION_MAX
        )
        {
            sqlite3_finalize(statement);
            return false;
        }

        sqlite3_bind_int64(statement, 5, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 0)));
        sqlite3_bind_int64(statement, 6, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 1)));
        sqlite3_bind_int64(statement, 7, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 2)));
    }
    else
    {
        sqlite3_bind_int64(statement, 5, 0);
        sqlite3_bind_int64(statement, 6, 0);
        sqlite3_bind_int64(statement, 7, 0);
    }

    if (cJSON_GetObjectItem(dependency, "max") != NULL)
    {
        if (
            cJSON_GetArraySize(cJSON_GetObjectItem(dependency, "max")) != 3 ||
            !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 0)) ||
            !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 1)) ||
            !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 2)) ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 0)) < 0 ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 1)) < 0 ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 2)) < 0 ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 0)) > SEMVER_VERSION_MAX ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 1)) > SEMVER_VERSION_MAX ||
            cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 2)) > SEMVER_VERSION_MAX
        )
        {
            sqlite3_finalize(statement);
            return false;
        }

        sqlite3_bind_int64(statement, 8, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 0)));
        sqlite3_bind_int64(statement, 9, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 1)));
        sqlite3_bind_int64(statement, 10, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 2)));
    }
    else
    {
        sqlite3_bind_int64(statement, 8, SEMVER_VERSION_MAX);
        sqlite3_bind_int64(statement, 9, SEMVER_VERSION_MAX);
        sqlite3_bind_int64(statement, 10, SEMVER_VERSION_MAX);
    }

    if (
        cJSON_GetObjectItem(dependency, "min") != NULL &&
        cJSON_GetObjectItem(dependency, "max") != NULL &&
        SemVerCmp(
            (SemVer) {
                .major = cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 0)),
                .minor = cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 1)),
                .patch = cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "min"), 2))
            },
            (SemVer) {
                .major = cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 0)),
                .minor = cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 1)),
                .patch = cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(dependency, "max"), 2))
            }
        ) >= 0
    )
    {
        sqlite3_finalize(statement);
        return false;
    }

    int status;
    if ((status = sqlite3_step(statement)) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        kpmio->log(KPM_VERBOSITY_ERROR, "SQLite error: (%i: %s - %s)", status, sqlite3_errstr(status), sqlite3_errmsg(kpm->db));
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}

bool index_artifact(KPM *kpm, uint32_t repository_version, char *url, char *repository_id, char *package_id, cJSON *artifact, KPMIO *kpmio)
{
    assert(repository_version < 3 || url != NULL);
    char *artifact_url = url;
    if (artifact_url == NULL)
        artifact_url = cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "url"));
    kpmio->log(KPM_VERBOSITY_DEBUG, "  - A: %s", artifact_url);

    if (artifact_url == NULL || strlen(artifact_url) == 0)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Artifact has an empty URL field");
        return false;
    }

    if (!cJSON_IsArray(cJSON_GetObjectItem(artifact, "version")) || cJSON_GetArraySize(cJSON_GetObjectItem(artifact, "version")) != 3)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Artifact version field is invalid");
        return false;
    }

    if (
        !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 0)) ||
        !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 1)) ||
        !cJSON_IsNumber(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 2)) ||
        cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 0)) < 0 ||
        cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 1)) < 0 ||
        cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 2)) < 0 ||
        cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 0)) > SEMVER_VERSION_MAX ||
        cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 1)) > SEMVER_VERSION_MAX ||
        cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 2)) > SEMVER_VERSION_MAX
    )
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Artifact version field contains invalid entries");
        return false;
    }

    if (repository_version >= 3 && (cJSON_GetObjectItem(artifact, "installed_size") == NULL || cJSON_GetObjectItem(artifact, "digest") == NULL))
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Artifact v3 must contain both installed_size and digest");
        return false;
    }

    if (repository_version < 3 && (cJSON_GetObjectItem(artifact, "installed_size") != NULL || cJSON_GetObjectItem(artifact, "digest") != NULL))
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Artifact v2 must not contain either installed_size or digest");
        return false;
    }

    if (cJSON_GetObjectItem(artifact, "digest") != NULL && !cJSON_IsString(cJSON_GetObjectItem(artifact, "digest")))
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Artifact digest must be a string");
        return false;
    }

    if (cJSON_GetObjectItem(artifact, "installed_size") != NULL && !cJSON_IsNumber(cJSON_GetObjectItem(artifact, "installed_size")))
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Artifact installed_size must be a number");
        return false;
    }

    kpmio->log(KPM_VERBOSITY_DEBUG, "  indexing artifact %s (%.0f.%.0f.%.0f)", package_id, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 0)), cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 1)), cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 2)));
    const char *zSQL = "INSERT INTO artifacts (repository, id, url, installed_size, digest, version_major, version_minor, version_patch) VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, repository_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, package_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, artifact_url, -1, SQLITE_STATIC);
    
    if (cJSON_GetObjectItem(artifact, "installed_size"))
        sqlite3_bind_int64(statement, 4, cJSON_GetNumberValue(cJSON_GetObjectItem(artifact, "installed_size")));
    else
        sqlite3_bind_null(statement, 4);

    if (cJSON_GetObjectItem(artifact, "digest"))
        sqlite3_bind_text(statement, 5, cJSON_GetStringValue(cJSON_GetObjectItem(artifact, "digest")), -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(statement, 5);

    sqlite3_bind_int64(statement, 6, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 0)));
    sqlite3_bind_int64(statement, 7, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 1)));
    sqlite3_bind_int64(statement, 8, cJSON_GetNumberValue(cJSON_GetArrayItem(cJSON_GetObjectItem(artifact, "version"), 2)));

    int status;
    if ((status = sqlite3_step(statement)) != SQLITE_DONE)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "SQLite3 error (%i: %s - %s)", status, sqlite3_errstr(status), sqlite3_errmsg(kpm->db));
        sqlite3_finalize(statement);
        return false;
    }
    sqlite3_finalize(statement);

    if (
        (repository_version >= 3 && (
                cJSON_GetObjectItem(artifact, "dependencies") == NULL ||
                !cJSON_IsObject(cJSON_GetObjectItem(artifact, "dependencies"))
            )
        ) ||
        (repository_version < 3 && cJSON_GetObjectItem(artifact, "dependencies") != NULL && !cJSON_IsArray(cJSON_GetObjectItem(artifact, "dependencies")))
    )
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Invalid dependencies in artifact");
        return false;
    }

    // Index this artifact's dependencies
    cJSON *dependency;
    cJSON_ArrayForEach(dependency, cJSON_GetObjectItem(artifact, "dependencies"))
    {
        if (!index_dependency(kpm, repository_version, dependency->string, repository_id, package_id, artifact_url, dependency, kpmio))
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "Could not index artifact dependencies for [%s] (%s)", package_id, artifact_url);
            return false;
        }
    }

    return true;
}

bool index_package(KPM *kpm, uint32_t repository_version, char *repository_id, cJSON *package, KPMIO *kpmio)
{
    sqlite3_exec(kpm->db, "SAVEPOINT package", NULL, NULL, NULL);
    // Ensure the package is well-formed
    if (cJSON_GetObjectItem(package, "name") == NULL ||
        cJSON_GetObjectItem(package, "author") == NULL ||
        cJSON_GetObjectItem(package, "description") == NULL ||
        cJSON_GetObjectItem(package, "artifacts") == NULL ||
        cJSON_GetArraySize(cJSON_GetObjectItem(package, "artifacts")) == 0 ||
        (
            repository_version >= 3 && (
                !cJSON_IsObject(cJSON_GetObjectItem(package, "artifacts")) ||
                cJSON_GetObjectItem(package, "category") == NULL ||
                !cJSON_IsString(cJSON_GetObjectItem(package, "category")) ||
                strlen(cJSON_GetStringValue(cJSON_GetObjectItem(package, "category"))) == 0 ||
                (cJSON_GetObjectItem(package, "subcategory") != NULL && (
                        !cJSON_IsString(cJSON_GetObjectItem(package, "subcategory")) ||
                        strlen(cJSON_GetStringValue(cJSON_GetObjectItem(package, "subcategory"))) == 0
                    )
                )
            )
        ) ||
        (
            repository_version < 3 && (
                !cJSON_IsArray(cJSON_GetObjectItem(package, "artifacts")) ||
                cJSON_GetObjectItem(package, "category") != NULL ||
                cJSON_GetObjectItem(package, "subcategory") != NULL
            )
        ) ||
        (
            cJSON_GetObjectItem(package, "icon") &&
            !cJSON_IsNull(cJSON_GetObjectItem(package, "icon")) &&
            (
                !cJSON_IsString(cJSON_GetObjectItem(package, "icon")) ||
                strlen(cJSON_GetStringValue(cJSON_GetObjectItem(package, "icon"))) < 1
            )
        ) ||
        (
            cJSON_GetObjectItem(package, "changelog") &&
            !cJSON_IsNull(cJSON_GetObjectItem(package, "changelog")) &&
            (
                !cJSON_IsString(cJSON_GetObjectItem(package, "changelog")) ||
                strlen(cJSON_GetStringValue(cJSON_GetObjectItem(package, "changelog"))) < 1
            )
        )
    )
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Could not parse package %s", package->string);
        sqlite3_exec(kpm->db, "ROLLBACK TRANSACTION TO SAVEPOINT package", NULL, NULL, NULL);
        return false;
    }

    if (package->string == NULL || strlen(package->string) < KPM_MIN_ID_LEN)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Package ID too short");
        sqlite3_exec(kpm->db, "ROLLBACK TRANSACTION TO SAVEPOINT package", NULL, NULL, NULL);
        return false;
    }

    kpmio->log(KPM_VERBOSITY_DEBUG, "indexing package [%s]", package->string);

    char *category = "miscellaneous";
    if (cJSON_GetObjectItem(package, "category") != NULL)
        category = cJSON_GetStringValue(cJSON_GetObjectItem(package, "category"));
    char *subcategory = NULL;
    if (cJSON_GetObjectItem(package, "subcategory") != NULL)
        subcategory = cJSON_GetStringValue(cJSON_GetObjectItem(package, "subcategory"));

    // INSERT that package INTO the DATABASE (deltarune reference???)
    const char *zSQL = "INSERT INTO packages (repository, id, name, author, description, category, subcategory, icon, changelog) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, repository_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 2, package->string, -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 3, cJSON_GetStringValue(cJSON_GetObjectItem(package, "name")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 4, cJSON_GetStringValue(cJSON_GetObjectItem(package, "author")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 5, cJSON_GetStringValue(cJSON_GetObjectItem(package, "description")), -1, SQLITE_STATIC);
    sqlite3_bind_text(statement, 6, category, -1, SQLITE_STATIC);    

    if (subcategory)
        sqlite3_bind_text(statement, 7, subcategory, -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(statement, 7);

    if (cJSON_GetObjectItem(package, "icon"))
        sqlite3_bind_text(statement, 8, cJSON_GetStringValue(cJSON_GetObjectItem(package, "icon")), -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(statement, 8);

    if (cJSON_GetObjectItem(package, "changelog"))
        sqlite3_bind_text(statement, 9, cJSON_GetStringValue(cJSON_GetObjectItem(package, "changelog")), -1, SQLITE_STATIC);
    else
        sqlite3_bind_null(statement, 9);

    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "SQLite error encountered when indexing package %s", package->string);
        sqlite3_finalize(statement);
        sqlite3_exec(kpm->db, "ROLLBACK TRANSACTION TO SAVEPOINT package", NULL, NULL, NULL);
        return false;
    }
    sqlite3_finalize(statement);



    // Now we index artifacts
    bool has_any_artifact=false;
    cJSON *artifact;
    cJSON_ArrayForEach(artifact, cJSON_GetObjectItem(package, "artifacts"))
    {
        // Check this artifact's supported_platforms
        if (!cJSON_IsArray(cJSON_GetObjectItem(artifact, "supported_platforms")))
            continue;
        
        bool supported=false;
        cJSON *supported_platform;
        cJSON_ArrayForEach(supported_platform, cJSON_GetObjectItem(artifact, "supported_platforms"))
        {
            if (strcmp(supported_platform->valuestring, KPM_PLATFORM) == 0)
            {
                supported = true;
                break;
            }
        }

        if (!supported)
            continue;

        if (!index_artifact(kpm, repository_version, artifact->string, repository_id, package->string, artifact, kpmio))
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "Could not index artifact for [%s]", package->string);
            sqlite3_exec(kpm->db, "ROLLBACK TRANSACTION TO SAVEPOINT package", NULL, NULL, NULL);
            return false; // Stop adding packages if one fails
        }
        has_any_artifact = true;
    }

    if (has_any_artifact)
        sqlite3_exec(kpm->db, "RELEASE SAVEPOINT package", NULL, NULL, NULL);
    else
        sqlite3_exec(kpm->db, "ROLLBACK TRANSACTION TO SAVEPOINT package", NULL, NULL, NULL);

    return true;
}

KPM_Result KPM_UpdateIndex(KPM *kpm, KPMIO *kpmio)
{
    kpmio->log(KPM_VERBOSITY_INFO, "Getting repositories...");

    size_t repository_count;
    KPM_Repository *repositories;
    KPM_Result result;
    if ((result = KPM_ListRepositories(kpm, &repository_count, &repositories)) != KPM_OK)
    {
        kpmio->log(KPM_VERBOSITY_ERROR, "Unable to list KPM repositories");
        if (result == KPM_SQLITE_ERROR)
            kpmio->log(KPM_VERBOSITY_ERROR, "SQLite error: %s", sqlite3_errmsg(kpm->db));
        return result;
    }

    bool all_ok = true;
    SimpleGETRequest *request;
    for (size_t i=0; i < repository_count; i++)
    {
        kpmio->log(KPM_VERBOSITY_INFO, "Downloading index [%s]", repositories[i].url);
        request = SimpleGET_Initialise(repositories[i].url);
        CURLcode curl_code = SimpleGET_Perform(request);
        if (curl_code != CURLE_OK)
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "Could not fetch url [%s] - CURL Error: %i - %s", repositories[i].url, curl_code, curl_easy_strerror(curl_code));
            SimpleGET_Cleanup(request);
            all_ok = false;
            continue;
        }

        kpmio->log(KPM_VERBOSITY_DEBUG, "Got response code: %i", request->response_code);
        if (request->response_code >= 400 && strncmp(repositories[i].url, "file://", strlen("file://")) != 0)
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "Could not fetch url [%s]", repositories[i].url);
            SimpleGET_Cleanup(request);
            all_ok = false;
            continue;
        }

        kpmio->log(KPM_VERBOSITY_DEBUG, "Got manifest: %s", request->buffer);
        cJSON *json = cJSON_Parse((char*) request->buffer);
        if (json == NULL)
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "Could not parse manifest");
            SimpleGET_Cleanup(request);
            cJSON_Delete(json);
            all_ok = false;
            continue;
        }

        if (
            cJSON_GetObjectItem(json, "manifest_version") == NULL ||
            !cJSON_IsNumber(cJSON_GetObjectItem(json, "manifest_version")) ||
            cJSON_GetNumberValue(cJSON_GetObjectItem(json, "manifest_version")) < 1 ||
            cJSON_GetNumberValue(cJSON_GetObjectItem(json, "manifest_version")) > KPM_MANIFEST_VERSION
        )
        {
            kpmio->log(KPM_VERBOSITY_ERROR, "Invalid manifest version, got %.0f, expected %i", cJSON_GetNumberValue(cJSON_GetObjectItem(json, "manifest_version")), KPM_MANIFEST_VERSION);
            kpmio->log(KPM_VERBOSITY_ERROR, "You may need to update KPM, run: kpm install kpm or kpm upgrade");
            SimpleGET_Cleanup(request);
            cJSON_Delete(json);
            all_ok = false;
            continue;
        }

        uint32_t repository_version = cJSON_GetNumberValue(cJSON_GetObjectItem(json, "manifest_version"));

        // V1 - Requires no special logic, only difference is packages are lzma compressed, not gzip

        // Start a transaction for every repository
        sqlite3_exec(kpm->db, "BEGIN", NULL, NULL, NULL); // "But Chudda what if?" IT WONT FAIL

        // Clear indexed packages
        kpmio->log(KPM_VERBOSITY_DEBUG, "Clearing index");
        const char *zSQL = "DELETE FROM packages WHERE repository=?;";
        sqlite3_stmt *statement;
        sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, repositories[i].id, -1, SQLITE_STATIC);
        
        int status = SQLITE_ROW;
        status = sqlite3_step(statement);
        sqlite3_finalize(statement);
        if (status != SQLITE_DONE)
        {
            KPM_FreeRepositoryList(repository_count, repositories);
            kpmio->log(KPM_VERBOSITY_ERROR, "Could not clear packages for %s (%i: %s - %s)", repositories[i].id, status, sqlite3_errstr(status), sqlite3_errmsg(kpm->db));
            cJSON_Delete(json);
            SimpleGET_Cleanup(request);
            sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
            all_ok = false;
            continue;
        }

        // Index packages & artifacts
        // We COULD give progress status here... but nah - HD
        bool any_ok = false;
        cJSON *package;
        cJSON_ArrayForEach(package, cJSON_GetObjectItem(json, "packages"))
        {
            if (index_package(kpm, repository_version, repositories[i].id, package, kpmio))
                any_ok = true;
            else
                all_ok = false;
        }

        if (any_ok)
        {
            sqlite3_exec(kpm->db, "COMMIT", NULL, NULL, NULL);
        }
        else
        {
            all_ok = false;
            sqlite3_exec(kpm->db, "ROLLBACK", NULL, NULL, NULL);
        }
        
        cJSON_Delete(json);
        SimpleGET_Cleanup(request);
    }

    KPM_FreeRepositoryList(repository_count, repositories);

    if (all_ok)
        return KPM_OK;
    else
        return KPM_GENERIC_ERROR;
}
