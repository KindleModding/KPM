#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <filesystem>
#include <vector>

#include "repositories.hpp"
#include "database.hpp"
#include "flags.hpp"
#include "log.hpp"
#include "utils.hpp"

int main(int argc, char* argv[]) {
    std::vector<char*> targets;
    std::string operation;

    // Parse further args
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            const int flagCount = strlen(argv[i]) - 1;
            if (flagCount == 0) {
                Log::E("Invalid flags specified.");
                return 1;
            }

            if (argv[i][1] == '-') { // Parse complex parameters
                if (strcmp(argv[i], "--kpkg_dir") == 0) {
                    Flags::GetInstance()->kpkg_dir = std::string(argv[i+1]);
                    i++;
                } else if (strcmp(argv[i], "--force_architecture") == 0) {
                    Flags::GetInstance()->architecture = std::string(argv[i+1]);
                    i++;
                } else if (strcmp(argv[i], "--force_firmware") == 0) {
                    Flags::GetInstance()->firmware_version = std::string(argv[i+1]);
                    i++;
                } else {
                    Log::E("Invalid flags specified.");
                    return 1; // For now we don't have any of these
                }
            } else { // Parse single-letter boolean flags
                for (int j = 0; j < flagCount; j++) {
                    switch (argv[i][j+1]) {
                        case 'v':
                            Flags::GetInstance()->verbose = true;
                            break;
                        default:
                            Log::E("Invalid flags specified.");
                            return 1;
                    }
                }
            }
        } else if (operation.length() != 0) { // If the operation was gotten then we are now on targets
            targets.push_back(argv[i]);
        } else if (operation == "") { // No operation and not a flag? this must be it
            operation = argv[i];
        }
    }

    /**
     * Debug stuff 
     */
    Log::D("Running with flags:");
    Log::D("architecture: [%s]", Flags::GetInstance()->architecture.c_str());
    Log::D("firmware_version: [%s]", Flags::GetInstance()->firmware_version.c_str());
    Log::D("kpkg_dir: [%s]", Flags::GetInstance()->kpkg_dir.c_str());
    Log::D("verbose: [%d]", Flags::GetInstance()->verbose);
    Log::D("operation: [%s]", operation.c_str());

    if (operation == "") {
        Log::E("No operator specified.");
        return 1;
    }

    // Try to create the kpkg directory if it doesn't already exist
    std::filesystem::create_directories(Flags::GetInstance()->kpkg_dir);
    std::filesystem::create_directories(Flags::GetInstance()->cache_dir);
    std::filesystem::create_directories(Flags::GetInstance()->kpkg_dir + "/packages");
    
    // Initialise the database object
    Database database(Flags::GetInstance()->kpkg_dir + "/kpm.db");

    // Initialise curl
    curl_global_init(CURL_GLOBAL_ALL);

    /**
     * Main KPM code here
     * 
     */

    // Refresh the repository index from online sources
    if (operation == "update") {
        if (targets.size() != 0) {
            Log::E("UPDATE operation MUST not specify TARGETS");
            return 1;
        }
        const int updateResult = Repositories::updateRepositories(database);
        Log::I("Update complete - [%d] package(s) pulled!", updateResult);

    // Add a repository from an online source
    } else if (operation == "add-repo") {
        if (targets.size() == 0) {
            Log::E("Error: No targets specified for [add-repo]");
            return 1;
        }

        for (const std::string target : targets) {
            Log::I("* Adding repository - %s", target.c_str());
            const std::string result = Repositories::add(database, target);
            if (result == "") {
                Log::E("* Failed to add repository.");
            } else {
                Log::I("* Succesfully added repository - %s", result.c_str());
            }
        }

        const int updateResult = Repositories::updateRepositories(database);
        Log::I("Update complete - [%d] package(s) pulled!", updateResult);

    // Remove a repository by its ID
    } else if (operation == "remove-repo") {
        if (targets.size() == 0) {
            Log::E("Error: No targets specified for [remove-repo]");
            return 1;
        }

        for (const std::string target : targets) {
            Log::I("* Removing repository - %s", target.c_str());
            const int changed = database.DeleteRepository(target);
            if (changed == 0) {
                Log::E("* Failed to remove repository.");
            } else {
                Log::D("[%d] rows affected.", changed);
                Log::I("* Succesfully removed repository");
            }
        }
    // Install a specific package
    } else if (operation == "install") {
        std::vector<PackageInstallCandidate> packagesToInstall;

        for (const std::string target : targets) {
            Log::D("Getting package and repository for %s", target.c_str());
            ParsedPackageTarget parsedTarget = parsePackageTarget(target);
            Log::D("Parsed as: %s/%s@%s", parsedTarget.repository_id.c_str(), parsedTarget.package_name.c_str(), parsedTarget.version_name.c_str());

            // Find package candidates from the DB
            const std::vector<PackageInstallCandidate> installationCandidates = database.FindInstallationCandidates(parsedTarget);
            for (PackageInstallCandidate installCandidate : installationCandidates) {
                Log::I("Found installation candidate: %s/%s@%s", installCandidate.repository_id.c_str(), installCandidate.package_id.c_str(), installCandidate.version_name.c_str());
            }

            if (installationCandidates.size() > 1) {
                Log::E("Found multiple installation candidates - Unable to proceed"); // @TODO: Implement actual version candidate selection
                exit(1);
            }
            if (installationCandidates.size() == 0) {
                Log::E("No installation candidate found for '%s'", target.c_str());
                exit(1);
            }

            // Get the recursive dependencies for this package
            Log::D("Obtaining dependencies for %s", installationCandidates[0].package_id.c_str());
            std::vector<PackageInstallCandidate> dependencyCandidates = getRecursiveDependencies(database, {
                .package_id = installationCandidates[0].package_id,
                .repository_id = installationCandidates[0].repository_id,
                .version_name = installationCandidates[0].version_name,
                .version_number = installationCandidates[0].version_number,
                .architecture = installationCandidates[0].architecture,
                .min_firmware = installationCandidates[0].min_firmware,
                .max_firmware = installationCandidates[0].max_firmware
            });

            for (PackageInstallCandidate dependencyCandidate : dependencyCandidates) {
                // Check if we already added this package ID
                // @TODO: Track this in a seperate set or smth?
                bool found = false;
                for (PackageInstallCandidate alreadyInstalling : packagesToInstall) {
                    if (alreadyInstalling.package_id == dependencyCandidate.package_id) {
                        found = true;
                        break;
                    }
                }

                if (found) {
                    continue;
                }

                packagesToInstall.push_back(dependencyCandidate);
            }
            packagesToInstall.push_back(installationCandidates[0]);

            // Check that our installation packages don't intefere with any dependencies
            Log::D("Checking for dependency conflicts");
            for (PackageInstallCandidate package : packagesToInstall) {
                // Get existing dependencies that depend on this package
                std::vector<PackageDependency> dependencies = database.GetInstalledPackageDependenciesFromDependencyID(package.package_id, package.alias);

                // Ensure that the package we are installing is all good with the existing dependencies
                for (PackageDependency dependency : dependencies) {
                    // Find the dependency in our package_index
                    std::vector<PackageInstallCandidate> dependencyCandidates = database.FindInstallationCandidates({
                        .repository_id = dependency.repository_id,
                        .package_name = dependency.package_name,
                        .version_name = dependency.version_name,
                        .version_comparison_type = VersionComparisonType::EQ
                    });
                    if (dependencyCandidates.size() != 1) {
                        Log::E("Failed to find version_number for dependency %s - not in repo? (%i)", dependency.package_name.c_str(), dependencyCandidates.size());
                    }

                    if (
                        (
                            dependency.version_comparison_type == VersionComparisonType::GTEQ &&
                            package.version_number < dependencyCandidates[0].version_number
                        ) || (
                            dependency.version_comparison_type == VersionComparisonType::LTEQ &&
                            package.version_number > dependencyCandidates[0].version_number
                        ) || (
                            dependency.version_comparison_type == VersionComparisonType::GT &&
                            package.version_number <= dependencyCandidates[0].version_number
                        ) || (
                            dependency.version_comparison_type == VersionComparisonType::LT &&
                            package.version_number >= dependencyCandidates[0].version_number
                        ) || (
                            dependency.version_comparison_type == VersionComparisonType::EQ &&
                            package.version_number != dependencyCandidates[0].version_number
                        )
                    ) {
                        Log::E("ERROR: %s (%s) conflicts with dependency %s (%s) required by %s - ABORTING", package.package_id.c_str(), package.version_name.c_str(), dependency.package_name.c_str(), dependency.version_name.c_str(), dependency.dependent_package_id.c_str());
                        exit(1);
                    }
                }
            }
        }

        for (PackageInstallCandidate package : packagesToInstall) {
            Log::I("%s/%s=%s", package.repository_id.c_str(), package.package_id.c_str(), package.version_name.c_str());
        }
        Log::I("Preparing to install %i packages.", packagesToInstall.size());

        for (PackageInstallCandidate package : packagesToInstall) {
            database.InstallPackage(package);
        }
        Log::I("Done. Installed %i packages.", packagesToInstall.size());

    // Invalid operation requested
    } else {
        Log::E("No such operation [%s].", operation.c_str());
        return 1;
    }

    return 0;
}