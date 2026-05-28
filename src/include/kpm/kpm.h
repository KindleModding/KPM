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

#include <sqlite3.h>
#include <stddef.h>
#include <sys/types.h>

#include "semver.h"

enum KPMResult
{
    KPM_OK,
    KPM_GENERIC_ERROR,
    KPM_SQLITE_ERROR,
    KPM_CURL_ERROR,
    KPM_INVALID_RESPONSE_CODE, /**< Non-200 response code from server */
    KPM_INVALID_RESPONSE_CONTENT, /**< Invalid content when parsing it (likely malformed JSON or repository manifest) */
    KPM_FILE_SYSTEM_ERROR,
    KPM_LIBARCHIVE_ERROR,
    KPM_PARSE_ERROR
};

/**
* @brief A repository that KPM is using
* 
*/
struct Repository
{
    char* id; /**< The repository's ID */
    char* url; /**< The url of the repository */
    char* name; /**< The repository's name */
    char* description; /**< The repository's description */
};

/**
* @brief A package that KPM has indexed
* 
*/
struct IndexedPackage
{
    char* repository; /**< The repository ID */
    char* id; /**< The package ID */
    char* name; /**< The name of the package */
    char* author; /**< The author of the package */
    char* description; /**< The description of the package */
};

/**
* @brief An artifact KPM has indexed
* 
*/
struct IndexedArtifact
{
    char* repository; /**< The repository ID */
    char* id; /**< The package ID */
    char* url; /**< URL of the artifact - primary key */
    struct SemVer version; /**< The version of this artifact */
};

/**
* @brief A dependency object
* 
*/
struct ArtifactDependency
{
    char* artifact_repository; /**< Repository of the artifact */
    char* artifact_id; /**< ID of the artifact */
    char* artifact_url; /**< URL of the artifact */
    char* id; /**< The package ID */
    struct SemVer min_version; /**< The min version of the dependency (inclusive) */
    struct SemVer max_version; /**< The max version of the dependency (exclusive) */
};

/**
* @brief A package KPM has installed
* 
*/
struct InstalledPackage
{
    char* id; /**< The package ID */
    char* repository; /**< The repository this was installed from - empty is locally installed from a file */
    char* name; /**< The package name */
    char* author; /**< The package author */
    char* description; /**< The package description */
    struct SemVer version; /**< The package version */
};

/**
* @brief A dependency of a package KPM has installed
* 
*/
struct InstalledDependency
{
    char* dependent; /**< ID of installed package */
    char* dependency_id; /**< ID of the dependency */
    struct SemVer min_version; /**< The min version of the dependency (inclusive) */
    struct SemVer max_version; /**< The max version of the dependency (exclusive) */
};



struct InstallTarget
{
    char* repository; /**< The repository to look for the package in (or NULL) */
    char* id; /**< The id of the package to install */
    struct SemVer* version; /**< Note: may be NULL if version doesn't matter */
};

struct KPM
{
    sqlite3* db; /**< The sqlite db object */
    char* pkgPath; /**< The path to KPM packages */
    bool prompt; /**< Should prompt user for things */
    bool confirmInstall; /**< Should confirm install */
    int maxConnections; /**< Maximum number of parallel connections to hold when downloading stuff */
};

enum Verbosity
{
    KPM_VERBOSITY_DEBUG,
    KPM_VERBOSITY_INFO,
    KPM_VERBOSITY_WARN,
    KPM_VERBOSITY_ERROR
};

typedef void KPMStream(char c);
typedef void KPMLog(enum Verbosity, char* details, ...);
typedef void KPMLogProgress(uint progress, char* details, ...);
typedef bool KPMGetInput(char* details, ...);

struct KPMLogging
{
    KPMLog* log;
    KPMStream* stream;
    KPMLogProgress* logProgress;
    KPMGetInput* getInput;
};

/**
 * @brief Initialise the KPM object
 * 
 * @param kpm A pointer to an uninitialised KPM struct
 * @param dbPath The path to the KPM database file
 * @return enum KPMResult 
 */
enum KPMResult KPM_Initialise(struct KPM *kpm, const char* dbPath);

/**
 * @brief Cleanup a KPM object and free its resources
 * 
 * @param kpm 
 */
void KPM_Cleanup(struct KPM *kpm);

// Repo management functions
/**
 * @brief Free a repository object's properties (WILL NOT FREE THE POINTER ITSELF)
 * 
 * @param repository 
 */
void KPM_FreeRepository(struct Repository* repository);

/**
 * @brief Free an allocated repository list returned by KPM_ListRepositories
 * 
 * @param repositoryCount The number of repositories in the list
 * @param repositories The list of repositories
 */
void KPM_FreeRepositoryList(size_t repositoryCount, struct Repository* repositories);

/**
 * @brief List indexed repositories
 * 
 * @param kpm The KPM object
 * @param repositoryCount A pointer to store the number of repositories indexed
 * @param repositories A pointer to allocate and store store the indexed repository objects in (must be freed with KPM_FreeRepositoryList) - can be NULL to only get a count
 * @return enum KPMResult 
 */
enum KPMResult KPM_ListRepositories(struct KPM* kpm, size_t* repositoryCount, struct Repository** repositories);

/**
 * @brief Get a single indexed repository from its Id
 * 
 * @param kpm The KPM object
 * @param repositoryId The Id of the repository to get
 * @param repository A pointer to return the repository object (Values will be NULL if the repository could not be fetched)
 * @return enum KPMResult 
 */
enum KPMResult KPM_GetRepository(struct KPM *kpm, const char *repositoryId, struct Repository* repository);

/**
 * @brief Index a repository from a URL
 * 
 * @param kpm The KPM object
 * @param url The URL to the repository manifest file
 * @param repository A pointer to return the indexed repository object
 * @return enum KPMResult 
 */
enum KPMResult KPM_AddRepository(struct KPM *kpm, const char *url, struct Repository* repository);

/**
 * @brief Remove an indexed repository by its Id
 * 
 * @param kpm The KPM object
 * @param repositoryId The Id of the repository to remove
 * @return enum KPMResult 
 */
enum KPMResult KPM_RemoveRepository(struct KPM* kpm, const char* repositoryId);

/**
 * @brief Return a list of indexed packages under a repository
 * 
 * @param kpm The KPM object
 * @param repositoryId The Id of the repository to get packages for
 * @param packageCount A pointer to return the number of packages indexed
 * @param packages A pointer to allocate and return an array of packages - Must be freed with KPM_FreeIndexedPackageList - Can be NULL to return only a count
 * @return enum KPMResult 
 */
enum KPMResult KPM_ListRepositoryPackages(struct KPM* kpm, const char* repositoryId, size_t* packageCount, struct IndexedPackage** packages);



// Package management functions
/**
 * @brief Free the properties of a package - WILL NOT FREE THE POINTER ITSELF
 * 
 * @param package The package to free the properties of
 */
void KPM_FreeIndexedPackage(struct IndexedPackage* package);

/**
 * @brief Free an allocated list of packages - such as returned by KPM_ListRepositoryPackages
 * 
 * @param packageCount The number of packages in the array
 * @param packages The package array
 */
void KPM_FreeIndexedPackageList(size_t packageCount, struct IndexedPackage* packages);

/**
 * @brief Get a package given a repositoryId (optional) and a packageId
 * 
 * @param kpm The KPM object
 * @param repository The id of the repository to get the package from (or NULL)
 * @param id The id of the package to get
 * @param package A pointer to write the returned package info
 * @return enum KPMResult 
 */
enum KPMResult KPM_GetPackage(struct KPM* kpm, const char* repository, const char* id, struct IndexedPackage* package);

/**
 * @brief Get a list of packages given a package id
 * 
 * @param kpm The KPM object
 * @param id The id of the package to get
 * @param packageCount The number of packages in the array
 * @param packages The package array
 * @return enum KPMResult 
 */
enum KPMResult KPM_GetPackages(struct KPM* kpm, const char* id, size_t* packageCount, struct IndexedPackage** packages);

/**
 * @brief Return a list of packages where either the name or id contain the query
 * 
 * @param kpm The KPM object
 * @param query The query to search for
 * @param packageCount The number of packages in the array
 * @param packages The package array
 * @return enum KPMResult 
 */
enum KPMResult KPM_SearchPackages(struct KPM* kpm, const char* query, size_t* packageCount, struct IndexedPackage** packages);

// Installed package management functions
void KPM_FreeInstalledPackage(struct InstalledPackage* package);
void KPM_FreeInstalledPackageList(size_t packageCount, struct InstalledPackage* packages);

/**
 * @brief Get an installed package object by ID
 * 
 * @param kpm The KPM object
 * @param packageId The package ID
 * @param package The installed package object
 * @return enum KPMResult 
 */
enum KPMResult KPM_GetInstalledPackage(struct KPM* kpm, const char* packageId, struct InstalledPackage* package);

/**
 * @brief Get the list of installed packages
 * 
 * @param kpm The KPM object
 * @param packageCount The number of installed packages in the packages array
 * @param packages The packages array
 * @return enum KPMResult 
 */
enum KPMResult KPM_ListInstalledPackages(struct KPM* kpm, size_t* packageCount, struct InstalledPackage** packages);

// Installed dependency management functions
void KPM_FreeInstalledPackageDependency(struct InstalledDependency* dependency);
void KPM_FreeInstalledPackageDependencyList(size_t artifactCount, struct InstalledDependency* dependency);

/**
 * @brief List dependencies for an installed package
 * 
 * @param kpm The KPM object
 * @param id The id of the installed package
 * @param dependencyCount The number of dependencies in the dependency array
 * @param dependencies The dependency array
 * @return enum KPMResult 
 */
enum KPMResult KPM_ListInstalledPackageDependencies(struct KPM* kpm, char* id, size_t* dependencyCount, struct InstalledDependency** dependencies);

// Artifact management functions
/**
 * @brief Free the properties of a package - WILL NOT FREE THE POINTER ITSELF
 * 
 * @param package The package to free the properties of
 */
void KPM_FreeIndexedArtifact(struct IndexedArtifact* artifact);

/**
 * @brief Free an allocated list of packages - such as returned by KPM_ListRepositoryPackages
 * 
 * @param packageCount The number of packages in the array
 * @param packages The package array
*/
void KPM_FreeIndexedArtifactList(size_t artifactCount, struct IndexedArtifact* artifacts);

/**
 * @brief Get an IndexedArtifact
 * 
 * @param kpm The KPM object
 * @param repositoryId The repository of the artifact to get (or NULL)
 * @param packageId The id of the package to get an artifact for
 * @param version The version to get the artifact for
 * @param artifact The indexed artifact
 * @return enum KPMResult 
 */
enum KPMResult KPM_GetArtifact(struct KPM* kpm, const char* repositoryId, const char* packageId, struct SemVer version, struct IndexedArtifact* artifact);

/**
 * @brief List the indexed artifacts of a package 
 * 
 * @param kpm The KPM object
 * @param repositoryId The repository ID of the package
 * @param packageId The package ID of the package
 * @param artifactCount Pointer to store the artifact count
 * @param artifacts Pointer to store the artifact array
 * @return enum KPMResult 
 */
enum KPMResult KPM_ListPackageArtifacts(struct KPM* kpm, const char* repositoryId, const char* packageId, size_t* artifactCount, struct IndexedArtifact** artifacts);

// Dependency management functions
void KPM_FreeArtifactDependency(struct ArtifactDependency* dependency);
void KPM_FreeArtifactDependencyList(size_t artifactCount, struct ArtifactDependency* dependency);

/**
 * @brief Get a list of dependencies for a given artifact
 * 
 * @param kpm The KPM object
 * @param repository The repository ID of the artifact
 * @param id The package ID of the artifact
 * @param url The url of the artifact
 * @param dependencyCount The number of dependencies in the dependency array
 * @param dependencies The dependency array
 * @return enum KPMResult 
 */
enum KPMResult KPM_ListArtifactDependencies(struct KPM* kpm, char* repository, char* id, char* url, size_t* dependencyCount, struct ArtifactDependency** dependencies);

/**
 * @brief Update the local index of package by downloading repository manifests
 * 
 * @param kpm The KPM object
 * @param statusCallback A callback for progress information
 * @return enum KPMResult 
 */
enum KPMResult KPM_UpdateIndex(struct KPM *kpm, struct KPMLogging* kpmLogging);


/**
 * @brief Free an installation target object
 * 
 * @param target The installation target object to free
 */
void KPM_FreeInstallTarget(struct InstallTarget* target);



/**
 * @brief Installs/upgrades a target package and its dependencies
 * 
 * @param kpm 
 * @param target 
 * @param kpmLogging 
 * @return enum KPMResult 
 */
enum KPMResult KPM_InstallPackage(struct KPM* kpm, struct InstallTarget* target, struct KPMLogging* kpmLogging);