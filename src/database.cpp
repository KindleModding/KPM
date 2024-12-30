#include "database.h"
#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Statement.h"
#include <cstring>
#include <vector>
#include "log.h"

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
        Log::D("Repos table not found - creating");
        SQLite::Statement query(db, "CREATE TABLE " + repoTableName + " ("
                                                "id TEXT NOT NULL PRIMARY KEY,"
                                                "url TEXT NOT NULL"
                                                ") STRICT;");
        query.exec();

        // Add default repo
        // @TODO
    }

    if (shouldCreatePackageIndexTable) {
        Log::D("Package index not found - creating");
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
        Log::D("Installed package index not found - creating");
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
            .url = query.getColumn(1)
        });
    }

    return repositories;
}

Repository Database::GetRepository(const std::string& id) {
    SQLite::Statement query(db, "SELECT * FROM " + repoTableName + " WHERE id=? LIMIT=1;");
    query.bind(1, id);
    const bool hasResult = query.executeStep();

    if (hasResult) {
        return {
            .id = query.getColumn(0),
            .url = query.getColumn(1)
        };
    } else {
        return {
            .id = "",
            .url = ""
        };
    }
}

int Database::AddRepository(Repository repository) {
    SQLite::Statement query(db, "INSERT INTO " + repoTableName + " (id, url) VALUES (?, ?);");
    query.bind(1, repository.id);
    query.bind(2, repository.url);
    return query.exec();
}

int Database::DeleteRepository(const std::string& id) {
    SQLite::Statement query(db, "DELETE FROM " + repoTableName + " WHERE id=?;");
    query.bind(1, id);
    return query.exec();
}

int Database::DeleteRepositoryPackages(const std::string& id) {
    SQLite::Statement query(db, "DELETE FROM " + packageIndexTableName + " WHERE repo_id=?;");
    query.bind(1, id);
    return query.exec();
}

std::vector<Package> Database::GetRepositoryPackages(const std::string& id) {
    SQLite::Statement query(db, "SELECT * FROM " + packageIndexTableName + " WHERE repo_id = ?;");
    query.bind(1, id);
    std::vector<Package> packages;
    while (query.executeStep()) {
        packages.push_back({
            .id = query.getColumn(0),
            .repo_id = query.getColumn(1),
            .architecture = query.getColumn(2),
            .version_name = query.getColumn(3),
            .version_number = query.getColumn(4),
            .min_firmware = query.getColumn(5),
            .max_firmware = query.getColumn(6),
            .screenshots = query.getColumn(7)
        });
    }

    return packages;
}

Package Database::GetPackage(const std::string& id) {
    SQLite::Statement query(db, "SELECT * FROM " + packageIndexTableName + " WHERE id=? LIMIT=1;");
    query.bind(1, id);
    const bool hasResult = query.executeStep();

    if (hasResult) {
        return {
            .id = query.getColumn(0),
            .repo_id = query.getColumn(1),
            .architecture = query.getColumn(2),
            .version_name = query.getColumn(3),
            .version_number = query.getColumn(4),
            .min_firmware = query.getColumn(5),
            .max_firmware = query.getColumn(6),
            .screenshots = query.getColumn(7)
        };
    } else {
        return {
            .id = "",
            .repo_id = "",
            .architecture = "",
            .version_name = "",
            .version_number = 0,
            .min_firmware = 0,
            .max_firmware = 0,
            .screenshots = 0
        };
    }
}

int Database::AddPackage(Package package) {
    SQLite::Statement query(db, "INSERT INTO " + packageIndexTableName + " (id, repo_id, architecture, version_name, version_number, min_firmware, max_firmware, screenshots) VALUES (?, ?, ?, ?, ?, ?, ?, ?);");
    query.bind(1, package.id);
    query.bind(2, package.repo_id);
    query.bind(3, package.architecture);
    query.bind(4, package.version_name);
    query.bind(5, package.version_number);
    query.bind(6, package.min_firmware);
    query.bind(7, package.max_firmware);
    query.bind(8, package.screenshots);
    return query.exec();
}

int Database::DeletePackage(const std::string& id) {
    SQLite::Statement query(db, "DELETE FROM " + packageIndexTableName + " WHERE id=?;");
    query.bind(1, id);
    return query.exec();
}