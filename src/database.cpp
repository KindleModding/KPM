#include "database.hpp"
#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Statement.h"
#include "flags.hpp"
#include "log.hpp"
#include <cstring>
#include <vector>

Database::Database(std::string path): db(path, SQLite::OPEN_READWRITE|SQLite::OPEN_CREATE) {
    /**
     * @brief Initialises the database object and creates the database file with the required tables as needed
     * 
     */
    SQLite::Statement setup(db, "PRAGMA foreign_keys = ON;");
    setup.exec();

    SQLite::Statement createReposTable(db, "CREATE TABLE IF NOT EXISTS repositories ("
                                            "id TEXT NOT NULL PRIMARY KEY,"
                                            "url TEXT NOT NULL"
                                            ") STRICT;");
    createReposTable.exec();

    SQLite::Statement createPackageIndexTable(db, "CREATE TABLE IF NOT EXISTS package_index ("
                                            "id TEXT NOT NULL,"
                                            "alias TEXT NOT NULL,"
                                            "repository_id TEXT REFERENCES repositories(id) ON DELETE CASCADE,"
                                            "name TEXT NOT NULL,"
                                            "description TEXT NOT NULL,"
                                            "screenshots TEXT NOT NULL,"
                                            "PRIMARY KEY(id, repository_id)"
                                            ") STRICT;");
    createPackageIndexTable.exec();

    SQLite::Statement createVersionIndexTable(db, "CREATE TABLE IF NOT EXISTS version_index ("
                                            "package_id TEXT,"
                                            "repository_id TEXT,"
                                            "version_name TEXT NOT NULL,"
                                            "version_number INTEGER NOT NULL,"
                                            "architecture TEXT NOT NULL,"
                                            "min_firmware TEXT NOT NULL,"
                                            "max_firmware TEXT NOT NULL,"
                                            "PRIMARY KEY(package_id, repository_id, version_number, architecture),"
                                            "FOREIGN KEY (package_id, repository_id) REFERENCES package_index(id, repository_id) ON DELETE CASCADE"
                                            ") STRICT;");
    createVersionIndexTable.exec();

    SQLite::Statement createDependencyIndexTable(db, "CREATE TABLE IF NOT EXISTS dependency_index ("
                                            "dependent_package_id TEXT,"
                                            "dependent_repository_id TEXT,"
                                            "dependent_version_number INTEGER NOT NULL,"
                                            "dependent_architecture TEXT NOT NULL,"
                                            "install_string TEXT NOT NULL,"
                                            "PRIMARY KEY(dependent_package_id, dependent_repository_id, dependent_version_number, dependent_architecture),"
                                            "FOREIGN KEY (dependent_package_id, dependent_repository_id, dependent_version_number, dependent_architecture) REFERENCES version_index(package_id, repository_id, version_number, architecture) ON DELETE CASCADE"
                                            ") STRICT;");
    createDependencyIndexTable.exec();

    SQLite::Statement createInstalledPackagesTable(db, "CREATE TABLE IF NOT EXISTS installed_packages ("
                                            "package_id TEXT NOT NULL,"
                                            "repository_id TEXT NOT NULL,"
                                            "name TEXT NOT NULL,"
                                            "description TEXT NOT NULL,"
                                            "screenshots TEXT NOT NULL,"
                                            "version_name TEXT NOT NULL,"
                                            "version_number INTEGER NOT NULL,"
                                            "min_firmware TEXT NOT NULL,"
                                            "max_firmware TEXT NOT NULL,"
                                            "PRIMARY KEY(package_id, repository_id, version_number)"
                                            ") STRICT;");
    createInstalledPackagesTable.exec();

    SQLite::Statement createInstalledDependencyTable(db, "CREATE TABLE IF NOT EXISTS installed_package_dependencies ("
                                            "dependent_package_id TEXT,"
                                            "dependent_repository_id TEXT,"
                                            "dependent_version_number INTEGER NOT NULL,"
                                            "dependent_architecture TEXT NOT NULL,"
                                            "install_string TEXT NOT NULL,"
                                            "PRIMARY KEY(dependent_package_id, dependent_repository_id, dependent_version_number, dependent_architecture),"
                                            "FOREIGN KEY (dependent_package_id, dependent_repository_id, dependent_version_number) REFERENCES installed_packages(package_id, repository_id, version_number) ON DELETE CASCADE"
                                            ") STRICT;");
    createInstalledDependencyTable.exec();
}

void Database::Begin() {
    /**
     * @brief Begin a transaction on the database
     * 
     */

    if (isTransaction) {
        return;
    }

    db.exec("BEGIN");
    isTransaction = true;
}

void Database::End(bool rollback) {
    /**
     * @brief End a transaction on the database, rolling back if applicable
     * 
     */
    if (!isTransaction) {
        return;
    }

    if (rollback) {
        db.exec("ROLLBACK");
    } else {
        db.exec("COMMIT");
    }
    isTransaction = false;
}

std::vector<Repository> Database::GetRepositories() {
    /**
     * @brief Return a list of Repository objects from the repository list on the database
     * 
     */
    SQLite::Statement query(db, "SELECT * FROM repositories;");
    std::vector<Repository> repositories;
    while (query.executeStep()) {
        repositories.push_back({
            .id = query.getColumn("id"),
            .url = query.getColumn("url")
        });
    }

    return repositories;
}

Repository Database::GetRepository(const std::string& id) {
    /**
     * @brief Get a single repository from its ID
     * 
     */
    SQLite::Statement query(db, "SELECT * FROM repositories WHERE id=? LIMIT 1;");
    query.bind(1, id);
    const bool hasResult = query.executeStep();

    if (hasResult) {
        return {
            .id = query.getColumn("id"),
            .url = query.getColumn("url")
        };
    } else {
        return {
            .id = "",
            .url = ""
        };
    }
}

int Database::AddRepository(Repository repository) {
    /**
     * @brief Insert a single repository into the repositories table
     * 
     */
    SQLite::Statement query(db, "INSERT INTO repositories (id, url) VALUES (?, ?);");
    query.bind(1, repository.id);
    query.bind(2, repository.url);
    return query.exec();
}

int Database::DeleteRepository(const std::string& id) {
    /**
     * @brief Remove a single repository from the repositories repo by its ID
     * 
     */
    SQLite::Statement query(db, "DELETE FROM repositories WHERE id=?;");
    query.bind(1, id);
    return query.exec();
}

int Database::DeleteRepositoryPackages(const std::string& id) {
    /**
     * @brief Clear all packages in the package_index of a certain repository by its id (cascades to other tables)
     * 
     */
    SQLite::Statement query(db, "DELETE FROM package_index WHERE repository_id=?;");
    query.bind(1, id);
    return query.exec();
}

void Database::AddPackage(Package package) {
    SQLite::Statement query(db, "INSERT INTO package_index (id, alias, repository_id, name, description, screenshots) VALUES (?, ?, ?, ?, ?, ?);");
    query.bind(1, package.id);
    query.bind(2, package.alias);
    query.bind(3, package.repository_id);
    query.bind(4, package.name);
    query.bind(5, package.description);
    query.bind(6, package.screenshots);
    query.exec();
}

void Database::AddPackageVersion(PackageVersion packageVersion) {
    SQLite::Statement query(db, "INSERT INTO version_index (package_id, repository_id, version_name, version_number, architecture, min_firmware, max_firmware) VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.bind(1, packageVersion.package_id);
    query.bind(2, packageVersion.repository_id);
    query.bind(3, packageVersion.version_name);
    query.bind(4, packageVersion.version_number);
    query.bind(5, packageVersion.architecture);
    query.bind(6, packageVersion.min_firmware);
    query.bind(7, packageVersion.max_firmware);
    query.exec();
}

void Database::AddPackageDependency(PackageDependency packageDependency) {
    SQLite::Statement query(db, "INSERT INTO dependency_index VALUES (?, ?, ?, ?, ?)");
    query.bind(1, packageDependency.dependent_package_id);
    query.bind(2, packageDependency.dependent_repository_id);
    query.bind(3, packageDependency.dependent_version_number);
    query.bind(4, packageDependency.dependent_architecture);
    query.bind(5, packageDependency.install_string);
    query.exec();
}