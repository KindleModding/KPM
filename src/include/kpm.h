#pragma once


#include <filesystem>
#include <string>
#include <vector>
#include <sqlite3.h>

#include "semver.h"

enum class DependencyType
{
    NONE, // No dependency
    LESS_THAN,
    LESS_THAN_OR_EQUAL_TO,
    EQUAL_TO,
    GREATER_THAN_OR_EQUAL_TO,
    GREATER_THAN
};

struct Repository
{
    std::string id;
    std::string url;
};

struct IndexedRepository
{
    std::string id;
    std::string name;
    std::string description;
};

struct IndexedPackage
{
    std::string repository;
    std::string id;
    std::string name;
    std::string description;
    std::string author;
    std::string icon;
};

struct IndexedArtifact
{
    std::string url; // URL of the artifact - primary key
    std::string repository;
    std::string id;
    int version_major;
    int version_minor;
    int version_patch;
    std::vector<std::string> supported_arch;
    std::vector<std::string> supported_kindles;
};

struct IndexedArtifactDependency
{
    std::string artifact; // URL of the artifact
    std::string repository;
    std::string id;
    std::string type;
    SemVer version;
};

struct InstalledPackage
{
    std::string id;
    std::string name;
    std::string author;
    SemVer version;
};

struct Dependency
{
    std::string dependent; // ID of installed package
    std::string dependency; // Name of installed package
    DependencyType dependency_type;
    SemVer version;
};


/**
 * @brief This is the KPM class you initialise to interact with KPM
 * 
 */
class KPM
{
public:
    KPM(const std::filesystem::path dbPath)
    {
        sqlite3_open(dbPath.c_str(), &db);

        // Create tables if needed
        sqlite3_exec(db, R"(
            CREATE TABLE IF NOT EXISTS repositories (
                id TEXT PRIMARY KEY NOT NULL,
                url TEXT NOT NULL
            )
        )", NULL, NULL, NULL);

        sqlite3_exec(db, R"(
            CREATE TABLE IF NOT EXISTS repository_index (
                id TEXT PRIMARY KEY NOT NULL,
                name TEXT NOT NULL,
                description TEXT NOT NULL,
                FOREIGN KEY(id) REFERENCES repositories(id)
            )
        )", NULL, NULL, NULL);

        sqlite3_exec(db, R"(
            CREATE TABLE IF NOT EXISTS package_index (
                repository TEXT NOT NULL,
                id TEXT NOT NULL,
                name TEXT NOT NULL,
                description TEXT NOT NULL,
                author TEXT NOT NULL,
                icon TEXT NOT NULL,
                PRIMARY KEY (repository, id),
                FOREIGN KEY(repository) REFERENCES repositories(id)
            )
        )", NULL, NULL, NULL);

        sqlite3_exec(db, R"(
            CREATE TABLE IF NOT EXISTS artifact_index (
                url TEXT PRIMARY KEY NOT NULL,
                repository TEXT NOT NULL,
                id TEXT NOT NULL,
                version_major INTEGER NOT NULL,
                version_minor INTEGER NOT NULL,
                version_patch INTEGER NOT NULL,
                supported_arch TEXT NOT NULL,
                supported_kindles TEXT NOT NULL
            )
        )", NULL, NULL, NULL);

        sqlite3_exec(db, R"(
            CREATE TABLE IF NOT EXISTS artifact_dependency_index (
                artifact TEXT PRIMAR KEY NOT NULL,
                repository TEXT,
                id TEXT NOT NULL,
                type TEXT NOT NULL,
                version_major INTEGER,
                version_minor INTEGER,
                version_patch INTEGER,
                FOREIGN KEY(artifact) REFERENCES artifact_index(url)
            )
        )", NULL, NULL, NULL);

        sqlite3_exec(db, R"(
            CREATE TABLE IF NOT EXISTS installed_packages (
                id TEXT PRIMARY KEY NOT NULL,
                name TEXT NOT NULL,
                author TEXT NOT NULL,
                description TEXT NOT NULL,
                version_major INTEGER,
                version_minor INTEGER,
                version_patch INTEGER
            )
        )", NULL, NULL, NULL);

        sqlite3_exec(db, R"(
            CREATE TABLE IF NOT EXISTS dependencies (
                dependent TEXT NOT NULL,
                dependency TEXT NOT NULL,
                dependency_type TEXT NOT NULL,
                version_major INTEGER NOT NULL,
                version_minor INTEGER NOT NULL,
                version_patch INTEGER NOT NULL,
                FOREIGN KEY(dependent) REFERENCES installed_packages(id)
            )
        )", NULL, NULL, NULL);
    };

    ~KPM()
    {
        sqlite3_close(db);
    }

    // Repo management functions
    Repository AddRepository(const std::string& url);
    bool RemoveRepository(const std::string& id);
    std::vector<Repository> GetRepositories();
    std::vector<Package> GetRepositoryPackages(Repository repository);
    std::vector<Artifact> GetPackageArtifacts(Repository repository, Package package);

    // Internet functions
    bool UpdateIndices();
    bool DownloadPackage(const std::string& installString, const std::filesystem::path& path); // installString is generally a package ID but MAY contain a repo prefix
    

    // Local package functions
    std::vector<Artifact> GetInstalledPackages();
    bool InstallPackage(const std::filesystem::path& package);
    bool UninstallPackage(std::string id);
private:
    sqlite3* db;
};