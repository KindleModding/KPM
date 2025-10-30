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
    char* repository; /**< The repository ID */
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
    char* dependency_repository; /**< ID of the repository of the dependency */
    char* dependency_id; /**< ID of the dependency */
    struct SemVer min_version; /**< The min version of the dependency (inclusive) */
    struct SemVer max_version; /**< The max version of the dependency (exclusive) */
};



struct InstallTarget
{
    char* repository;
    char* id;
    struct SemVer* version;
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

enum KPMResult KPM_Initialise(struct KPM *kpm, const char* dbPath);
void KPM_Cleanup(struct KPM *kpm);

// Repo management functions
void KPM_FreeRepository(struct Repository* repository);
void KPM_FreeRepositoryList(size_t repositoryCount, struct Repository* repositories);
enum KPMResult KPM_ListRepositories(struct KPM* kpm, size_t* repositoryCount, struct Repository** repositories);
enum KPMResult KPM_GetRepository(struct KPM *kpm, const char *repositoryId, struct Repository* repository);
enum KPMResult KPM_AddRepository(struct KPM *kpm, const char *url, struct Repository* repository);
enum KPMResult KPM_RemoveRepository(struct KPM* kpm, const char* repositoryId);
enum KPMResult KPM_ListRepositoryPackages(struct KPM* kpm, const char* repositoryId, size_t* packageCount, struct IndexedPackage** packages);

// Package management functions
void KPM_FreeIndexedPackage(struct IndexedPackage* package);
void KPM_FreeIndexedPackageList(size_t packageCount, struct IndexedPackage* packages);
enum KPMResult KPM_GetPackage(struct KPM* kpm, const char* repositoryId, const char* packageId, struct IndexedPackage* package);
enum KPMResult KPM_GetPackages(struct KPM* kpm, const char* id, size_t* packageCount, struct IndexedPackage** packages);
enum KPMResult KPM_SearchPackages(struct KPM* kpm, const char* query, size_t* packageCount, struct IndexedPackage** packages);

// Installed package management functions
void KPM_FreeInstalledPackage(struct InstalledPackage* package);
void KPM_FreeInstalledPackageList(size_t packageCount, struct InstalledPackage* packages);
enum KPMResult KPM_GetInstalledPackage(struct KPM* kpm, const char* packageId, struct InstalledPackage* package);
enum KPMResult KPM_ListInstalledPackages(struct KPM* kpm, size_t* packageCount, struct InstalledPackage** packages);

// Installed dependency management functions
void KPM_FreeInstalledPackageDependency(struct InstalledDependency* dependency);
void KPM_FreeInstalledPackageDependencyList(size_t artifactCount, struct InstalledDependency* dependency);
enum KPMResult KPM_ListInstalledPackageDependencies(struct KPM* kpm, char* id, size_t* dependencyCount, struct InstalledDependency** dependencies);

// Artifact management functions
void KPM_FreeIndexedArtifact(struct IndexedArtifact* artifact);
void KPM_FreeIndexedArtifactList(size_t artifactCount, struct IndexedArtifact* artifacts);
enum KPMResult KPM_GetArtifact(struct KPM* kpm, const char* repositoryId, const char* packageId, struct SemVer version, struct IndexedArtifact* artifact);
enum KPMResult KPM_ListPackageArtifacts(struct KPM* kpm, const char* repositoryId, const char* packageId, size_t* artifactCount, struct IndexedArtifact** artifacts);

// Dependency management functions
void KPM_FreeArtifactDependency(struct ArtifactDependency* dependency);
void KPM_FreeArtifactDependencyList(size_t artifactCount, struct ArtifactDependency* dependency);
enum KPMResult KPM_ListArtifactDependencies(struct KPM* kpm, char* repository, char* id, char* url, size_t* dependencyCount, struct ArtifactDependency** dependencies);

enum KPMResult KPM_UpdateIndex(struct KPM *kpm, struct KPMLogging* kpmLogging);

void KPM_FreeInstallTarget(struct InstallTarget* target);

enum KPMResult KPM_InstallPackage(struct KPM* kpm, struct InstallTarget* target, struct KPMLogging* kpmLogging);