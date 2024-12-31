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
                                            "version_comparison_type INTEGER NOT NULL,"
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
                                            "package_id TEXT NOT NULL,"
                                            "repository_id TEXT NOT NULL,"
                                            "version_number TEXT NOT NULL,"
                                            "version_comparison_type INTEGER NOT NULL,"
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

std::vector<PackageInstallCandidate> Database::GetCompatiblePackageVersions(const std::string& package_id, const std::string& repository_id, const uint& version_number, const VersionComparisonType& version_comparison_type) {
    /**
     * @brief Given a set of constraints, find compatible package version candidates that we can install
     * 
     */
    std::string queryString = "SELECT package_index.*, version_index.*, repositories.url repository_url FROM package_index JOIN repositories ON repositories.id=package_index.repository_id JOIN version_index ON version_index.package_id=package_index.id";

    if (version_comparison_type != VersionComparisonType::NONE) {
        queryString += " AND version_index.repository_id=package_index.repository_id";
    }
    if (repository_id.length() != 0) {
        queryString += " WHERE package_index.repository_id = ?2";
    }
    queryString += " AND (package_index.id = ?1 OR package_index.alias = ?1) AND version_index.architecture = ?4";

    if (version_comparison_type != VersionComparisonType::NONE) {
        queryString += " AND version_number ";
        switch (version_comparison_type) {
            case VersionComparisonType::EQ:
                queryString += "=";
                break;
            case VersionComparisonType::GT:
                queryString += ">";
                break;
            case VersionComparisonType::LT:
                queryString += "<";
                break;
            case VersionComparisonType::LTEQ:
                queryString += "<=";
                break;
            case VersionComparisonType::GTEQ:
                queryString += ">=";
                break;
            default:
                break;
        }
        queryString += " ?5";
    }
    
    SQLite::Statement query(db, queryString + ";");
    query.bind(1, package_id);
    if (repository_id.length() != 0) {
        query.bind(2, repository_id);
    }
    if (version_comparison_type != VersionComparisonType::NONE) {
        query.bind(5, version_number);
    }
    query.bind(4, Flags::GetInstance()->architecture);

    std::vector<PackageInstallCandidate> packages;
    while (query.executeStep()) {
        packages.push_back({
            .package_id = query.getColumn("package_id"),
            .alias = query.getColumn("alias"),
            .repository_id = query.getColumn("repository_id"),
            .repository_url = query.getColumn("repository_url"),
            .name = query.getColumn("name"),
            .description = query.getColumn("description"),
            .screenshots = query.getColumn("screenshots"),
            .version_number = query.getColumn("version_number"),
            .version_name = query.getColumn("version_name"),
            .architecture = query.getColumn("architecture"),
            .min_firmware = query.getColumn("min_firmware"),
            .max_firmware = query.getColumn("max_firmware")
        });
    }

    return packages;
}

std::vector<PackageInstallCandidate> Database::SearchCompatiblePackages(const std::string& queryString) {
    /**
     * @brief Search the package index for packages
     * 
     */
    SQLite::Statement query(db, "SELECT * FROM (SELECT *, MAX(version_number) latest_version FROM package_index LEFT JOIN version_index ON package_id=id WHERE (name LIKE ?1 OR alias LIKE ?1 OR id LIKE ?1) AND architecture = ?2 ORDER BY ABS(LENGTH(?1) - LENGTH(name)) ASC) WHERE version_number = latest_version;");
    query.bind(1, queryString);
    query.bind(2, Flags::GetInstance()->architecture);
    std::vector<PackageInstallCandidate> packages;
    while (query.executeStep()) {
        packages.push_back({
            .package_id = query.getColumn("package_id"),
            .alias = query.getColumn("alias"),
            .repository_id = query.getColumn("repository_id"),
            .name = query.getColumn("name"),
            .description = query.getColumn("description"),
            .screenshots = query.getColumn("screenshots"),
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

int Database::AddPackageDependency(PackageDependency packageVersionDependency) {
    /**
     * @brief Add a package version dependency to the dependency index
     * 
     */
    SQLite::Statement query(db, "INSERT INTO dependency_index (dependent_package_id, dependent_repository_id, dependent_version_number, dependent_architecture, package_id, repository_id, version_number, version_comparison_type) VALUES (?, ?, ?, ?, ?, ?, ?, ?);");
    query.bind(1, packageVersionDependency.dependent_package_id);
    query.bind(2, packageVersionDependency.dependent_repository_id);
    query.bind(3, packageVersionDependency.dependent_version_number);
    query.bind(4, packageVersionDependency.dependent_architecture);
    query.bind(5, packageVersionDependency.package_id);
    query.bind(6, packageVersionDependency.repository_id);
    query.bind(7, packageVersionDependency.version_number);
    query.bind(8, static_cast<int>(packageVersionDependency.version_comparison_type));
    return query.exec();
}

std::vector<PackageDependency> Database::GetPackageVersionDependencies(const PackageVersion& version) {
    /**
     * @brief Given a specific package version, get its dependencies
     * 
     */
    SQLite::Statement query(db, "SELECT * FROM dependency_index WHERE dependent_package_id=? AND dependent_repository_id=? AND dependent_version_number=? AND dependent_architecture=? LIMIT 1;");
    query.bind(1, version.package_id);
    query.bind(2, version.repository_id);
    query.bind(3, version.version_number);
    query.bind(4, version.architecture);

    std::vector<PackageDependency> packageDependencies;

    while (query.executeStep()) {
        packageDependencies.push_back({
            .dependent_package_id = query.getColumn("dependent_package_id"),
            .dependent_repository_id = query.getColumn("dependent_repository_id"),
            .dependent_version_number = query.getColumn("dependent_version_number"),
            .dependent_architecture = query.getColumn("dependent_architecture"),
            .package_id = query.getColumn("package_id"),
            .repository_id = query.getColumn("repository_id"),
            .version_number = query.getColumn("version_number"),
            .version_comparison_type = static_cast<VersionComparisonType>(query.getColumn("version_comparison_type").getInt())
        });
    }

    return packageDependencies;
}

std::vector<PackageDependency> Database::GetRequiredDependencies() {
    /**
     * @brief Get every required dependency from installed packages
     * 
     */
    SQLite::Statement query(db, "SELECT * FROM dependency_index JOIN installed_packages ON installed_packages.package_id=dependency_index.dependent_package_id AND installed_packages.repository_id=dependency_index.dependent_repository_id AND installed_packages.version_number=dependency_index.dependent_version_number WHERE dependency_index.dependent_architecture = ?1");
    query.bind(1, Flags::GetInstance()->architecture);

    std::vector<PackageDependency> packageDependencies;

    while (query.executeStep()) {
        packageDependencies.push_back({
            .dependent_package_id = query.getColumn("dependent_package_id"),
            .dependent_repository_id = query.getColumn("dependent_repository_id"),
            .dependent_version_number = query.getColumn("dependent_version_number"),
            .dependent_architecture = query.getColumn("dependent_architecture"),
            .package_id = query.getColumn("package_id"),
            .repository_id = query.getColumn("repository_id"),
            .version_number = query.getColumn("version_number"),
            .version_comparison_type = static_cast<VersionComparisonType>(query.getColumn("version_comparison_type").getInt())
        });
    }

    return packageDependencies;
}

void Database::InstallPackage(PackageInstallCandidate package) {
    // Remove if the package is currently installed
    SQLite::Statement deletePackageQuery(db, "DELETE FROM installed_packages WHERE package_id = ?1");
    deletePackageQuery.bind(1, package.package_id);
    deletePackageQuery.exec();

    // Add package and its dependencies to the corresponding tables
    SQLite::Statement insertInstalledPackage(db, "INSERT INTO installed_packages (package_id, repository_id, name, description, screenshots, version_name, version_number, min_firmware, max_firmware) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insertInstalledPackage.bind(1, package.package_id);
    insertInstalledPackage.bind(2, package.repository_id);
    insertInstalledPackage.bind(3, package.name);
    insertInstalledPackage.bind(4, package.description);
    insertInstalledPackage.bind(5, package.screenshots);
    insertInstalledPackage.bind(6, package.version_name);
    insertInstalledPackage.bind(7, package.version_number);
    insertInstalledPackage.bind(8, package.min_firmware);
    insertInstalledPackage.bind(9, package.max_firmware);
    insertInstalledPackage.exec();

    // Now add the dependencies
    SQLite::Statement copyDependencyQuery(db, "INSERT INTO installed_package_dependencies SELECT * FROM dependency_index WHERE dependent_package_id=? AND dependent_repository_id=? AND dependent_version_number=? AND dependent_architecture=?");
    copyDependencyQuery.bind(1, package.package_id);
    copyDependencyQuery.bind(2, package.repository_id);
    copyDependencyQuery.bind(3, package.version_number);
    copyDependencyQuery.bind(4, Flags::GetInstance()->architecture);
    copyDependencyQuery.exec();
}

bool Database::CheckConflicts() {
    bool conflicts = false;

    // List installed dependencies which are the wrong version
    SQLite::Statement checkPackagesViolatingGTEQ(db, "SELECT installed_package_dependencies.*, installed_packages.version_number installed_version_number FROM installed_packages JOIN installed_package_dependencies ON installed_packages.package_id=installed_package_dependencies.package_id WHERE version_comparison_type=? AND installed_packages.version_number < installed_package_dependencies.version_number");
    checkPackagesViolatingGTEQ.bind(1, static_cast<int>(VersionComparisonType::GTEQ));
    while (checkPackagesViolatingGTEQ.executeStep()) {
        conflicts = true;
        Log::E("Package %s@%s conflicts with %s@%s (which depends on %s)",
            checkPackagesViolatingGTEQ.getColumn("package_id").getString().c_str(),
            checkPackagesViolatingGTEQ.getColumn("installed_version_number").getString().c_str(),
            checkPackagesViolatingGTEQ.getColumn("dependent_package_id").getString().c_str(),
            checkPackagesViolatingGTEQ.getColumn("dependent_version_number").getString().c_str()
        );
    }

    SQLite::Statement checkPackagesViolatingLTEQ(db, "SELECT installed_package_dependencies.*, installed_packages.version_number installed_version_number FROM installed_packages JOIN installed_package_dependencies ON installed_packages.package_id=installed_package_dependencies.package_id WHERE version_comparison_type=? AND installed_packages.version_number > installed_package_dependencies.version_number");
    checkPackagesViolatingLTEQ.bind(1, static_cast<int>(VersionComparisonType::LTEQ));
    while (checkPackagesViolatingLTEQ.executeStep()) {
        conflicts = true;
        Log::E("Package %s@%s conflicts with %s@%s (which depends on %s)",
            checkPackagesViolatingLTEQ.getColumn("package_id").getString().c_str(),
            checkPackagesViolatingLTEQ.getColumn("installed_version_number").getString().c_str(),
            checkPackagesViolatingLTEQ.getColumn("dependent_package_id").getString().c_str(),
            checkPackagesViolatingLTEQ.getColumn("dependent_version_number").getString().c_str()
        );
    }

    SQLite::Statement checkPackagesViolatingEQ(db, "SELECT installed_package_dependencies.*, installed_packages.version_number installed_version_number FROM installed_packages JOIN installed_package_dependencies ON installed_packages.package_id=installed_package_dependencies.package_id WHERE version_comparison_type=? AND installed_packages.version_number != installed_package_dependencies.version_number");
    checkPackagesViolatingEQ.bind(1, static_cast<int>(VersionComparisonType::EQ));
    while (checkPackagesViolatingEQ.executeStep()) {
        conflicts = true;
        Log::E("Package %s@%s conflicts with %s@%s (which depends on %s)",
            checkPackagesViolatingEQ.getColumn("package_id").getString().c_str(),
            checkPackagesViolatingEQ.getColumn("installed_version_number").getString().c_str(),
            checkPackagesViolatingEQ.getColumn("dependent_package_id").getString().c_str(),
            checkPackagesViolatingEQ.getColumn("dependent_version_number").getString().c_str()
        );
    }

    SQLite::Statement checkPackagesViolatingGT(db, "SELECT installed_package_dependencies.*, installed_packages.version_number installed_version_number FROM installed_packages JOIN installed_package_dependencies ON installed_packages.package_id=installed_package_dependencies.package_id WHERE version_comparison_type=? AND installed_packages.version_number <= installed_package_dependencies.version_number");
    checkPackagesViolatingGT.bind(1, static_cast<int>(VersionComparisonType::GT));
    while (checkPackagesViolatingGT.executeStep()) {
        conflicts = true;
        Log::E("Package %s@%s conflicts with %s@%s (which depends on %s)",
            checkPackagesViolatingGT.getColumn("package_id").getString().c_str(),
            checkPackagesViolatingGT.getColumn("installed_version_number").getString().c_str(),
            checkPackagesViolatingGT.getColumn("dependent_package_id").getString().c_str(),
            checkPackagesViolatingGT.getColumn("dependent_version_number").getString().c_str()
        );
    }

    SQLite::Statement checkPackagesViolatingLT(db, "SELECT installed_package_dependencies.*, installed_packages.version_number installed_version_number FROM installed_packages JOIN installed_package_dependencies ON installed_packages.package_id=installed_package_dependencies.package_id WHERE version_comparison_type=? AND installed_packages.version_number >= installed_package_dependencies.version_number");
    checkPackagesViolatingLT.bind(1, static_cast<int>(VersionComparisonType::LT));
    while (checkPackagesViolatingLT.executeStep()) {
        conflicts = true;
        Log::E("Package %s@%s conflicts with %s@%s (which depends on %s)",
            checkPackagesViolatingLT.getColumn("package_id").getString().c_str(),
            checkPackagesViolatingLT.getColumn("installed_version_number").getString().c_str(),
            checkPackagesViolatingLT.getColumn("dependent_package_id").getString().c_str(),
            checkPackagesViolatingLT.getColumn("dependent_version_number").getString().c_str()
        );
    }

    return conflicts;
}