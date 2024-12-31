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
                                            "version_number INTEGER NOT NULL,"
                                            "version_name TEXT NOT NULL,"
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
                                            "package_id TEXT NOT NULL,"
                                            "repository_id TEXT NOT NULL,"
                                            "version_number TEXT NOT NULL,"
                                            "version_comparison TEXT NOT NULL,"
                                            "PRIMARY KEY(package_id, repository_id, version_number, architecture),"
                                            "FOREIGN KEY (package_id, repository_id, version_number, architecture) REFERENCES version_index(package_id, repository_id, version_number, architecture) ON DELETE CASCADE"
                                            ") STRICT;");
    createDependencyIndexTable.exec();

    SQLite::Statement createInstalledPackagesTable(db, "CREATE TABLE IF NOT EXISTS installed_packages ("
                                            "package_id TEXT NOT NULL PRIMARY KEY,"
                                            "repository_id TEXT NOT NULL,"
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

void Database::Begin() {
    /**
     * @brief Begin a transaction on the database
     * 
     */

    if (isTransaction) {
        return;
    }

    db.exec("BEGIN");
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

uint Database::ConvertVersionNameToNumber(const std::string& package_id, const std::string& repository_id, const std::string& version_name) {
    /**
     * @brief By querying the database, determine the version number of a package version from its version name
     * 
     */
    // Get version number if needed
    std::string preQueryString = "SELECT version_number FROM version_index JOIN package_index ON package_index.id=version_index.package_id WHERE version_index.package_id=?1 OR package_index.alias=?1";
    if (repository_id.length() != 0) {
        preQueryString += " AND version_index.repository_id=?2";
    }
    preQueryString += " AND version_name = ?3 ORDER BY version_number ASC";

    SQLite::Statement preQuery(db, preQueryString);
    preQuery.bind(1, package_id);
    preQuery.bind(2, repository_id);
    preQuery.bind(3, version_name);

    const bool hasResult = preQuery.executeStep();
    if (!hasResult) {
        Log::E("Failed to find package with version name: %s %s - ABORTING", package_id.c_str(), version_name.c_str());
        exit(1);
    }

    return preQuery.getColumn("version_number");
}

std::vector<PackageWithVersion> Database::GetCompatiblePackageVersions(const std::string& package_id, const std::string& repository_id, const uint& version_number, const std::string& version_comparison) {
    /**
     * @brief Given a set of constraints, find compatible package version candidates that we can install
     * 
     */
    std::string queryString = "SELECT * FROM package_index JOIN version_index ON version_index.package_id=package_index.id";

    if (version_comparison.length() != 0) {
        queryString += " AND version_index.repository_id=package_index.repository_id";
    }
    if (repository_id.length() != 0) {
        queryString += " WHERE package_index.repository_id = ?2";
    }
    queryString += " AND (package_index.id = ?1 OR package_index.alias = ?1) AND version_index.architecture = ?4";

    if (version_comparison.length() != 0) {
        queryString += " AND version_number " + version_comparison + " ?5";
    }
    
    SQLite::Statement query(db, queryString + ";");
    query.bind(1, package_id);
    if (repository_id.length() != 0) {
        query.bind(2, repository_id);
    }
    if (version_comparison.length() != 0) {
        query.bind(5, version_number);
    }
    query.bind(4, Flags::GetInstance()->architecture);

    std::vector<PackageWithVersion> packages;
    while (query.executeStep()) {
        packages.push_back({
            .id = query.getColumn("package_id"),
            .alias = query.getColumn("alias"),
            .repository_id = query.getColumn("repository_id"),
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
    /**
     * @brief Search the package index for packages
     * 
     */
    SQLite::Statement query(db, "SELECT * FROM (SELECT *, MAX(version_number) latest_version FROM package_index LEFT JOIN version_index ON package_id=id WHERE (name LIKE ?1 OR alias LIKE ?1 OR id LIKE ?1) AND architecture = ?2 ORDER BY ABS(LENGTH(?1) - LENGTH(name)) ASC) WHERE version_number = latest_version;");
    query.bind(1, queryString);
    query.bind(2, Flags::GetInstance()->architecture);
    std::vector<PackageWithVersion> packages;
    while (query.executeStep()) {
        packages.push_back({
            .id = query.getColumn("id"),
            .alias = query.getColumn("alias"),
            .repository_id = query.getColumn("repository_id"),
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

int Database::AddPackage(Package package) {
    /**
     * @brief Add a package into the package index
     * 
     */
    SQLite::Statement query(db, "INSERT INTO package_index (id, alias, repository_id, name, description, screenshots) VALUES (?, ?, ?, ?, ?, ?);");
    query.bind(1, package.id);
    query.bind(2, package.alias);
    query.bind(3, package.repository_id);
    query.bind(4, package.name);
    query.bind(5, package.description);
    query.bind(6, package.screenshots);
    return query.exec();
}

int Database::AddPackageVersion(PackageVersion packageVersion) {
    /**
     * @brief Add a package version to the package version index
     * 
     */
    SQLite::Statement query(db, "INSERT INTO version_index (package_id, repository_id, version_number, version_name, architecture, min_firmware, max_firmware) VALUES (?, ?, ?, ?, ?, ?, ?);");
    query.bind(1, packageVersion.package_id);
    query.bind(2, packageVersion.repository_id);
    query.bind(3, packageVersion.version_number);
    query.bind(4, packageVersion.version_name);
    query.bind(5, packageVersion.architecture);
    query.bind(6, packageVersion.min_firmware);
    query.bind(7, packageVersion.max_firmware);
    return query.exec();
}

int Database::AddPackageVersionDependency(PackageVersionDependency packageVersionDependency) {
    /**
     * @brief Add a package version dependency to the dependency index
     * 
     */
    SQLite::Statement query(db, "INSERT INTO dependency_index (dependent_package_id, dependent_repository_id, dependent_version_number, dependent_architecture, package_id, repository_id, version_number, version_comparison) VALUES (?, ?, ?, ?, ?, ?, ?, ?);");
    query.bind(1, packageVersionDependency.dependent_package_id);
    query.bind(2, packageVersionDependency.dependent_repository_id);
    query.bind(3, packageVersionDependency.dependent_version_number);
    query.bind(4, packageVersionDependency.dependent_architecture);
    query.bind(5, packageVersionDependency.package_id);
    query.bind(6, packageVersionDependency.repository_id);
    query.bind(7, packageVersionDependency.version_number);
    query.bind(8, packageVersionDependency.version_comparison);
    return query.exec();
}

std::vector<PackageVersionDependency> Database::GetPackageVersionDependencies(const PackageVersion& version) {
    /**
     * @brief Given a specific package version, get its dependencies
     * 
     */
    SQLite::Statement query(db, "SELECT * FROM dependency_index WHERE dependent_package_id=? AND dependent_repository_id=? AND dependent_version_number=? AND dependent_architecture=? LIMIT 1;");
    query.bind(1, version.package_id);
    query.bind(2, version.repository_id);
    query.bind(3, version.version_number);
    query.bind(4, version.architecture);

    std::vector<PackageVersionDependency> packageDependencies;

    while (query.executeStep()) {
        packageDependencies.push_back({
            .dependent_package_id = query.getColumn("dependent_package_id"),
            .dependent_repository_id = query.getColumn("dependent_repository_id"),
            .dependent_version_number = query.getColumn("dependent_version_number"),
            .dependent_architecture = query.getColumn("dependent_architecture"),
            .package_id = query.getColumn("package_id"),
            .repository_id = query.getColumn("repository_id"),
            .version_number = query.getColumn("version_number"),
            .version_comparison = query.getColumn("version_comparison")
        });
    }

    return packageDependencies;
}

std::vector<PackageVersionDependency> Database::GetRequiredDependencies() {
    /**
     * @brief Get every required dependency from installed packages
     * 
     */
    SQLite::Statement query(db, "SELECT * FROM dependency_index JOIN installed_packages ON installed_packages.package_id=dependency_index.dependent_package_id AND installed_packages.repository_id=dependency_index.dependent_repository_id AND installed_packages.version_number=dependency_index.dependent_version_number WHERE dependency_index.dependent_architecture = ?1");
    query.bind(1, Flags::GetInstance()->architecture);

    std::vector<PackageVersionDependency> packageDependencies;

    while (query.executeStep()) {
        packageDependencies.push_back({
            .dependent_package_id = query.getColumn("dependent_package_id"),
            .dependent_repository_id = query.getColumn("dependent_repository_id"),
            .dependent_version_number = query.getColumn("dependent_version_number"),
            .dependent_architecture = query.getColumn("dependent_architecture"),
            .package_id = query.getColumn("package_id"),
            .repository_id = query.getColumn("repository_id"),
            .version_number = query.getColumn("version_number"),
            .version_comparison = query.getColumn("version_comparison")
        });
    }

    return packageDependencies;
}