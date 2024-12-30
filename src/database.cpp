#include "database.hpp"
#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Statement.h"
#include "flags.hpp"
#include <cstring>
#include <vector>

Database::Database(std::string path): db(path, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE) {
    SQLite::Statement setup(db, "PRAGMA foreign_keys = ON;");
    setup.exec();

    SQLite::Statement createReposTable(db, "CREATE TABLE IF NOT EXISTS repos ("
                                            "id TEXT NOT NULL PRIMARY KEY,"
                                            "url TEXT NOT NULL"
                                            ") STRICT;");
    createReposTable.exec();

    SQLite::Statement createPackageIndexTable(db, "CREATE TABLE IF NOT EXISTS package_index ("
                                            "id TEXT NOT NULL,"
                                            "repo_id TEXT REFERENCES repos(id) ON DELETE CASCADE,"
                                            "name TEXT NOT NULL,"
                                            "description TEXT NOT NULL,"
                                            "screenshots TEXT NOT NULL,"
                                            "PRIMARY KEY(id, repo_id)"
                                            ") STRICT;");
    createPackageIndexTable.exec();

    SQLite::Statement createVersionIndexTable(db, "CREATE TABLE IF NOT EXISTS version_index ("
                                            "package_id TEXT,"
                                            "repo_id TEXT,"
                                            "version_number INTEGER NOT NULL,"
                                            "version_name TEXT NOT NULL,"
                                            "architecture TEXT NOT NULL,"
                                            "min_firmware TEXT NOT NULL,"
                                            "max_firmware TEXT NOT NULL,"
                                            "PRIMARY KEY(package_id, repo_id, version_number, architecture),"
                                            "FOREIGN KEY (package_id, repo_id) REFERENCES package_index(id, repo_id) ON DELETE CASCADE"
                                            ") STRICT;");
    createVersionIndexTable.exec();

    SQLite::Statement createInstalledPackagesTable(db, "CREATE TABLE IF NOT EXISTS installed_packages ("
                                            "package_id TEXT NOT NULL PRIMARY KEY,"
                                            "repo_id TEXT NOT NULL,"
                                            "name TEXT NOT NULL,"
                                            "description TEXT NOT NULL,"
                                            "screenshots TEXT NOT NULL,"
                                            "version_name TEXT NOT NULL,"
                                            "version_number INTEGER NOT NULL,"
                                            "min_firmware TEXT NOT NULL,"
                                            "max_firmware TEXT NOT NULL"
                                            ") STRICT;");
    createInstalledPackagesTable.exec();
}

std::vector<Repository> Database::GetRepositories() {
    SQLite::Statement query(db, "SELECT * FROM repos;");
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
    SQLite::Statement query(db, "SELECT * FROM repos WHERE id=? LIMIT 1;");
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
    SQLite::Statement query(db, "INSERT INTO repos (id, url) VALUES (?, ?);");
    query.bind(1, repository.id);
    query.bind(2, repository.url);
    return query.exec();
}

int Database::DeleteRepository(const std::string& id) {
    SQLite::Statement query(db, "DELETE FROM repos WHERE id=?;");
    query.bind(1, id);
    return query.exec();
}

int Database::DeleteRepositoryPackages(const std::string& id) {
    SQLite::Statement query(db, "DELETE FROM package_index WHERE repo_id=?;");
    query.bind(1, id);
    return query.exec();
}

std::vector<Package> Database::GetRepositoryPackages(const std::string& id) {
    SQLite::Statement query(db, "SELECT * FROM package_index WHERE repo_id = ?;");
    query.bind(1, id);
    std::vector<Package> packages;
    while (query.executeStep()) {
        packages.push_back({
            .id = query.getColumn(0),
            .repo_id = query.getColumn(1),
            .name = query.getColumn(2),
            .description = query.getColumn(3),
            .screenshots = query.getColumn(4)
        });
    }

    return packages;
}

Package Database::GetPackage(const std::string& id) {
    SQLite::Statement query(db, "SELECT * FROM package_index WHERE id=? LIMIT 1;");
    query.bind(1, id);
    const bool hasResult = query.executeStep();

    if (hasResult) {
        return {
            .id = query.getColumn(0),
            .repo_id = query.getColumn(1),
            .name = query.getColumn(2),
            .description = query.getColumn(3),
            .screenshots = query.getColumn(4)
        };
    } else {
        return {
            .id = "",
            .repo_id = "",
            .name = "",
            .description = "",
            .screenshots = ""
        };
    }
}

std::vector<PackageWithVersion> Database::FindPackage(const std::string& queryString) {
    SQLite::Statement query(db, "SELECT * FROM (SELECT *, MAX(version_number) latest_version FROM package_index LEFT JOIN version_index ON package_id=id WHERE name LIKE ?1 AND architecture = ?2 ORDER BY ABS(LENGTH(?1) - LENGTH(name)) ASC) WHERE version_number = latest_version;");
    query.bind(1, queryString);
    query.bind(2, Flags::GetInstance()->architecture);
    std::vector<PackageWithVersion> packages;
    while (query.executeStep()) {
        packages.push_back({
            .id = query.getColumn(0),
            .repo_id = query.getColumn(1),
            .name = query.getColumn(2),
            .description = query.getColumn(3),
            .screenshots = query.getColumn(4),
            .package_id = query.getColumn(5),
            .version_number = query.getColumn(7),
            .version_name = query.getColumn(8),
            .architecture = query.getColumn(9),
            .min_firmware = query.getColumn(10),
            .max_firmware = query.getColumn(11)
        });
    }

    return packages;
}

int Database::AddPackage(Package package) {
    SQLite::Statement query(db, "INSERT INTO package_index (id, repo_id, name, description, screenshots) VALUES (?, ?, ?, ?, ?);");
    query.bind(1, package.id);
    query.bind(2, package.repo_id);
    query.bind(3, package.name);
    query.bind(4, package.description);
    query.bind(5, package.screenshots);
    return query.exec();
}

int Database::DeletePackage(const std::string& id) {
    SQLite::Statement query(db, "DELETE FROM package_index WHERE id=?;");
    query.bind(1, id);
    return query.exec();
}


PackageVersion Database::GetPackageVersion(const std::string& package_id) {
    SQLite::Statement query(db, "SELECT * FROM version_index WHERE package_id=? LIMIT 1;");
    query.bind(1, package_id);
    const bool hasResult = query.executeStep();

    if (hasResult) {
        return {
            .package_id = query.getColumn(0),
            .repo_id = query.getColumn(1),
            .version_number = query.getColumn(2),
            .version_name = query.getColumn(3),
            .architecture = query.getColumn(4),
            .min_firmware = query.getColumn(5),
            .max_firmware = query.getColumn(6)
        };
    } else {
        return {
            .package_id = "",
            .repo_id = "",
            .version_number = 0,
            .version_name = "",
            .architecture = "",
            .min_firmware = 0,
            .max_firmware = 0
        };
    }
}

int Database::AddPackageVersion(PackageVersion packageVersion) {
    SQLite::Statement query(db, "INSERT INTO version_index (package_id, repo_id, version_number, version_name, architecture, min_firmware, max_firmware) VALUES (?, ?, ?, ?, ?, ?, ?);");
    query.bind(1, packageVersion.package_id);
    query.bind(2, packageVersion.repo_id);
    query.bind(3, packageVersion.version_number);
    query.bind(4, packageVersion.version_name);
    query.bind(5, packageVersion.architecture);
    query.bind(6, packageVersion.min_firmware);
    query.bind(7, packageVersion.max_firmware);
    return query.exec();
}

int Database::DeletePackageVersions(const std::string& package_id, const std::string& repo_id) {
    SQLite::Statement query(db, "DELETE FROM version_index WHERE package_id=? AND repo_id=?;");
    query.bind(1, package_id);
    query.bind(2, repo_id);
    return query.exec();
}