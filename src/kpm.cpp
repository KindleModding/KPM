#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <filesystem>
#include <vector>
#include <algorithm>
#include "multiDownload.hpp"
#include "repositories.hpp"
#include "database.hpp"
#include "flags.hpp"
#include "log.hpp"
#include "utils.hpp"

int main(int argc, char *argv[])
{
    std::vector<char *> targets;
    std::string operation;

    // Parse further args
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            const int flagCount = strlen(argv[i]) - 1;
            if (flagCount == 0)
            {
                Log::E("Invalid flags specified.");
                return 1;
            }

            if (argv[i][1] == '-')
            { // Parse complex parameters
                if (strcmp(argv[i], "--kpm_dir") == 0)
                {
                    Flags::GetInstance()->kpm_dir = std::string(argv[i + 1]);
                    i++;
                }
                else if (strcmp(argv[i], "--cache_dir") == 0)
                {
                    Flags::GetInstance()->cache_dir = std::string(argv[i + 1]);
                    i++;
                }
                else if (strcmp(argv[i], "--force_architecture") == 0)
                {
                    Flags::GetInstance()->architecture = std::string(argv[i + 1]);
                    i++;
                }
                else if (strcmp(argv[i], "--force_firmware") == 0)
                {
                    Flags::GetInstance()->firmware_version = std::string(argv[i + 1]);
                    i++;
                }
                else
                {
                    Log::E("Invalid flags specified.");
                    return 1; // For now we don't have any of these
                }
            }
            else
            { // Parse single-letter boolean flags
                for (int j = 0; j < flagCount; j++)
                {
                    switch (argv[i][j + 1])
                    {
                    case 'v':
                        Flags::GetInstance()->verbose = true;
                        break;
                    default:
                        Log::E("Invalid flags specified.");
                        return 1;
                    }
                }
            }
        }
        else if (operation.length() != 0)
        { // If the operation was gotten then we are now on targets
            targets.push_back(argv[i]);
        }
        else if (operation == "")
        { // No operation and not a flag? this must be it
            operation = argv[i];
        }
    }

    /**
     * Debug stuff
     */
    Log::D("Running with flags:");
    Log::D("architecture: [%s]", Flags::GetInstance()->architecture.c_str());
    Log::D("firmware_version: [%s]", Flags::GetInstance()->firmware_version.c_str());
    Log::D("kpm_dir: [%s]", Flags::GetInstance()->kpm_dir.c_str());
    Log::D("verbose: [%d]", Flags::GetInstance()->verbose);
    Log::D("operation: [%s]", operation.c_str());

    if (operation == "")
    {
        Log::E("No operator specified.");
        return 1;
    }

    // Try to create the kpkg directory if it doesn't already exist
    std::filesystem::create_directories(Flags::GetInstance()->kpm_dir);
    std::filesystem::create_directories(Flags::GetInstance()->cache_dir);
    std::filesystem::create_directories(Flags::GetInstance()->kpm_dir + "/packages");

    // Initialise the database object
    Database database(Flags::GetInstance()->kpm_dir + "/kpm.db");

    // Initialise curl
    curl_global_init(CURL_GLOBAL_ALL);

    /**
     * Main KPM code here
     *
     */

    // Refresh the repository index from online sources
    if (operation == "update")
    {
        if (targets.size() != 0)
        {
            Log::E("UPDATE operation MUST not specify TARGETS");
            curl_global_cleanup();
            return 1;
        }
        const int updateResult = Repositories::updateRepositories(database);
        Log::I("Update complete - [%d] package(s) pulled!", updateResult);

        // Add a repository from an online source
    }
    else if (operation == "add-repo")
    {
        if (targets.size() == 0)
        {
            Log::E("Error: No targets specified for [add-repo]");
            curl_global_cleanup();
            return 1;
        }

        for (const std::string target : targets)
        {
            Log::I("* Adding repository - %s", target.c_str());
            const std::string result = Repositories::add(database, target);
            if (result == "")
            {
                Log::E("* Failed to add repository.");
            }
            else
            {
                Log::I("* Succesfully added repository - %s", result.c_str());
            }
        }

        const int updateResult = Repositories::updateRepositories(database);
        Log::I("Update complete - [%d] package(s) pulled!", updateResult);

        // Remove a repository by its ID
    }
    else if (operation == "remove-repo")
    {
        if (targets.size() == 0)
        {
            Log::E("Error: No targets specified for [remove-repo]");
            curl_global_cleanup();
            return 1;
        }

        for (const std::string target : targets)
        {
            Log::I("* Removing repository - %s", target.c_str());
            const int changed = database.DeleteRepository(target);
            if (changed == 0)
            {
                Log::E("* Failed to remove repository.");
            }
            else
            {
                Log::D("[%d] rows affected.", changed);
                Log::I("* Succesfully removed repository");
            }
        }
        // Install a specific package
    }
    else if (operation == "list-repos")
    {
        std::vector<Repository> repositories = database.GetRepositories();
        for (Repository repository : repositories)
        {
            Log::I("%s (%s)", repository.id.c_str(), repository.url.c_str());
        }
    }
    else if (operation == "search")
    {
        if (targets.size() != 1)
        {
            Log::E("SEARCH operation requires exactly one search term");
            curl_global_cleanup();
            return 1;
        }

        const std::string searchTerm = targets[0];
        std::vector<AvailablePackage> searchResults = database.SearchPackages(searchTerm);

        if (searchResults.empty())
        {
            Log::I("No packages found matching '%s'", searchTerm.c_str());
            Log::I("Try using different search terms or run 'update' to refresh the repository data.");
        }
        else
        {
            Log::I("Search results for '%s':", searchTerm.c_str());
            for (const AvailablePackage &package : searchResults)
            {
                // Check if the package is already installed
                InstalledPackage installedPackage = database.GetInstalledPackage(package.package_id);
                bool isInstalled = !installedPackage.package_id.empty();

                // Format output with installation status
                if (isInstalled)
                {
                    Log::I("%s (%s) [installed: %s] - %s",
                           package.display_name.c_str(),
                           package.version_name.c_str(),
                           installedPackage.version_name.c_str(),
                           package.package_id.c_str());
                    Log::I("  %s", package.description.c_str());
                }
                else
                {
                    Log::I("%s (%s) - %s",
                           package.display_name.c_str(),
                           package.version_name.c_str(),
                           package.package_id.c_str());
                    Log::I("  %s", package.description.c_str());
                }
            }
            Log::I("Found %zu package(s)", searchResults.size());
        }
    }
    else if (operation == "list")
    {
        if (targets.size() != 0)
        {
            Log::E("LIST operation MUST not specify TARGETS");
            curl_global_cleanup();
            return 1;
        }

        std::vector<AvailablePackage> availablePackages = database.GetAvailablePackages();

        if (availablePackages.empty())
        {
            Log::I("No packages available for architecture %s and firmware %s.",
                   Flags::GetInstance()->architecture.c_str(),
                   Flags::GetInstance()->firmware_version.c_str());
            Log::I("Try running 'update' to refresh the repository data.");
        }
        else
        {
            Log::I("Available packages:");
            for (const AvailablePackage &package : availablePackages)
            {
                // Check if the package is already installed
                InstalledPackage installedPackage = database.GetInstalledPackage(package.package_id);
                bool isInstalled = !installedPackage.package_id.empty();

                // Format output with installation status
                if (isInstalled)
                {
                    Log::I("%s (%s) [installed: %s] - %s",
                           package.display_name.c_str(),
                           package.version_name.c_str(),
                           installedPackage.version_name.c_str(),
                           package.package_id.c_str());
                }
                else
                {
                    Log::I("%s (%s) - %s",
                           package.display_name.c_str(),
                           package.version_name.c_str(),
                           package.package_id.c_str());
                }
            }
            Log::I("Total: %zu package(s)", availablePackages.size());
        }
    }
    else if (operation == "list-installed")
    {
        if (targets.size() != 0)
        {
            Log::E("LIST-INSTALLED operation MUST not specify TARGETS");
            curl_global_cleanup();
            return 1;
        }

        std::vector<InstalledPackage> installedPackages = database.GetInstalledPackages();

        if (installedPackages.empty())
        {
            Log::I("No packages are installed.");
        }
        else
        {
            Log::I("Installed packages:");
            for (const InstalledPackage &package : installedPackages)
            {
                Log::I("%s (%s) - %s", package.display_name.c_str(), package.version_name.c_str(), package.package_id.c_str());
            }
            Log::I("Total: %zu package(s)", installedPackages.size());
        }
    }
    else if (operation == "install")
    {
        std::vector<PackageInstallCandidate> packagesToInstall;

        for (const std::string target : targets)
        {
            Log::D("Getting package and repository for %s", target.c_str());
            ParsedPackageTarget parsedTarget = parsePackageTarget(target);
            Log::D("Parsed as: %s/%s@%s", parsedTarget.repository_id.c_str(), parsedTarget.package_name.c_str(), parsedTarget.version_name.c_str());

            // Find package candidates from the DB
            const std::vector<PackageInstallCandidate> installationCandidates = database.FindInstallationCandidates(parsedTarget);
            for (PackageInstallCandidate installCandidate : installationCandidates)
            {
                Log::I("Found installation candidate: %s/%s@%s", installCandidate.repository_id.c_str(), installCandidate.package_id.c_str(), installCandidate.version_name.c_str());
            }

            if (installationCandidates.size() > 1)
            {
                Log::E("Found multiple installation candidates - Unable to proceed"); // @TODO: Implement actual version candidate selection
                exit(1);
            }
            if (installationCandidates.size() == 0)
            {
                Log::E("No installation candidate found for '%s'", target.c_str());
                exit(1);
            }

            // Get the recursive dependencies for this package
            Log::D("Obtaining dependencies for %s", installationCandidates[0].package_id.c_str());
            std::vector<PackageInstallCandidate> dependencyCandidates = getRecursiveDependencies(database, {.package_id = installationCandidates[0].package_id,
                                                                                                            .repository_id = installationCandidates[0].repository_id,
                                                                                                            .version_name = installationCandidates[0].version_name,
                                                                                                            .version_number = installationCandidates[0].version_number,
                                                                                                            .architecture = installationCandidates[0].architecture,
                                                                                                            .min_firmware = installationCandidates[0].min_firmware,
                                                                                                            .max_firmware = installationCandidates[0].max_firmware});

            for (PackageInstallCandidate dependencyCandidate : dependencyCandidates)
            {
                // Check if we already added this package ID
                // @TODO: Track this in a seperate set or smth?
                bool found = false;
                for (PackageInstallCandidate alreadyInstalling : packagesToInstall)
                {
                    if (alreadyInstalling.package_id == dependencyCandidate.package_id)
                    {
                        found = true;
                        break;
                    }
                }

                if (found)
                {
                    continue;
                }

                packagesToInstall.push_back(dependencyCandidate);
            }
            packagesToInstall.push_back(installationCandidates[0]);

            // Check that our installation packages don't intefere with any dependencies
            Log::D("Checking for dependency conflicts");
            for (PackageInstallCandidate package : packagesToInstall)
            {
                // Get existing dependencies that depend on this package
                std::vector<PackageDependency> dependencies = database.GetInstalledPackageDependenciesFromDependencyID(package.package_id, package.alias);

                // Ensure that the package we are installing is all good with the existing dependencies
                for (PackageDependency dependency : dependencies)
                {
                    // Find the dependency in our package_index
                    std::vector<PackageInstallCandidate> dependencyCandidates = database.FindInstallationCandidates({.repository_id = dependency.repository_id,
                                                                                                                     .package_name = dependency.package_name,
                                                                                                                     .version_name = dependency.version_name,
                                                                                                                     .version_comparison_type = VersionComparisonType::EQ});
                    if (dependencyCandidates.size() != 1)
                    {
                        Log::E("Failed to find version_number for dependency %s - not in repo? (%i)", dependency.package_name.c_str(), dependencyCandidates.size());
                    }

                    if (
                        (
                            dependency.version_comparison_type == VersionComparisonType::GTEQ &&
                            package.version_number < dependencyCandidates[0].version_number) ||
                        (dependency.version_comparison_type == VersionComparisonType::LTEQ &&
                         package.version_number > dependencyCandidates[0].version_number) ||
                        (dependency.version_comparison_type == VersionComparisonType::GT &&
                         package.version_number <= dependencyCandidates[0].version_number) ||
                        (dependency.version_comparison_type == VersionComparisonType::LT &&
                         package.version_number >= dependencyCandidates[0].version_number) ||
                        (dependency.version_comparison_type == VersionComparisonType::EQ &&
                         package.version_number != dependencyCandidates[0].version_number))
                    {
                        Log::E("ERROR: %s (%s) conflicts with dependency %s (%s) required by %s - ABORTING", package.package_id.c_str(), package.version_name.c_str(), dependency.package_name.c_str(), dependency.version_name.c_str(), dependency.dependent_package_id.c_str());
                        exit(1);
                    }
                }
            }
        }

        for (PackageInstallCandidate package : packagesToInstall)
        {
            Log::I("%s/%s=%s", package.repository_id.c_str(), package.package_id.c_str(), package.version_name.c_str());
        }
        Log::I("Preparing to install %i packages.", packagesToInstall.size());

        std::vector<DownloadTarget> downloadTargets;
        std::vector<size_t> packagesToDownload;

        for (size_t i = 0; i < packagesToInstall.size(); i++)
        {
            // Queue download file
            std::string filename = packagesToInstall[i].package_id + '_' + packagesToInstall[i].version_name + '_' + packagesToInstall[i].architecture + ".kpkg";
            std::string destPath = Flags::GetInstance()->cache_dir + '/' + filename;
            DownloadTarget target = {
                .url = packagesToInstall[i].repository_url + "/packages/" + packagesToInstall[i].package_id + '/' + packagesToInstall[i].version_name + '/' + filename,
                .dest = destPath};

            // Check if the package is already cached
            if (isPackageCached(target))
            {
                Log::I("Package %s is already cached, skipping download", filename.c_str());
            }
            else
            {
                // Package needs to be downloaded
                packagesToDownload.push_back(i);
                downloadTargets.push_back(target);
            }
        }

        // Only perform download if there are packages that need it
        if (!downloadTargets.empty())
        {
            Log::I("Downloading %zu packages...", downloadTargets.size());
            MultiDownload md(downloadTargets);
            if (!md.execute())
            {
                const auto &errors = md.get_errors();
                Log::E("Download failed for %zu files:", errors.size());
                for (const auto &error : errors)
                {
                    if (error.http_code > 0)
                    {
                        Log::E("  %s: %s (HTTP %ld)",
                               error.url.c_str(),
                               error.error.c_str(),
                               error.http_code);
                    }
                    else
                    {
                        Log::E("  %s: %s",
                               error.url.c_str(),
                               error.error.c_str());
                    }
                }
                Log::E("Aborting installation due to download errors.");
                return 1;
            }
        }
        else
        {
            Log::I("All packages are already cached, skipping download");
        }

        Log::I("Installing %i packages.", packagesToInstall.size());
        for (size_t i = 0; i < packagesToInstall.size(); i++)
        {
            // Path for the package file
            std::string filename = packagesToInstall[i].package_id + '_' + packagesToInstall[i].version_name + '_' + packagesToInstall[i].architecture + ".kpkg";
            std::string packagePath = Flags::GetInstance()->cache_dir + '/' + filename;

            // Unpack our kpkg files
            if (!installPackage(database, packagePath, {.id = packagesToInstall[i].repository_id, .url = packagesToInstall[i].repository_url}))
            {
                Log::E("Error installing %s - SKIPPING - YOU MAY HAVE BROKEN PACKAGES", packagePath.c_str());
            }
        }

        Log::I("Done. Installed %i packages.", packagesToInstall.size());
    }
    else if (operation == "uninstall" || operation == "remove")
    {
        if (targets.size() == 0)
        {
            Log::E("Error: No targets specified for [%s]", operation.c_str());
            curl_global_cleanup();
            return 1;
        }

        int successCount = 0;
        int failCount = 0;

        for (const std::string &target : targets)
        {
            Log::I("Uninstalling %s...", target.c_str());

            // First check if the package is installed
            InstalledPackage package = database.GetInstalledPackage(target);
            if (package.package_id.empty())
            {
                Log::E("Package '%s' is not installed.", target.c_str());
                failCount++;
                continue;
            }

            // Check if any installed packages depend on this one
            bool hasDependents = false;
            std::vector<InstalledPackage> installedPackages = database.GetInstalledPackages();
            for (const InstalledPackage &installedPkg : installedPackages)
            {
                if (installedPkg.package_id == target)
                {
                    continue; // Skip the package itself
                }

                // Get installed package dependencies
                std::vector<PackageDependency> dependencies = database.GetInstalledPackageDependenciesFromDependencyID(installedPkg.package_id, "");
                for (const PackageDependency &dependency : dependencies)
                {
                    if (dependency.package_name == target)
                    {
                        hasDependents = true;
                        Log::E("Cannot uninstall '%s': Package '%s' depends on it.", target.c_str(), installedPkg.package_id.c_str());
                        break;
                    }
                }

                if (hasDependents)
                {
                    break;
                }
            }

            if (hasDependents)
            {
                failCount++;
                continue;
            }

            // Uninstall the package
            if (uninstallPackage(database, target))
            {
                Log::I("Successfully uninstalled %s.", target.c_str());
                successCount++;
            }
            else
            {
                Log::E("Failed to uninstall %s.", target.c_str());
                failCount++;
            }
        }

        Log::I("Uninstallation complete: %d succeeded, %d failed.", successCount, failCount);

        if (failCount > 0)
        {
            return 1;
        }
    }
    else if (operation == "upgrade")
    {
        bool upgradeAll = true;
        std::vector<std::string> packagesToUpgrade;

        // If specific targets are provided, only upgrade those
        if (targets.size() > 0)
        {
            upgradeAll = false;
            packagesToUpgrade.assign(targets.begin(), targets.end());
        }

        // Get all installed packages
        std::vector<InstalledPackage> installedPackages = database.GetInstalledPackages();

        if (installedPackages.empty())
        {
            Log::I("No packages are installed.");
            return 0;
        }

        // Keep track of packages that need upgrading
        std::vector<std::pair<InstalledPackage, AvailablePackage>> packagesNeedingUpgrade;

        for (const InstalledPackage &installedPkg : installedPackages)
        {
            // Skip if we're not upgrading all and this package isn't explicitly requested
            if (!upgradeAll &&
                std::find(packagesToUpgrade.begin(), packagesToUpgrade.end(), installedPkg.package_id) == packagesToUpgrade.end())
            {
                continue;
            }

            // Find the latest version in the repositories
            ParsedPackageTarget target = {
                .repository_id = installedPkg.repository_id,
                .package_name = installedPkg.package_id,
                .version_name = "",
                .version_comparison_type = VersionComparisonType::NONE};

            std::vector<PackageInstallCandidate> candidates = database.FindInstallationCandidates(target);

            if (candidates.empty())
            {
                Log::W("No upgrades found for %s (%s)", installedPkg.display_name.c_str(), installedPkg.package_id.c_str());
                continue;
            }

            // Find the candidate with the highest version number
            PackageInstallCandidate latestCandidate = candidates[0];
            for (const auto &candidate : candidates)
            {
                if (candidate.version_number > latestCandidate.version_number)
                {
                    latestCandidate = candidate;
                }
            }

            // Check if the latest version is newer than the installed version
            if (latestCandidate.version_number > installedPkg.version_number)
            {
                Log::I("Upgrade available for %s: %s -> %s",
                       installedPkg.display_name.c_str(),
                       installedPkg.version_name.c_str(),
                       latestCandidate.version_name.c_str());

                // Convert to AvailablePackage format for consistency
                AvailablePackage availablePkg = {
                    .package_id = latestCandidate.package_id,
                    .alias = latestCandidate.alias,
                    .repository_id = latestCandidate.repository_id,
                    .display_name = latestCandidate.display_name,
                    .description = latestCandidate.description,
                    .screenshots = latestCandidate.screenshots,
                    .version_name = latestCandidate.version_name,
                    .version_number = latestCandidate.version_number,
                    .architecture = latestCandidate.architecture,
                    .min_firmware = latestCandidate.min_firmware,
                    .max_firmware = latestCandidate.max_firmware};

                packagesNeedingUpgrade.push_back({installedPkg, availablePkg});
            }
            else
            {
                if (!upgradeAll)
                {
                    Log::I("%s (%s) is already at the latest version.",
                           installedPkg.display_name.c_str(),
                           installedPkg.version_name.c_str());
                }
            }
        }

        if (packagesNeedingUpgrade.empty())
        {
            Log::I("All packages are up to date.");
            return 0;
        }

        Log::I("Preparing to upgrade %zu package(s)", packagesNeedingUpgrade.size());

        // First uninstall the existing packages
        for (const auto &upgradePair : packagesNeedingUpgrade)
        {
            Log::I("Removing previous version of %s (%s)",
                   upgradePair.first.display_name.c_str(),
                   upgradePair.first.version_name.c_str());

            if (!uninstallPackage(database, upgradePair.first.package_id))
            {
                Log::E("Failed to remove previous version of %s. Skipping upgrade.",
                       upgradePair.first.package_id.c_str());
                continue;
            }
        }

        // Now install the new versions
        std::vector<PackageInstallCandidate> packagesToInstall;
        for (const auto &upgradePair : packagesNeedingUpgrade)
        {
            // Find the installation candidate for this package
            ParsedPackageTarget target = {
                .repository_id = upgradePair.second.repository_id,
                .package_name = upgradePair.second.package_id,
                .version_name = upgradePair.second.version_name,
                .version_comparison_type = VersionComparisonType::EQ};

            const std::vector<PackageInstallCandidate> installationCandidates = database.FindInstallationCandidates(target);

            if (installationCandidates.empty())
            {
                Log::E("Error: Failed to find installation candidate for %s. Skipping.",
                       upgradePair.second.package_id.c_str());
                continue;
            }

            // Get dependencies for this package
            std::vector<PackageInstallCandidate> dependencyCandidates = getRecursiveDependencies(database, {.package_id = installationCandidates[0].package_id,
                                                                                                            .repository_id = installationCandidates[0].repository_id,
                                                                                                            .version_name = installationCandidates[0].version_name,
                                                                                                            .version_number = installationCandidates[0].version_number,
                                                                                                            .architecture = installationCandidates[0].architecture,
                                                                                                            .min_firmware = installationCandidates[0].min_firmware,
                                                                                                            .max_firmware = installationCandidates[0].max_firmware});

            // Add dependencies if not already in the list
            for (const auto &dependency : dependencyCandidates)
            {
                bool found = false;
                for (const auto &existing : packagesToInstall)
                {
                    if (existing.package_id == dependency.package_id)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    packagesToInstall.push_back(dependency);
                }
            }

            // Add the package itself if not already in the list
            bool found = false;
            for (const auto &existing : packagesToInstall)
            {
                if (existing.package_id == installationCandidates[0].package_id)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                packagesToInstall.push_back(installationCandidates[0]);
            }
        }

        // Download and install the packages
        if (packagesToInstall.empty())
        {
            Log::I("No packages to install.");
            return 0;
        }

        // Log the packages to be installed
        for (const auto &pkg : packagesToInstall)
        {
            Log::I("%s/%s=%s", pkg.repository_id.c_str(), pkg.package_id.c_str(), pkg.version_name.c_str());
        }

        // Download packages
        std::vector<DownloadTarget> downloadTargets;
        std::vector<size_t> packagesToDownload;

        for (size_t i = 0; i < packagesToInstall.size(); i++)
        {
            std::string filename = packagesToInstall[i].package_id + '_' + packagesToInstall[i].version_name + '_' + packagesToInstall[i].architecture + ".kpkg";
            std::string destPath = Flags::GetInstance()->cache_dir + '/' + filename;
            DownloadTarget target = {
                .url = packagesToInstall[i].repository_url + "/packages/" + packagesToInstall[i].package_id + '/' + packagesToInstall[i].version_name + '/' + filename,
                .dest = destPath};

            // Check if the package is already cached
            if (isPackageCached(target))
            {
                Log::I("Package %s is already cached, skipping download", filename.c_str());
            }
            else
            {
                // Package needs to be downloaded
                packagesToDownload.push_back(i);
                downloadTargets.push_back(target);
            }
        }

        // Only perform download if there are packages that need it
        if (!downloadTargets.empty())
        {
            Log::I("Downloading %zu packages...", downloadTargets.size());
            MultiDownload md(downloadTargets);
            if (!md.execute())
            {
                Log::E("Download failed - Aborting upgrade.");
                return 1;
            }
        }
        else
        {
            Log::I("All packages are already cached, skipping download");
        }

        // Install downloaded packages
        Log::I("Installing %zu packages...", packagesToInstall.size());
        int successCount = 0;
        for (size_t i = 0; i < packagesToInstall.size(); i++)
        {
            std::string filename = packagesToInstall[i].package_id + '_' + packagesToInstall[i].version_name + '_' + packagesToInstall[i].architecture + ".kpkg";
            std::string packagePath = Flags::GetInstance()->cache_dir + '/' + filename;

            if (installPackage(database, packagePath, {.id = packagesToInstall[i].repository_id, .url = packagesToInstall[i].repository_url}))
            {
                successCount++;
            }
            else
            {
                Log::E("Error installing %s - SKIPPING", packagePath.c_str());
            }
        }

        Log::I("Upgrade complete: %d of %zu packages successfully upgraded.",
               successCount, packagesNeedingUpgrade.size());

        if (successCount < static_cast<int>(packagesNeedingUpgrade.size()))
        {
            Log::W("Some packages could not be upgraded.");
            return 1;
        }
        // Invalid operation requested
    }
    else
    {
        Log::E("No such operation [%s].", operation.c_str());
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}