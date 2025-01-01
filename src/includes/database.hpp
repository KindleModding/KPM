#pragma once
#include "SQLiteCpp/Database.h"
#include "SQLiteCpp/Transaction.h"
#include <string>
#include <vector>

enum class VersionComparisonType {
    NONE=-1, // No version check
    EQ=0, // =
    LT, // <
    GT, // >
    GTEQ, // >=
    LTEQ // <=
};

struct ParsedPackageTarget {
    std::string repository_id;
    std::string package_name;
    std::string version_name;
    VersionComparisonType version_comparison_type;
};

struct Repository {
    std::string id;
    std::string url;
};

struct Package {
    std::string id;
    std::string alias;
    std::string repository_id;
    std::string display_name;
    std::string description;
    std::string screenshots;
};

struct PackageVersion {
    std::string package_id;
    std::string repository_id;
    std::string version_name;
    uint version_number;
    std::string architecture;
    std::string min_firmware;
    std::string max_firmware;
};

struct PackageDependency {
    std::string dependent_package_id;
    std::string dependent_repository_id;
    uint dependent_version_number;
    std::string dependent_architecture;
    std::string repository_id;
    std::string package_name;
    std::string version_name;
    VersionComparisonType version_comparison_type;
};

struct InstalledPackage {
    std::string package_id;
    std::string repository_id;
    std::string display_name;
    std::string description;
    std::string screenshots;
    std::string version_name;
    uint version_number;
    std::string min_firmware;
    std::string max_firmware;
};

struct PackageInstallCandidate {
    std::string package_id;
    std::string alias;
    std::string repository_id;
    std::string repository_url;
    std::string display_name;
    std::string description;
    std::string screenshots;
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
        
        void AddPackage(const Package& package);
        void AddPackageVersion(const PackageVersion& packageVersion);
        void AddPackageDependency(const PackageDependency& PackageDependency);

        std::vector<PackageInstallCandidate> FindInstallationCandidates(const ParsedPackageTarget& parsedTarget);

        std::vector<PackageDependency> GetPackageDependencies(const PackageVersion& package);
        InstalledPackage GetInstalledPackage(const std::string& package_id);
        std::vector<PackageDependency> GetInstalledPackageDependenciesFromDependencyID(const std::string& package_id, const std::string& package_alias);
        void InstallPackage(PackageInstallCandidate package);
    private:
        SQLite::Database db;
        bool isTransaction = false;
};