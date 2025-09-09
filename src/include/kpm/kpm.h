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
    KPM_INVALID_RESPONSE_CONTENT /**< Invalid content when parsing it (likely malformed JSON or repository manifest) */
};

/**
* @brief Type of dependency version
* 
*/
enum DependencyType
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
    char* url; /**< URL of the artifact - primary key */
    char* repository; /**< The repository ID */
    char* id; /**< The package ID */
    struct SemVer version; /**< The version of this artifact */
};

/**
* @brief A dependency of an artifact KPM has indexed
* 
*/
struct IndexedArtifactDependency
{
    char* artifact; /**< URL of the artifact */
    char* repository; /**< The repository ID */
    char* id; /**< The package ID */
    enum DependencyType type; /**< The type of dependency */
    struct SemVer version; /**< The version of the dependency */
};

/**
* @brief A package KPM has installed
* 
*/
struct InstalledPackage
{
    char* id; /**< The package ID */
    char* name; /**< The package name */
    char* author; /**< The package author */
    char* description; /**< The package description */
    struct SemVer version; /**< The package version */
};

/**
* @brief A dependency of a package KPM has installed
* 
*/
struct Dependency
{
    char* dependent; /**< ID of installed package */
    char* dependency_repository; /**< ID of the repository of the dependency */
    char* dependency_id; /**< ID of the dependency */
    enum DependencyType dependency_type; /**< The type of dependency */
    struct SemVer version; /**< The dependency's version */
};

enum Verbosity
{
    KPM_VERBOSITY_DEBUG,
    KPM_VERBOSITY_INFO,
    KPM_VERBOSITY_WARN,
    KPM_VERBOSITY_ERROR
};

struct KPM
{
    sqlite3* db;
};

typedef void KPMStatusCallback(enum Verbosity verbosity, uint progress, char* details, ...);

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
enum KPMResult KPM_SearchPackages(struct KPM* kpm, const char* query, size_t* packageCount, struct IndexedPackage** packages);

void KPM_FreeIndexedArtifact(struct IndexedArtifact* artifact);
void KPM_FreeIndexedArtifactList(size_t artifactCount, struct IndexedArtifact* artifacts);
enum KPMResult KPM_ListPackageArtifacts(struct KPM* kpm, const char* repositoryId, const char* packageId, size_t* artifactCount, struct IndexedArtifact** artifacts);

enum KPMResult KPM_UpdateIndex(struct KPM *kpm, KPMStatusCallback* statusCallback);

enum KPMResult KPM_ResolveInstallString();
enum KPMResult KPM_DownloadPackages(struct KPM *kpm, size_t packageCount, const char** packageIds, KPMStatusCallback* statusCallback);

// Artifact management functions

/*    // Internet functions
    bool UpdateIndex();
    bool InstallPackage(const char*& installString); // installString is generally a package ID but MAY contain a repo prefix

    // Local package functions
    std::vector<InstalledPackage> GetInstalledPackages();
    bool InstallPackage(const std::filesystem::path& package);
    bool UninstallPackage(InstalledPackage package);*/