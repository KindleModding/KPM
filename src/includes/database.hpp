#pragma once
#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Transaction.h"
#include <string>
#include <vector>

struct Repository {
    std::string id;
    std::string url;
};

struct Package {
    std::string id;
    std::string alias;
    std::string repository_id;
    std::string name;
    std::string description;
    std::string screenshots;
};

struct PackageVersion {
    std::string package_id;
    std::string repository_id;
    uint version_number;
    std::string version_name;
    std::string architecture;
    std::string min_firmware;
    std::string max_firmware;
};

struct PackageVersionDependency {
    std::string dependent_package_id;
    std::string dependent_repository_id;
    uint dependent_version_number;
    std::string dependent_architecture;
    std::string package_id;
    std::string repository_id;
    uint version_number;
    std::string version_comparison;
};

struct InstalledPackage {
    std::string package_id;
    std::string repository_id;
    std::string name;
    std::string description;
    std::string screenshots;
    std::string version_name;
    uint version_number;
    std::string min_firmware;
    std::string max_firmware;
};

struct PackageWithVersion {
    std::string id;
    std::string alias;
    std::string repository_id;
    std::string name;
    std::string description;
    std::string screenshots;
    std::string package_id;
    uint version_number;
    std::string version_name;
    std::string architecture;
    std::string min_firmware;
    std::string max_firmware;
};

class Database {
    public:
        Database(std::string path);
        
        Database(Database& database) = delete;
        Database operator = (const Database&) = delete;

        void Begin();
        void End() { End(false); }
        void End(bool rollback);

        std::vector<Repository> GetRepositories();
        Repository GetRepository(const std::string& id);
        int AddRepository(Repository repository);
        int DeleteRepository(const std::string& id);

        int DeleteRepositoryPackages(const std::string& id);

        // Convert a version name to a version number by checking the index
        uint ConvertVersionNameToNumber(const std::string& package_id, const std::string& repository_id, const std::string& version_name);

        // Get possible package versions to install given a set of constraints
        std::vector<PackageWithVersion> GetCompatiblePackageVersions(const std::string& package_id, const std::string& repository_id, const uint& version_number, const std::string& version_comparison);
        
        std::vector<PackageWithVersion> SearchCompatiblePackages(const std::string& queryString);
        
        int AddPackage(Package package);

        int AddPackageVersion(PackageVersion version);

        int AddPackageVersionDependency(PackageVersionDependency packageVersionDependency);

        // Get dependencies for a specific package version
        std::vector<PackageVersionDependency> GetPackageVersionDependencies(const PackageVersion& version);

        std::vector<PackageVersionDependency> GetRequiredDependencies(); // Get dependencies required by installed packages

    private:
        SQLite::Database db;
        bool isTransaction = false;
};