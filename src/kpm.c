#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "kpm/kpm.h"
#include "internal_utils.h"
#include "sqlite3.h"

const char *KPM_ErrorToString(KPM_Result error)
{
    switch (error)
    {
    case KPM_OK:
        return "Ok";
    case KPM_ABORTED:
        return "Aborted";
    case KPM_GENERIC_ERROR:
        return "Generic Error";
    case KPM_SQLITE_ERROR:
        return "SQLite Error";
    case KPM_CURL_ERROR:
        return "cURL error";
    case KPM_INVALID_RESPONSE_CODE:
        return "Invalid response code";
    case KPM_INVALID_RESPONSE_CONTENT:
        return "Invalid response content";
    case KPM_FILE_SYSTEM_ERROR:
        return "File system error";
    case KPM_LIBARCHIVE_ERROR:
        return "Libarchive error";
    case KPM_PARSE_ERROR:
        return "Parse error";
    case KPM_RESOLUTION_ERROR:
        return "Resolution error";
    }
};

void create_tables(sqlite3 *db)
{
    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS kpm_meta (
            key TEXT PRIMARY KEY NOT NULL,
            value TEXT
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS repositories (
            id TEXT PRIMARY KEY NOT NULL,
            url TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT NOT NULL
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS packages (
            repository TEXT NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
            id TEXT NOT NULL,
            name TEXT NOT NULL,
            author TEXT NOT NULL,
            description TEXT NOT NULL,
            icon TEXT,
            PRIMARY KEY (repository, id)
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS artifacts (
            repository TEXT NOT NULL,
            id TEXT NOT NULL,
            url TEXT NOT NULL,
            installed_size INTEGER,
            digest TEXT,
            version_major INTEGER NOT NULL,
            version_minor INTEGER NOT NULL,
            version_patch INTEGER NOT NULL,
            PRIMARY KEY (repository, id, url),
            UNIQUE (repository, id, version_major, version_minor, version_patch),
            FOREIGN KEY (repository, id) REFERENCES packages(repository, id) ON DELETE CASCADE
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS artifact_dependencies (
            artifact_repository TEXT NOT NULL,
            artifact_id TEXT NOT NULL,
            artifact_url TEXT NOT NULL,
            id TEXT NOT NULL,
            min_version_major INTEGER NOT NULL,
            min_version_minor INTEGER NOT NULL,
            min_version_patch INTEGER NOT NULL,
            max_version_major INTEGER NOT NULL,
            max_version_minor INTEGER NOT NULL,
            max_version_patch INTEGER NOT NULL,
            PRIMARY KEY (artifact_repository, artifact_id, artifact_url, id),
            FOREIGN KEY (artifact_repository, artifact_id, artifact_url) REFERENCES artifacts(repository, id, url) ON DELETE CASCADE
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS installed_packages (
            id TEXT PRIMARY KEY NOT NULL,
            repository TEXT,
            name TEXT NOT NULL,
            author TEXT NOT NULL,
            description TEXT NOT NULL,
            installed_size INTEGER NOT NULL,
            version_major INTEGER NOT NULL,
            version_minor INTEGER NOT NULL,
            version_patch INTEGER NOT NULL,
            installed_as_dependency INTEGER NOT NULL
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS current_dependencies (
            dependent TEXT NOT NULL REFERENCES installed_packages(id) ON DELETE CASCADE,
            dependency_id TEXT NOT NULL REFERENCES installed_packages(id) ON DELETE CASCADE,
            min_version_major INTEGER NOT NULL,
            min_version_minor INTEGER NOT NULL,
            min_version_patch INTEGER NOT NULL,
            max_version_major INTEGER NOT NULL,
            max_version_minor INTEGER NOT NULL,
            max_version_patch INTEGER NOT NULL
        ) STRICT;
    )", NULL, NULL, NULL);
}

void KPM_MigrateDB(sqlite3 *db)
{
    int db_version;

    sqlite3_stmt *statement;
    sqlite3_prepare_v2(db, "SELECT value FROM kpm_meta WHERE key='schema_version';", -1, &statement, NULL);
    if (sqlite3_step(statement) == SQLITE_ROW)
        db_version = atoi((const char*) sqlite3_column_text(statement, 0));
    else
        db_version = 0;
    sqlite3_finalize(statement);

    switch (db_version)
    {
    case 0:
        // Allow installed packages with no repository
        sqlite3_exec(db, "ALTER TABLE installed_packages ALTER COLUMN repository TEXT;", NULL, NULL, NULL);
    case 1:
        sqlite3_exec(db, "ALTER TABLE artifacts ADD COLUMN installed_size INTEGER;", NULL, NULL, NULL); // For KPKG3+ the installed size of the artifact
        sqlite3_exec(db, "ALTER TABLE artifacts ADD COLUMN digest TEXT;", NULL, NULL, NULL); // For KPKG3+ the digest of the package header
        sqlite3_exec(db, "ALTER TABLE packages ADD COLUMN icon TEXT;", NULL, NULL, NULL); // For KPKG3+ the package icon

        // Update constraints
        sqlite3_exec(db, "PRAGMA foreign_keys=off;", NULL, NULL, NULL);
        sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
        sqlite3_exec(db, "ALTER TABLE kpm_meta RENAME TO old_kpm_meta;", NULL, NULL, NULL);
        sqlite3_exec(db, "ALTER TABLE repositories RENAME TO old_repositories;", NULL, NULL, NULL);
        sqlite3_exec(db, "ALTER TABLE packages RENAME TO old_packages;", NULL, NULL, NULL);
        sqlite3_exec(db, "ALTER TABLE artifacts RENAME TO old_artifacts;", NULL, NULL, NULL);
        sqlite3_exec(db, "ALTER TABLE artifact_dependencies RENAME TO old_artifact_dependencies;", NULL, NULL, NULL);
        sqlite3_exec(db, "ALTER TABLE installed_packages RENAME TO old_installed_packages;", NULL, NULL, NULL);
        sqlite3_exec(db, "ALTER TABLE current_dependencies RENAME TO old_current_dependencies;", NULL, NULL, NULL);
        create_tables(db);
        sqlite3_exec(db, "INSERT INTO kpm_meta SELECT * FROM old_kpm_meta;", NULL, NULL, NULL);
        sqlite3_exec(db, "INSERT INTO repositories SELECT * FROM old_repositories;", NULL, NULL, NULL);
        sqlite3_exec(db, "INSERT INTO packages SELECT * FROM old_packages;", NULL, NULL, NULL);
        sqlite3_exec(db, "INSERT INTO artifacts SELECT * FROM old_artifacts;", NULL, NULL, NULL);
        sqlite3_exec(db, "INSERT INTO artifact_dependencies SELECT * FROM old_artifact_dependencies;", NULL, NULL, NULL);
        sqlite3_exec(db, "INSERT INTO installed_packages SELECT * FROM old_installed_packages;", NULL, NULL, NULL);
        sqlite3_exec(db, "INSERT INTO current_dependencies SELECT * FROM old_current_dependencies;", NULL, NULL, NULL);
        sqlite3_exec(db, "DROP TABLE old_kpm_meta;", NULL, NULL, NULL);
        sqlite3_exec(db, "DROP TABLE old_repositories;", NULL, NULL, NULL);
        sqlite3_exec(db, "DROP TABLE old_packages;", NULL, NULL, NULL);
        sqlite3_exec(db, "DROP TABLE old_artifacts;", NULL, NULL, NULL);
        sqlite3_exec(db, "DROP TABLE old_artifact_dependencies;", NULL, NULL, NULL);
        sqlite3_exec(db, "DROP TABLE old_installed_packages;", NULL, NULL, NULL);
        sqlite3_exec(db, "DROP TABLE old_current_dependencies;", NULL, NULL, NULL);
        sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
        sqlite3_exec(db, "PRAGMA foreign_keys=on;", NULL, NULL, NULL);
    default:
        break;
    };

    db_version = 2;
    sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO kpm_meta (key, value) VALUES ('schema_version', ?);", -1, &statement, NULL);
    char *db_version_str = asprintf_hd("%i", db_version);
    sqlite3_bind_text(statement, 1, db_version_str, -1, SQLITE_STATIC);
    sqlite3_step(statement);
    sqlite3_finalize(statement);
    free(db_version_str);
}

KPM *KPM_InitialiseAdv(char *db_path, char *pkg_path, char *tmp_path)
{
    KPM *kpm = malloc(sizeof(KPM));
    kpm->max_connections = 5;

    if (db_path)
        kpm->db_path = strdup(db_path);
    else
        kpm->db_path = asprintf_hd("%s/kpm.db", KPM_PATH);

    if (db_path)
        kpm->pkg_path = strdup(pkg_path);
    else
        kpm->pkg_path = asprintf_hd("%s/packages", KPM_PATH);

    if (tmp_path)
        kpm->tmp_path = strdup(tmp_path);
    else
        kpm->tmp_path = asprintf_hd("%s/tmp", KPM_PATH);


    sqlite3_open(kpm->db_path, &kpm->db);
    sqlite3_exec(kpm->db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL); // @TODO: Ensure this succeeds

    create_tables(kpm->db);

    KPM_MigrateDB(kpm->db);

    sqlite3_exec(kpm->db, "INSERT INTO repositories (id, url, name, description) VALUES ('kindlemodding', 'https://repo.kindlemodding.org/manifest.v3.json', 'Official KMC Repo', 'The official KMC repo') ON CONFLICT(id) DO UPDATE SET url='https://repo.kindlemodding.org/manifest.v3.json'", NULL, NULL, NULL);

    char *query = asprintf_hd("INSERT OR REPLACE INTO installed_packages (id, repository, name, author, description, version_major, version_minor, version_patch, installed_as_dependency) VALUES ('kpm', 'kindlemodding', 'KPM', 'Hackerdude', 'The Kindle Package Manager allows you to install and uninstall apps and programs easily on your Kindle', %i, %i, %i, 0)", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH);
    sqlite3_exec(kpm->db, query, NULL, NULL, NULL);
    free(query);

    return kpm;
}

/**
 * @brief Initialise the KPM object
 * 
 * @return KPM* 
 */
KPM *KPM_Initialise()
{
    return KPM_InitialiseAdv(NULL, NULL, NULL);
}

void KPM_Cleanup(KPM *kpm)
{
    sqlite3_close(kpm->db);
    kpm->db = NULL;
    free(kpm->db_path);
    free(kpm->pkg_path);
    free(kpm->tmp_path);
    free(kpm);
}