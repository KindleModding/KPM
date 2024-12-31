#pragma once
#include "SQLiteCpp/Database.h"
#include <string>
#include <vector>

struct Repository {
    std::string id;
    std::string url;
};

struct Package {
    std::string id;
    std::string alias;
    std::string repo_id;
    std::string name;
    std::string description;
    std::string screenshots;
};

struct PackageVersion {
    std::string package_id;
    std::string repo_id;
    uint version_number;
    std::string version_name;
    std::string architecture;
    std::string min_firmware;
    std::string max_firmware;
};

struct PackageVersionDependency {
    std::string package_id;
    std::string repo_id;
    uint version_number;
    std::string architecture;
    std::string dependency_install_string;
};

struct InstalledPackage {
    std::string package_id;
    std::string repo_id;
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
    std::string repo_id;
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

        std::vector<Repository> GetRepositories();
        Repository GetRepository(const std::string& id);
        int AddRepository(Repository repository);
        int DeleteRepository(const std::string& id);

        int DeleteRepositoryPackages(const std::string& id);
        std::vector<Package> GetRepositoryPackages(const std::string& id);

        std::vector<PackageWithVersion> SearchCompatiblePackages(const std::string& queryString);
        std::vector<PackageWithVersion> GetCompatiblePackageVersions(const std::string& package_id, const std::string& repo_id, const std::string& version_name, const std::string& version_comparison);
        int AddPackage(Package package);

        std::vector<PackageVersion> GetPackageVersions(const Package& package);
        int AddPackageVersion(PackageVersion version);

        int AddPackageVersionDependency(PackageVersionDependency packageVersionDependency);
        std::vector<PackageVersionDependency> GetPackageVersionDependencies(const PackageVersion& version);
    private:
        SQLite::Database db;
};