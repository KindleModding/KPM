#pragma once

/**
 * @file kpm.h
 * @author Hackerdude (hackerdude@hackerdude.tech)
 * @brief The main KPM header
 * @version 0.1
 * @date 2025-08-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include <filesystem>
#include <string>
#include <vector>
#include <sqlite3.h>

#include "semver.hpp"

namespace KPM
{

/**
* @brief Type of dependency version
* 
*/
enum class DependencyType
{
    NONE, /**< No version-specific dependency */
    LESS_THAN, /**< Installed package must be less than dependent version */
    LESS_THAN_OR_EQUAL_TO, /**< Installed package must be greater than or equal to dependent version */
    EQUAL_TO, /**< Installed package must be equal to dependent version */
    GREATER_THAN_OR_EQUAL_TO, /**< Installed package must be greater than or equal to dependent version */
    GREATER_THAN /**< Installed package must be greater than dependent version */
};

/**
* @brief A repository that KPM is using
* 
*/
struct Repository
{
    std::string id; /**< The repository's ID */
    std::string url; /**< The url of the repository */
    std::string name; /**< The repository's name */
    std::string description; /**< The repository's description */
};

/**
* @brief A package that KPM has indexed
* 
*/
struct IndexedPackage
{
    std::string repository; /**< The repository ID */
    std::string id; /**< The package ID */
    std::string name; /**< The name of the package */
    std::string description; /**< The description of the package */
    std::string author; /**< The author of the package */
    std::string icon; /**< The package's icon */
};

/**
* @brief An artifact KPM has indexed
* 
*/
struct IndexedArtifact
{
    std::string url; /**< URL of the artifact - primary key */
    std::string repository; /**< The repository ID */
    std::string id; /**< The package ID */
    SemVer version; /**< The version of this artifact */
    std::vector<std::string> supported_arch; /**< List of supported architectures */
    std::vector<std::string> supported_kindles; /**< List of supported Kindles */
};

/**
* @brief A dependency of an artifact KPM has indexed
* 
*/
struct IndexedArtifactDependency
{
    std::string artifact; /**< URL of the artifact */
    std::string repository; /**< The repository ID */
    std::string id; /**< The package ID */
    DependencyType type; /**< The type of dependency */
    SemVer version; /**< The version of the dependency */
};

/**
* @brief A package KPM has installed
* 
*/
struct InstalledPackage
{
    std::string id; /**< The package ID */
    std::string name; /**< The package name */
    std::string author; /**< The package author */
    std::string description; /**< The package description */
    SemVer version; /**< The package version */
};

/**
* @brief A dependency of a package KPM has installed
* 
*/
struct Dependency
{
    std::string dependent; /**< ID of installed package */
    std::string dependency_repository; /**< ID of the repository of the dependency */
    std::string dependency_id; /**< ID of the dependency */
    DependencyType dependency_type; /**< The type of dependency */
    SemVer version; /**< The dependency's version */
};

namespace Exceptions
{
    class HTTPError: public std::exception
    {
        virtual const char* what() const throw()
        {
            return "Connection error.";
        }
    };

    class ManifestError: public std::exception
    {
        virtual const char* what() const throw()
        {
            return "Manifest format error.";
        }
    };

    class SQLiteError: public std::exception
    {
        virtual const char* what() const throw()
        {
            return "Unknown SQLite error.";
        }
    };
};

/**
 * @brief This is the KPM class you initialise to interact with KPM
 * 
 */
class KPM
{
public:
    KPM(const std::filesystem::path& dbPath = "./kpm.db");
    ~KPM();
    
    // Repo management functions
    std::vector<Repository> ListRepositories();
    Repository GetRepository(const std::string& repositoryID);
    Repository AddRepository(const std::string& url);
    void RemoveRepository(Repository repository);
    std::vector<IndexedPackage> ListRepositoryPackages(Repository repository);

    /*IndexedPackage GetPackage(const std::string& installString);
    std::vector<IndexedArtifact> GetArtifacts(IndexedPackage package);
    bool InstallArtifact(IndexedArtifact artifact);

    // Internet functions
    bool UpdateIndex();
    bool InstallPackage(const std::string& installString); // installString is generally a package ID but MAY contain a repo prefix

    // Local package functions
    std::vector<InstalledPackage> GetInstalledPackages();
    bool InstallPackage(const std::filesystem::path& package);
    bool UninstallPackage(InstalledPackage package);*/
private:
    sqlite3* db;
};

};