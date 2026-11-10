#ifndef KPM_H
#define KPM_H

/**
 * @file kpm.h
 * @author Hackerdude (hackerdude@hackerdude.tech)
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include <sqlite3.h>
#include <sys/types.h>

#include "kpkg.h"
#include "semver.h"
#include "kpmio.h"

#define KPM_MANIFEST_VERSION 3

#define KPM_VERSION_MAJOR 0
#define KPM_VERSION_MINOR 3
#define KPM_VERSION_PATCH 0

/**
 * @brief Return value for many KPM functions
 * 
 */
typedef enum
{
    KPM_OK,
    KPM_ABORTED,
    KPM_GENERIC_ERROR,
    KPM_SQLITE_ERROR,
    KPM_CURL_ERROR,
    KPM_INVALID_RESPONSE_CODE,      /**< Non-200 response code from server */
    KPM_INVALID_RESPONSE_CONTENT,   /**< Invalid content when parsing it */
    KPM_FILE_SYSTEM_ERROR,
    KPM_LIBARCHIVE_ERROR,
    KPM_PARSE_ERROR,
    KPM_RESOLUTION_ERROR            /**< BIG BAD! */
} KPM_Result;

const char *KPM_ErrorToString(KPM_Result error);

/**
* @brief A repository that KPM is using
* 
*/
typedef struct
{
    char *id; /**< The repository's ID */
    char *url; /**< The url of the repository */
    char *name; /**< The repository's name */
    char *description; /**< The repository's description */
} KPM_Repository;

/**
* @brief A package that KPM has indexed
* 
*/
typedef struct
{
    char *repository; /**< The repository ID */
    char *id; /**< The package ID */
    char *name; /**< The name of the package */
    char *author; /**< The author of the package */
    char *description; /**< The description of the package */
} KPM_IndexedPackage;

/**
* @brief An artifact KPM has indexed
* 
*/
typedef struct
{
    char *repository; /**< The repository ID */
    char *id; /**< The package ID */
    char *url; /**< URL of the artifact - primary key */
    SemVer version; /**< The version of this artifact */
} KPM_Artifact;


/**
* @brief A dependency object
* 
*/
typedef struct
{
    char *id; /**< The package ID */
    SemVer min_version; /**< The min version of the dependency (inclusive) */
    SemVer max_version; /**< The max version of the dependency (exclusive) */
} KPM_Dependency;


/**
* @brief A package KPM has installed
* 
*/
typedef struct
{
    char *id; /**< The package ID */
    char *repository; /**< The repository this was installed from - may be NULL */
    char *name; /**< The package name */
    char *author; /**< The package author */
    char *description; /**< The package description */
    SemVer version; /**< The package version */
    bool installed_as_dependency; /**< Whether or not this package was installed as a dependency of another package */
} KPM_InstalledPackage;


/**
 * @brief The source for an install target
 * 
 */
typedef enum {
    INSTALL_ARTIFACT,   /**< A remote artifact */
    INSTALL_FILE        /**< A local file */
} KPM_InstallTargetSource;

/**
 * @brief Represent an installation target
 * 
 */
typedef struct
{
    KPM_InstallTargetSource type; /**< The source of this target */
    union {
        KPM_Artifact artifact;  /**< The remote artifact for the package */
        KPKG_File *file;    /**< The local file for the package */
    };
} KPM_InstallTarget;

typedef struct
{
    sqlite3 *db;        /**< The sqlite db object */
    char *db_path;       /**< The path to KPM's database */
    char *pkg_path;      /**< The path to KPM packages */
    char *tmp_path;      /**< The path to store temporary files (ideally on same device as pkg_path) */
    int max_connections; /**< Maximum number of parallel connections to hold when downloading stuff */
} KPM;

char *KPM_GetInstallTargetId(KPM_InstallTarget target);
SemVer KPM_GetInstallTargetVersion(KPM_InstallTarget target);
int KPM_GetInstallTargetDependencies(KPM *kpm, KPM_InstallTarget target, KPM_Dependency **dependencies);

/**
 * @brief Initialise the KPM object
 * 
 * @param db_path Override the default database path (or null)
 * @param pkg_path Override the default pkg path (or null)
 * @return KPM* 
 */
KPM *KPM_InitialiseAdv(char *db_path, char *pkg_path, char *tmp_path);

/**
 * @brief Initialise the KPM object
 * 
 * @return KPM* 
 */
KPM *KPM_Initialise();

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
void KPM_FreeRepository(KPM_Repository *repository);

/**
 * @brief Free an allocated repository list returned by KPM_ListRepositories
 * 
 * @param repository_count The number of repositories in the list
 * @param repositories The list of repositories
 */
void KPM_FreeRepositoryList(size_t repository_count, KPM_Repository *repositories);

/**
 * @brief List indexed repositories
 * 
 * @param kpm The KPM object
 * @param repository_count A pointer to store the number of repositories indexed
 * @param repositories A pointer to allocate and store store the indexed repository objects in (must be freed with KPM_FreeRepositoryList) - can be NULL to only get a count
 * @return KPMResult 
 */
KPM_Result KPM_ListRepositories(KPM *kpm, size_t *repository_count, KPM_Repository **repositories);

/**
 * @brief Get a single indexed repository from its Id
 * 
 * @param kpm The KPM object
 * @param repository_id The Id of the repository to get
 * @param repository A pointer to return the repository object (Values will be NULL if the repository could not be fetched, pointer can be set to NULL)
 * @return KPMResult 
 */
KPM_Result KPM_GetRepository(KPM *kpm, const char *repository_id, KPM_Repository *repository);

/**
 * @brief Index a repository from a URL
 * 
 * @param kpm The KPM object
 * @param url The URL to the repository manifest file
 * @param repository A pointer to return the indexed repository object (or NULL)
 * @param kpm_io A KPM IO object
 * @return KPMResult 
 */
KPM_Result KPM_AddRepository(KPM *kpm, const char *url, KPM_Repository *repository, KPMIO *kpm_io);

/**
 * @brief Remove an indexed repository by its Id
 * 
 * @param kpm The KPM object
 * @param repository_id The Id of the repository to remove
 * @return KPMResult 
 */
KPM_Result KPM_RemoveRepository(KPM *kpm, const char *repository_id);

/**
 * @brief Return a list of indexed packages under a repository
 * 
 * @param kpm The KPM object
 * @param repository_id The Id of the repository to get packages for
 * @param package_count A pointer to return the number of packages indexed
 * @param packages A pointer to allocate and return an array of packages - Must be freed with KPM_FreeIndexedPackageList - Can be NULL to return only a count
 * @return KPMResult 
 */
KPM_Result KPM_ListRepositoryPackages(KPM *kpm, const char *repository_id, size_t *package_count, KPM_IndexedPackage **packages);



// Package management functions
/**
 * @brief Free the properties of a package
 * 
 * @param package The package to free the properties of
 */
void KPM_FreeIndexedPackage(KPM_IndexedPackage *package);

/**
 * @brief Free an allocated list of packages - such as returned by KPM_ListRepositoryPackages
 * 
 * @param package_count The number of packages in the array
 * @param packages The package array
 */
void KPM_FreeIndexedPackageList(size_t package_count, KPM_IndexedPackage *packages);

/**
 * @brief Get a package given a repository_id (optional) and a package_id
 * 
 * @param kpm The KPM object
 * @param repository The id of the repository to get the package from (or NULL)
 * @param id The id of the package to get
 * @param package A pointer to write the returned package info
 * @return KPMResult 
 */
KPM_Result KPM_GetPackage(KPM *kpm, const char *repository, const char *id, KPM_IndexedPackage *package);

/**
 * @brief Get a list of packages given a package id
 * 
 * @param kpm The KPM object
 * @param id The id of the package to get
 * @param package_count The number of packages in the array
 * @param packages The package array
 * @return KPMResult 
 */
KPM_Result KPM_GetPackages(KPM *kpm, const char *id, size_t *package_count, KPM_IndexedPackage **packages);

/**
 * @brief Return a list of packages where either the name or id contain the query
 * 
 * @param kpm The KPM object
 * @param query The query to search for
 * @param package_count The number of packages in the array
 * @param packages The package array
 * @return KPMResult 
 */
KPM_Result KPM_SearchPackages(KPM *kpm, const char *query, size_t *package_count, KPM_IndexedPackage **packages);

// Installed package management functions
/**
 * @brief Free an installed package
 * 
 * @param package The package to free
 */
void KPM_FreeInstalledPackage(KPM_InstalledPackage *package);

/**
 * @brief Free an allocated list of installed packages
 * 
 * @param package_count The number of packages in the array
 * @param packages The package array
 */
void KPM_FreeInstalledPackageList(size_t package_count, KPM_InstalledPackage *packages);

/**
 * @brief Get an installed package object by ID
 * 
 * @param kpm The KPM object
 * @param package_id The package ID
 * @param package The installed package object
 * @return KPMResult 
 */
KPM_Result KPM_GetInstalledPackage(KPM *kpm, const char *package_id, KPM_InstalledPackage *package);

/**
 * @brief Get the list of installed packages
 * 
 * @param kpm The KPM object
 * @param package_count The number of installed packages in the packages array
 * @param packages The packages array
 * @return KPMResult 
 */
KPM_Result KPM_ListInstalledPackages(KPM *kpm, size_t *package_count, KPM_InstalledPackage **packages);

/**
 * @brief List dependencies for an installed package
 * 
 * @param kpm The KPM object
 * @param id The id of the installed package
 * @param dependency_count The number of dependencies in the dependency array
 * @param dependencies The dependency array
 * @return KPMResult 
 */
KPM_Result KPM_ListInstalledPackageDependencies(KPM *kpm, const char *id, size_t *dependency_count, KPM_Dependency **dependencies);

/**
 * @brief Get the list of dependent packages for a given package id
 * 
 * @param kpm The KPM object
 * @param id The id of the package to check
 * @param dependent_count The number of dependent packages in the packages array
 * @param dependents The dependent packages array
 * @return KPMResult 
 */
KPM_Result KPM_ListInstalledPackageDependents(KPM *kpm, const char *id, size_t *dependent_count, KPM_Dependency **dependents);

// Artifact management functions
/**
 * @brief Free the properties of an indexed artifact
 * 
 * @param artifact The artifact to free the properties of
 */
void KPM_FreeArtifact(KPM_Artifact *artifact);

/**
 * @brief Free an allocated list of indexed artifact
 * 
 * @param artifact_count The number of indexed artifacts in the array
 * @param artifacts The indexed artifact array
*/
void KPM_FreeArtifactList(size_t artifact_count, KPM_Artifact *artifacts);

/**
 * @brief Get an Artifact
 * 
 * @param kpm The KPM object
 * @param repository_id The repository of the artifact to get (or NULL)
 * @param package_id The id of the package to get an artifact for
 * @param version The version to get the artifact for
 * @param artifact The indexed artifact
 * @return KPMResult 
 */
KPM_Result KPM_GetArtifact(KPM *kpm, const char *repository_id, const char *package_id, SemVer version, KPM_Artifact *artifact);

/**
 * @brief List the indexed artifacts of a package 
 * 
 * @param kpm The KPM object
 * @param repository_id The repository ID of the package (or NULL)
 * @param package_id The package ID of the package
 * @param artifact_count Pointer to store the artifact count
 * @param artifacts Pointer to store the artifact array
 * @return KPMResult 
 */
KPM_Result KPM_ListPackageArtifacts(KPM *kpm, const char *repository_id, const char *package_id, size_t *artifact_count, KPM_Artifact **artifacts);

// Dependency management functions
/**
 * @brief Free the properties of an artifact dependency
 * 
 * @param dependency The artifact dependency to free the properties of
 */
void KPM_FreeDependency(KPM_Dependency *dependency);
/**
 * @brief Free an allocated list of artifact dependencies
 * 
 * @param dependency_count The number of artifact dependencies in the array
 * @param dependencies The artifact dependency array
*/
void KPM_FreeDependencyList(size_t dependency_count, KPM_Dependency *dependencies);

/**
 * @brief Get a list of dependencies for a given artifact
 * 
 * @param kpm The KPM object
 * @param repository The repository ID of the artifact
 * @param id The package ID of the artifact
 * @param url The url of the artifact
 * @param dependency_count The number of dependencies in the dependency array
 * @param dependencies The dependency array
 * @return KPMResult 
 */
KPM_Result KPM_ListArtifactDependencies(KPM *kpm, const char *repository, const char *id, const char *url, size_t *dependency_count, KPM_Dependency **dependencies);

/**
 * @brief Update the local index of package by downloading repository manifests
 * 
 * @param kpm The KPM object
 * @param statusCallback A callback for progress information
 * @return KPMResult 
 */
KPM_Result KPM_UpdateIndex(KPM *kpm, KPMIO *kpm_io);


/**
 * @brief Free an installation target object
 * 
 * @param target The installation target object to free
 */
void KPM_FreeInstallTarget(KPM_InstallTarget *target);

/**
 * @brief Frees a list of KPM_InstallTarget objects
 * 
 * @param target_count The number of KPM_InstallTarget objects
 * @param targets The list of KPM_InstallTarget objects
 */
void KPM_FreeInstallTargetList(size_t target_count, KPM_InstallTarget *targets);


/**
 * @brief Installs/upgrades a target package and its dependencies
 * 
 * @param kpm 
 * @param target_count The number of packages to install
 * @param targets An array of packages to install
 * @param kpm_io 
 * @return KPMResult 
 */
KPM_Result KPM_InstallPackages(KPM *kpm, size_t target_count, KPM_InstallTarget *targets, KPMIO *kpm_io);

/**
 * @brief Uninstalls a package
 * 
 * @param kpm 
 * @param package_count
 * @param package_ids 
 * @param kpm_io 
 * @return KPMResult 
 */
KPM_Result KPM_UninstallPackages(KPM *kpm, size_t package_count, const char *package_ids[], KPMIO *kpm_io);

#endif