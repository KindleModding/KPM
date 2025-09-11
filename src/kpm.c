#include <stddef.h>
#include <stdio.h>

#include "kpm/kpm.h"

/**
 * @brief Initialise the KPM object
 * 
 * @param kpm A pointer to an uninitialised KPM struct
 * @param dbPath The path to the KPM database file
 * @return enum KPMResult 
 */
enum KPMResult KPM_Initialise(struct KPM *kpm, const char* dbPath)
{
    sqlite3_open(dbPath, &kpm->db);

    sqlite3_exec(kpm->db, R"(
        CREATE TABLE IF NOT EXISTS repositories (
            id TEXT PRIMARY KEY NOT NULL,
            url TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT NOT NULL
        )
    )", NULL, NULL, NULL);

    sqlite3_exec(kpm->db, R"(
        CREATE TABLE IF NOT EXISTS packages (
            repository TEXT NOT NULL,
            id TEXT NOT NULL,
            name TEXT NOT NULL,
            author TEXT NOT NULL,
            description TEXT NOT NULL,
            PRIMARY KEY (repository, id)
        )
    )", NULL, NULL, NULL);

    sqlite3_exec(kpm->db, R"(
        CREATE TABLE IF NOT EXISTS artifacts (
            url TEXT PRIMARY KEY NOT NULL,
            repository TEXT NOT NULL REFERENCES packages(repository) ON DELETE CASCADE,
            id TEXT NOT NULL REFERENCES packages(id) ON DELETE CASCADE,
            version_major INTEGER NOT NULL,
            version_minor INTEGER NOT NULL,
            version_patch INTEGER NOT NULL
        )
    )", NULL, NULL, NULL);

    sqlite3_exec(kpm->db, R"(
        CREATE TABLE IF NOT EXISTS artifact_dependencies (
            artifact TEXT NOT NULL REFERENCES artifacts(url) ON DELETE CASCADE,
            repository TEXT NOT NULL,
            id TEXT NOT NULL,
            min_version_major INTEGER,
            min_version_minor INTEGER,
            min_version_patch INTEGER,
            max_version_major INTEGER,
            max_version_minor INTEGER,
            max_version_patch INTEGER,
            PRIMARY KEY (artifact, repository, id)
        )
    )", NULL, NULL, NULL);

    sqlite3_exec(kpm->db, R"(
        CREATE TABLE IF NOT EXISTS installed_packages (
            id TEXT PRIMARY KEY NOT NULL,
            name TEXT NOT NULL,
            author TEXT NOT NULL,
            description TEXT NOT NULL,
            version_major INTEGER,
            version_minor INTEGER,
            version_patch INTEGER
        )
    )", NULL, NULL, NULL);

    sqlite3_exec(kpm->db, R"(
        CREATE TABLE IF NOT EXISTS current_dependencies (
            dependent TEXT NOT NULL REFERENCES installed_packages(id) ON DELETE CASCADE,
            dependency_repository TEXT NOT NULL,
            dependency_id TEXT NOT NULL,
            dependency_type TEXT NOT NULL,
            min_version_major INTEGER,
            min_version_minor INTEGER,
            min_version_patch INTEGER,
            max_version_major INTEGER,
            max_version_minor INTEGER,
            max_version_patch INTEGER
        )
    )", NULL, NULL, NULL);

    return KPM_OK;
}

/**
 * @brief Cleanup a KPM object
 * 
 * @param kpm 
 */
void KPM_Cleanup(struct KPM *kpm)
{
    sqlite3_close(kpm->db);
    return;
}