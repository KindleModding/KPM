#include "kpm/kpm.hpp"

KPM::KPM::KPM(const std::filesystem::path& dbPath)
{
    sqlite3_open(dbPath.c_str(), &db);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS repositories (
            id TEXT PRIMARY KEY NOT NULL,
            url TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT NOT NULL,
            FOREIGN KEY(id) REFERENCES repositories(id)
        )
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS packages (
            repository TEXT NOT NULL,
            id TEXT NOT NULL,
            name TEXT NOT NULL,
            description TEXT NOT NULL,
            author TEXT NOT NULL,
            icon TEXT NOT NULL,
            PRIMARY KEY (repository, id),
            FOREIGN KEY(repository) REFERENCES repositories(id)
        )
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS artifacts (
            url TEXT PRIMARY KEY NOT NULL,
            repository TEXT NOT NULL,
            id TEXT NOT NULL,
            version_major INTEGER NOT NULL,
            version_minor INTEGER NOT NULL,
            version_patch INTEGER NOT NULL,
            supported_arch TEXT NOT NULL,
            supported_kindles TEXT NOT NULL,
            FOREIGN KEY (repository, id) REFERENCES packages(repository, id)
        )
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS artifact_dependencies (
            artifact TEXT NOT NULL,
            repository TEXT,
            id TEXT NOT NULL,
            type INTEGER NOT NULL,
            version_major INTEGER,
            version_minor INTEGER,
            version_patch INTEGER,
            PRIMARY KEY (artifact, repository, id),
            FOREIGN KEY(artifact) REFERENCES artifacts(url)
        )
    )", NULL, NULL, NULL);

    sqlite3_exec(db, R"(
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

    sqlite3_exec(db, R"(
        CREATE TABLE IF NOT EXISTS dependencies (
            dependent TEXT NOT NULL,
            dependency_repository TEXT NOT NULL,
            dependency_id TEXT NOT NULL,
            dependency_type TEXT NOT NULL,
            version_major INTEGER NOT NULL,
            version_minor INTEGER NOT NULL,
            version_patch INTEGER NOT NULL,
            FOREIGN KEY(dependent) REFERENCES installed_packages(id)
        )
    )", NULL, NULL, NULL);
};

KPM::KPM::~KPM()
{
    sqlite3_close(db);
}