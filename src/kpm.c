#include <stddef.h>
#include <stdlib.h>

#include "kpm/kpm.h"

enum KPMResult KPM_Initialise(struct KPM *kpm, const char* dbPath)
{
    sqlite3_open(dbPath, &kpm->db);
    sqlite3_exec(kpm->db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL); // @TODO: Ensure this succeeds

    sqlite3_exec(kpm->db, R"(
        CREATE TABLE IF NOT EXISTS repositories (
            id TEXT PRIMARY KEY NOT NULL,
            url TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT NOT NULL
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(kpm->db, R"(
        CREATE TABLE IF NOT EXISTS packages (
            repository TEXT NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
            id TEXT NOT NULL,
            name TEXT NOT NULL,
            author TEXT NOT NULL,
            description TEXT NOT NULL,
            PRIMARY KEY (repository, id)
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(kpm->db, R"(
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

    sqlite3_exec(kpm->db, R"(
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

    sqlite3_exec(kpm->db, R"(
        CREATE TABLE IF NOT EXISTS installed_packages (
            id TEXT PRIMARY KEY NOT NULL,
            repository TEXT NOT NULL,
            name TEXT NOT NULL,
            author TEXT NOT NULL,
            description TEXT NOT NULL,
            version_major INTEGER NOT NULL,
            version_minor INTEGER NOT NULL,
            version_patch INTEGER NOT NULL,
            installed_as_dependency INTEGER NOT NULL
        ) STRICT;
    )", NULL, NULL, NULL);

    sqlite3_exec(kpm->db, R"(
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

    sqlite3_exec(kpm->db, "INSERT OR REPLACE INTO repositories (id, url, name, description) VALUES ('org.kindlemodding.repo', 'https://repo.kindlemodding.org/manifest.json', 'Official KMC Repo', 'The official KMC repo')", NULL, NULL, NULL);

    return KPM_OK;
}

void KPM_Cleanup(struct KPM *kpm)
{
    sqlite3_close(kpm->db);
    kpm->db = NULL;
    return;
}