#pragma once
#include <string>
#include <sqlite3.h>

struct Repository {
    std::string id;
    std::string uri;
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
        ~Database();

        void mustRunQuery(const std::string& query);

        std::string* GetRepositories();
    private:
    const std::string repoTableName = "repos";
    const std::string packageIndexTableName = "package_index";
    const std::string installedPackageTableName = "installed_packages";

    sqlite3* database;
};