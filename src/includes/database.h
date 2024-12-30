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
    std::string architecture;
    std::string version_name;
    uint version_number;
    uint min_firmware;
    uint max_firmware;
    std::string screenshots;
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
    private:
    const std::string repoTableName = "repos";
    const std::string packageIndexTableName = "package_index";
    const std::string packagesInstalledTableName = "installed_packages";

    SQLite::Database db;
};