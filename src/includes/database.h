#pragma once
#include "SQLiteCpp/Database.h"
#include <string>
#include <sqlite3.h>
#include <vector>

struct Repository {
    std::string id;
    std::string url;
};

struct Package {
    std::string id;
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

class Database {
    public:
        Database(std::string path);

        std::vector<Repository> GetRepositories();
        Repository GetRepository(const std::string& id);
        int AddRepository(Repository repository);
        int DeleteRepository(const std::string& id);

        int DeleteRepositoryPackages(const std::string& id);
        std::vector<Package> GetRepositoryPackages(const std::string& id);
        
        Package GetPackage(const std::string& id);
        int AddPackage(Package package);
        int DeletePackage(const std::string& id);

        PackageVersion GetPackageVersion(const std::string& package_id);
        int AddPackageVersion(PackageVersion version);
        int DeletePackageVersions(const std::string& package_id, const std::string& repo_id);
    private:
        SQLite::Database db;
};