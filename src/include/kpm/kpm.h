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
#include <sys/types.h>

#include "semver.h"

#define KPM_MANIFEST_VERSION 3

#define KPM_VERSION_MAJOR 0
#define KPM_VERSION_MINOR 3
#define KPM_VERSION_PATCH 0

typedef enum
{
    KPM_OK,
    KPM_ABORTED,
    KPM_GENERIC_ERROR,
    KPM_SQLITE_ERROR,
    KPM_CURL_ERROR,
    KPM_INVALID_RESPONSE_CODE, /**< Non-200 response code from server */
    KPM_INVALID_RESPONSE_CONTENT, /**< Invalid content when parsing it (likely malformed JSON or repository manifest) */
    KPM_FILE_SYSTEM_ERROR,
    KPM_LIBARCHIVE_ERROR,
    KPM_PARSE_ERROR
} KPMResult;

const char* KPM_ErrorToString(KPMResult error);

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
typedef struct Repository Repository;

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
typedef struct IndexedPackage IndexedPackage;

/**
* @brief An artifact KPM has indexed
* 
*/
struct IndexedArtifact
{
    char* repository; /**< The repository ID */
    char* id; /**< The package ID */
    char* url; /**< URL of the artifact - primary key */
    SemVer version; /**< The version of this artifact */
};
typedef struct IndexedArtifact IndexedArtifact;


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
    SemVer min_version; /**< The min version of the dependency (inclusive) */
    SemVer max_version; /**< The max version of the dependency (exclusive) */
};
typedef struct ArtifactDependency ArtifactDependency;


/**
* @brief A package KPM has installed
* 
*/
struct InstalledPackage
{
    char* id; /**< The package ID */
    char* repository; /**< The repository this was installed from - may be NULL */
    char* name; /**< The package name */
    char* author; /**< The package author */
    char* description; /**< The package description */
    SemVer version; /**< The package version */
    bool installed_as_dependency; /**< Whether or not this package was installed as a dependency of another package */
};
typedef struct InstalledPackage InstalledPackage;


/**
* @brief A dependency of a package KPM has installed
* 
*/
struct InstalledDependency
{
    char* dependent; /**< ID of installed package */
    char* dependency_id; /**< ID of the dependency */
    SemVer min_version; /**< The min version of the dependency (inclusive) */
    SemVer max_version; /**< The max version of the dependency (exclusive) */
};
typedef struct InstalledDependency InstalledDependency;


struct InstallTarget
{
    char* repository; /**< The repository to look for the package in (or NULL) */
    char* id; /**< The id of the package to install */
    SemVer* version; /**< Note: may be NULL if version doesn't matter */
};
typedef struct InstallTarget InstallTarget;

struct KPM
{
    sqlite3* db; /**< The sqlite db object */
    char* dbPath; /**< The path to KPM's database */
    char* pkgPath; /**< The path to KPM packages */
    int maxConnections; /**< Maximum number of parallel connections to hold when downloading stuff */
};
typedef struct KPM KPM;

typedef enum
{
    KPM_VERBOSITY_DEBUG,
    KPM_VERBOSITY_INFO,
    KPM_VERBOSITY_WARN,
    KPM_VERBOSITY_ERROR
} KPMVerbosity;

typedef void KPMStream(char c);
typedef void KPMLog(KPMVerbosity, const char* format, ...) __attribute__((format(printf, 2, 3)));
typedef void KPMLogProgress(unsigned int progress, const char* format, ...) __attribute__((format(printf, 2, 3)));
typedef bool KPMGetInput(const char* format, ...) __attribute__((format(printf, 1, 2)));

struct KPMIO
{
    KPMLog* log;
    KPMStream* stream;
    KPMLogProgress* logProgress;
    KPMGetInput* getInput;
};
typedef struct KPMIO KPMIO;

/**
 * @brief Initialise the KPM object
 * 
 * @param kpm A pointer to an uninitialised KPM struct
 * @return KPMResult 
 */
KPMResult KPM_Initialise(KPM *kpm);

/**
 * @brief Cleanup a KPM object and free its resources
 * 
 * @param kpm 
 */
void KPM_Cleanup(KPM *kpm);

// Repo management functions
/**
 * @brief Free a repository object's properties (WILL NOT FREE THE POINTER ITSELF)
 * 
 * @param repository 
 */
void KPM_FreeRepository(Repository* repository);

/**
 * @brief Free an allocated repository list returned by KPM_ListRepositories
 * 
 * @param repositoryCount The number of repositories in the list
 * @param repositories The list of repositories
 */
void KPM_FreeRepositoryList(size_t repositoryCount, Repository* repositories);

/**
 * @brief List indexed repositories
 * 
 * @param kpm The KPM object
 * @param repositoryCount A pointer to store the number of repositories indexed
 * @param repositories A pointer to allocate and store store the indexed repository objects in (must be freed with KPM_FreeRepositoryList) - can be NULL to only get a count
 * @return KPMResult 
 */
KPMResult KPM_ListRepositories(KPM* kpm, size_t* repositoryCount, Repository** repositories);

/**
 * @brief Get a single indexed repository from its Id
 * 
 * @param kpm The KPM object
 * @param repositoryId The Id of the repository to get
 * @param repository A pointer to return the repository object (Values will be NULL if the repository could not be fetched, pointer can be set to NULL)
 * @return KPMResult 
 */
KPMResult KPM_GetRepository(KPM *kpm, const char *repositoryId, Repository* repository);

/**
 * @brief Index a repository from a URL
 * 
 * @param kpm The KPM object
 * @param url The URL to the repository manifest file
 * @param repository A pointer to return the indexed repository object (or NULL)
 * @param kpm_io A KPM IO object
 * @return KPMResult 
 */
KPMResult KPM_AddRepository(KPM *kpm, const char *url, Repository* repository, KPMIO* kpm_io);

/**
 * @brief Remove an indexed repository by its Id
 * 
 * @param kpm The KPM object
 * @param repositoryId The Id of the repository to remove
 * @return KPMResult 
 */
KPMResult KPM_RemoveRepository(KPM* kpm, const char* repositoryId);

/**
 * @brief Return a list of indexed packages under a repository
 * 
 * @param kpm The KPM object
 * @param repositoryId The Id of the repository to get packages for
 * @param packageCount A pointer to return the number of packages indexed
 * @param packages A pointer to allocate and return an array of packages - Must be freed with KPM_FreeIndexedPackageList - Can be NULL to return only a count
 * @return KPMResult 
 */
KPMResult KPM_ListRepositoryPackages(KPM* kpm, const char* repositoryId, size_t* packageCount, IndexedPackage** packages);



// Package management functions
/**
 * @brief Free the properties of a package
 * 
 * @param package The package to free the properties of
 */
void KPM_FreeIndexedPackage(IndexedPackage* package);

/**
 * @brief Free an allocated list of packages - such as returned by KPM_ListRepositoryPackages
 * 
 * @param packageCount The number of packages in the array
 * @param packages The package array
 */
void KPM_FreeIndexedPackageList(size_t packageCount, IndexedPackage* packages);

/**
 * @brief Get a package given a repositoryId (optional) and a packageId
 * 
 * @param kpm The KPM object
 * @param repository The id of the repository to get the package from (or NULL)
 * @param id The id of the package to get
 * @param package A pointer to write the returned package info
 * @return KPMResult 
 */
KPMResult KPM_GetPackage(KPM* kpm, const char* repository, const char* id, IndexedPackage* package);

/**
 * @brief Get a list of packages given a package id
 * 
 * @param kpm The KPM object
 * @param id The id of the package to get
 * @param packageCount The number of packages in the array
 * @param packages The package array
 * @return KPMResult 
 */
KPMResult KPM_GetPackages(KPM* kpm, const char* id, size_t* packageCount, IndexedPackage** packages);

/**
 * @brief Return a list of packages where either the name or id contain the query
 * 
 * @param kpm The KPM object
 * @param query The query to search for
 * @param packageCount The number of packages in the array
 * @param packages The package array
 * @return KPMResult 
 */
KPMResult KPM_SearchPackages(KPM* kpm, const char* query, size_t* packageCount, IndexedPackage** packages);

// Installed package management functions
/**
 * @brief Free an installed package
 * 
 * @param package The package to free
 */
void KPM_FreeInstalledPackage(InstalledPackage* package);

/**
 * @brief Free an allocated list of installed packages
 * 
 * @param packageCount The number of packages in the array
 * @param packages The package array
 */
void KPM_FreeInstalledPackageList(size_t packageCount, InstalledPackage* packages);

/**
 * @brief Get an installed package object by ID
 * 
 * @param kpm The KPM object
 * @param packageId The package ID
 * @param package The installed package object
 * @return KPMResult 
 */
KPMResult KPM_GetInstalledPackage(KPM* kpm, const char* packageId, InstalledPackage* package);

/**
 * @brief Get the list of installed packages
 * 
 * @param kpm The KPM object
 * @param packageCount The number of installed packages in the packages array
 * @param packages The packages array
 * @return KPMResult 
 */
KPMResult KPM_ListInstalledPackages(KPM* kpm, size_t* packageCount, InstalledPackage** packages);

// Installed dependency management functions
/**
 * @brief Free the properties of an installed dependency
 * 
 * @param dependency The installed dependency to free the properties of
 */
void KPM_FreeInstalledPackageDependency(InstalledDependency* dependency);
/**
 * @brief Free an allocated list of installed dependencies
 * 
 * @param dependencyCount The number of installed dependencies in the array
 * @param dependency The installed dependency array
*/
void KPM_FreeInstalledPackageDependencyList(size_t dependencyCount, InstalledDependency* dependency);

/**
 * @brief List dependencies for an installed package
 * 
 * @param kpm The KPM object
 * @param id The id of the installed package
 * @param dependencyCount The number of dependencies in the dependency array
 * @param dependencies The dependency array
 * @return KPMResult 
 */
KPMResult KPM_ListInstalledPackageDependencies(KPM* kpm, const char* id, size_t* dependencyCount, InstalledDependency** dependencies);

/**
 * @brief Get the list of dependent packages for a given package id
 * 
 * @param kpm The KPM object
 * @param id The id of the package to check
 * @param dependentCount The number of dependent packages in the packages array
 * @param dependents The dependent packages array
 * @return KPMResult 
 */
KPMResult KPM_ListInstalledPackageDependents(KPM* kpm, const char* id, size_t* dependentCount, InstalledDependency** dependents);

// Artifact management functions
/**
 * @brief Free the properties of an indexed artifact
 * 
 * @param artifact The artifact to free the properties of
 */
void KPM_FreeIndexedArtifact(IndexedArtifact* artifact);

/**
 * @brief Free an allocated list of indexed artifact
 * 
 * @param artifactCount The number of indexed artifacts in the array
 * @param artifacts The indexed artifact array
*/
void KPM_FreeIndexedArtifactList(size_t artifactCount, IndexedArtifact* artifacts);

/**
 * @brief Get an IndexedArtifact
 * 
 * @param kpm The KPM object
 * @param repositoryId The repository of the artifact to get (or NULL)
 * @param packageId The id of the package to get an artifact for
 * @param version The version to get the artifact for
 * @param artifact The indexed artifact
 * @return KPMResult 
 */
KPMResult KPM_GetArtifact(KPM* kpm, const char* repositoryId, const char* packageId, SemVer version, IndexedArtifact* artifact);

/**
 * @brief List the indexed artifacts of a package 
 * 
 * @param kpm The KPM object
 * @param repositoryId The repository ID of the package
 * @param packageId The package ID of the package
 * @param artifactCount Pointer to store the artifact count
 * @param artifacts Pointer to store the artifact array
 * @return KPMResult 
 */
KPMResult KPM_ListPackageArtifacts(KPM* kpm, const char* repositoryId, const char* packageId, size_t* artifactCount, IndexedArtifact** artifacts);

// Dependency management functions
/**
 * @brief Free the properties of an artifact dependency
 * 
 * @param dependency The artifact dependency to free the properties of
 */
void KPM_FreeArtifactDependency(ArtifactDependency* dependency);
/**
 * @brief Free an allocated list of artifact dependencies
 * 
 * @param dependencyCount The number of artifact dependencies in the array
 * @param dependencies The artifact dependency array
*/
void KPM_FreeArtifactDependencyList(size_t dependencyCount, ArtifactDependency* dependencies);

/**
 * @brief Get a list of dependencies for a given artifact
 * 
 * @param kpm The KPM object
 * @param repository The repository ID of the artifact
 * @param id The package ID of the artifact
 * @param url The url of the artifact
 * @param dependencyCount The number of dependencies in the dependency array
 * @param dependencies The dependency array
 * @return KPMResult 
 */
KPMResult KPM_ListArtifactDependencies(KPM* kpm, const char* repository, const char* id, const char* url, size_t* dependencyCount, ArtifactDependency** dependencies);

/**
 * @brief Update the local index of package by downloading repository manifests
 * 
 * @param kpm The KPM object
 * @param statusCallback A callback for progress information
 * @return KPMResult 
 */
KPMResult KPM_UpdateIndex(KPM *kpm, KPMIO* kpmIO);


/**
 * @brief Free an installation target object
 * 
 * @param target The installation target object to free
 */
void KPM_FreeInstallTarget(InstallTarget* target);

/**
 * @brief Frees a list of InstallTarget objects
 * 
 * @param targetCount The number of InstallTarget objects
 * @param targets The list of InstallTarget objects
 */
void KPM_FreeInstallTargetList(size_t targetCount, InstallTarget* targets);


/**
 * @brief Installs/upgrades a target package and its dependencies
 * 
 * @param kpm 
 * @param targetCount The number of packages to install
 * @param targets An array of packages to install
 * @param kpmIO 
 * @return KPMResult 
 */
KPMResult KPM_InstallPackages(KPM* kpm, size_t targetCount, InstallTarget* targets, KPMIO* kpmIO);

/**
 * @brief Uninstalls a package
 * 
 * @param kpm 
 * @param packageCount
 * @param packageIds 
 * @param kpmIO 
 * @return KPMResult 
 */
KPMResult KPM_UninstallPackages(KPM* kpm, size_t packageCount, const char* packageIds[], KPMIO* kpmIO);