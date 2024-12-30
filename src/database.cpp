#include "database.hpp"
#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Statement.h"
#include "flags.hpp"
#include "log.hpp"
#include <cstring>
#include <stdexcept>
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

    SQLite::Statement createDependencyIndexTable(db, "CREATE TABLE IF NOT EXISTS dependency_index ("
                                            "package_id TEXT,"
                                            "repo_id TEXT,"
                                            "version_number INTEGER NOT NULL,"
                                            "architecture TEXT NOT NULL,"
                                            "dependency_install_string TEXT NOT NULL,"
                                            "PRIMARY KEY(package_id, repo_id, version_number, architecture),"
                                            "FOREIGN KEY (package_id, repo_id, version_number, architecture) REFERENCES version_index(package_id, repo_id, version_number, architecture) ON DELETE CASCADE"
                                            ") STRICT;");
    createDependencyIndexTable.exec();

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

std::vector<PackageWithVersion> Database::GetCompatiblePackageVersions(const std::string& package_id, const std::string& repo_id, const std::string& version_name, const std::string& version_comparison) {
    // Get version number if needed
    int version_number;
    if (version_comparison.length() != 0 && version_name.length() != 0) {
        std::string preQueryString = "SELECT version_number FROM version_index WHERE version_index.package_id=?1";
        if (repo_id.length() != 0) {
            preQueryString += " AND version_index.repo_id=?2";
        }
        preQueryString += " AND version_name = ?3 ORDER BY version_number ASC";

        SQLite::Statement preQuery(db, preQueryString);
        preQuery.bind(1, package_id);
        preQuery.bind(2, repo_id);
        preQuery.bind(3, version_name);

        const bool hasResult = preQuery.executeStep();
        if (!hasResult) {
            Log::E("Failed to get version with name: %s - ABORTING", version_name.c_str());
            exit(1);
        }

        version_number = preQuery.getColumn(0);
    }
    
    std::string queryString = "SELECT * FROM package_index JOIN version_index ON version_index.package_id=package_index.id";

    if (version_name.length() != 0 && version_comparison.length() != 0) {
        queryString += " AND version_index.repo_id=package_index.repo_id";
    }
    if (repo_id.length() != 0) {
        queryString += " WHERE package_index.repo_id = ?2";
    }
    queryString += " AND package_index.id = ?1 AND version_index.architecture = ?4";

    if (version_name.length() != 0 && version_comparison.length() != 0) {
        queryString += " AND version_number " + version_comparison + " ?5";
    }
    
    SQLite::Statement query(db, queryString + ";");
    query.bind(1, package_id);
    if (repo_id.length() != 0) {
        query.bind(2, repo_id);
    }
    if (version_name.length() != 0 && version_comparison.length() != 0) {
        query.bind(3, version_name);
        query.bind(5, version_number);
    }
    query.bind(4, Flags::GetInstance()->architecture);

    std::vector<PackageWithVersion> packages;
    while (query.executeStep()) {
        packages.push_back({
            .id = query.getColumn("package_id"),
            .repo_id = query.getColumn("repo_id"),
            .name = query.getColumn("name"),
            .description = query.getColumn("description"),
            .screenshots = query.getColumn("screenshots"),
            .package_id = query.getColumn("package_id"),
            .version_number = query.getColumn("version_number"),
            .version_name = query.getColumn("version_name"),
            .architecture = query.getColumn("architecture"),
            .min_firmware = query.getColumn("min_firmware"),
            .max_firmware = query.getColumn("max_firmware")
        });
    }

    return packages;
}

std::vector<PackageWithVersion> Database::SearchCompatiblePackages(const std::string& queryString) {
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


std::vector<PackageVersion> Database::GetPackageVersions(const Package& package) {
    SQLite::Statement query(db, "SELECT * FROM version_index WHERE package_id=? AND repo_id=? LIMIT 1;");
    query.bind(1, package.id);
    query.bind(2, package.repo_id);

    std::vector<PackageVersion> packageVersions;

    while (query.executeStep()) {
        packageVersions.push_back({
            .package_id = query.getColumn(0),
            .repo_id = query.getColumn(1),
            .version_number = query.getColumn(2),
            .version_name = query.getColumn(3),
            .architecture = query.getColumn(4),
            .min_firmware = query.getColumn(5),
            .max_firmware = query.getColumn(6)
        });
    }

    return packageVersions;
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

int Database::AddPackageVersionDependency(PackageVersionDependency packageVersionDependency) {
    SQLite::Statement query(db, "INSERT INTO dependency_index (package_id, repo_id, version_number, architecture, dependency_install_string) VALUES (?, ?, ?, ?, ?);");
    query.bind(1, packageVersionDependency.package_id);
    query.bind(2, packageVersionDependency.repo_id);
    query.bind(3, packageVersionDependency.version_number);
    query.bind(4, packageVersionDependency.architecture);
    query.bind(5, packageVersionDependency.dependency_install_string);
    return query.exec();
}

std::vector<PackageVersionDependency> Database::GetPackageVersionDependencies(const PackageVersion& version) {
    SQLite::Statement query(db, "SELECT * FROM dependency_index WHERE package_id=? AND repo_id=? AND version_number=? AND architecture=? LIMIT 1;");
    query.bind(1, version.package_id);
    query.bind(2, version.repo_id);
    query.bind(3, version.version_number);
    query.bind(4, version.architecture);

    std::vector<PackageVersionDependency> packageDependencies;

    while (query.executeStep()) {
        packageDependencies.push_back({
            .package_id = query.getColumn(0),
            .repo_id = query.getColumn(1),
            .version_number = query.getColumn(2),
            .architecture = query.getColumn(3),
            .dependency_install_string = query.getColumn(4)
        });
    }

    return packageDependencies;
}