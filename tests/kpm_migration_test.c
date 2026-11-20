#include "internal_utils.h"
#include "sqlite3.h"
#include <string.h>

int main(int argc, char *argv[])
{
    remove("./migration_test.db");
    sqlite3 *db;
    sqlite3_open("./migration_test.db", &db);
    sqlite3_exec(db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL); // @TODO: Ensure this succeeds

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
            PRIMARY KEY (repository, id)
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS artifacts (
            repository TEXT NOT NULL,
            id TEXT NOT NULL,
            url TEXT NOT NULL,
            version_major INTEGER NOT NULL,
            version_minor INTEGER NOT NULL,
            version_patch INTEGER NOT NULL,
            PRIMARY KEY (repository, id, url),
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

    sqlite3_exec(db, "INSERT INTO repositories VALUES ('testrepo', 'tbd', 'Test Repo', 'A Test Repo')", NULL, NULL, NULL);

    KPM_MigrateDB(db);
    KPM_MigrateDB(db);

    sqlite3_close(db);
}