#include "database.h"
#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Statement.h"
#include <cstring>
#include <vector>

Database::Database(std::string path): db(path, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE) {
    // Check what tables the database has
    bool shouldCreateReposTable = true;
    bool shouldCreatePackageIndexTable = true;
    bool shouldCreatePackagesInstalledTable = true;

    SQLite::Statement query(db, "SELECT name FROM sqlite_schema WHERE type='table';");
    while (query.executeStep()) {
        if (query.getColumn(0).getString() == repoTableName) {
            shouldCreateReposTable = false;
        } else if (query.getColumn(0).getString() == packageIndexTableName) {
            shouldCreatePackageIndexTable = false;
        } else if (query.getColumn(0).getString() == packagesInstalledTableName) {
            shouldCreatePackagesInstalledTable = false;
        }
    }

    if (shouldCreateReposTable) {
        printf("Repos table not found - creating\n");
        SQLite::Statement query(db, "CREATE TABLE " + repoTableName + " ("
                                                "id TEXT NOT NULL PRIMARY KEY,"
                                                "uri TEXT NOT NULL"
                                                ") STRICT;");
        query.exec();

        // Add default repo
        // @TODO
    }

    if (shouldCreatePackageIndexTable) {
        printf("Package index not found - creating\n");
        SQLite::Statement query(db, "CREATE TABLE " + packageIndexTableName + " ("
                                                "id TEXT NOT NULL,"
                                                "repo_id TEXT NOT NULL,"
                                                "architecture TEXT NOT NULL,"
                                                "version_name TEXT NOT NULL,"
                                                "version_number INTEGER NOT NULL,"
                                                "min_firmware INTEGER NOT NULL,"
                                                "max_firmware INTEGER NOT NULL,"
                                                "screenshots TEXT NOT NULL,"
                                                "PRIMARY KEY(id, repo_id, architecture)"
                                                ") STRICT;");
        query.exec();
    }

    if (shouldCreatePackagesInstalledTable) {
        printf("Installed package index not found - creating\n");
        SQLite::Statement query(db, "CREATE TABLE " + packagesInstalledTableName + " ("
                                                "id TEXT NOT NULL,"
                                                "repo_id TEXT NOT NULL,"
                                                "architecture TEXT NOT NULL,"
                                                "version_name TEXT NOT NULL,"
                                                "version_number INTEGER NOT NULL,"
                                                "min_firmware INTEGER NOT NULL,"
                                                "max_firmware INTEGER NOT NULL,"
                                                "screenshots TEXT NOT NULL,"
                                                "PRIMARY KEY(id, repo_id, architecture)"
                                                ") STRICT;");
        query.exec();
    }
}

std::vector<Repository> Database::GetRepositories() {
    SQLite::Statement query(db, "SELECT * FROM " + repoTableName + ";");
    std::vector<Repository> repositories;
    while (query.executeStep()) {
        repositories.push_back({
            .id = query.getColumn(0),
            .uri = query.getColumn(1)
        });
    }

    return repositories;
}