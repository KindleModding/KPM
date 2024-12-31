#include "utils.hpp"
#include "database.hpp"
#include "log.hpp"
#include <vector>

bool compareSemverGTEQ(const std::string& a, const std::string& b) { // Will return true if a is greater than or equal to b
    std::vector<uint> semverComponentsA;
    std::vector<uint> semverComponentsB;

    size_t startIndex = 0;
    // Parse A
    while (true) {
        size_t foundIndex = a.find('.', startIndex);
        if (foundIndex == a.npos) {
            semverComponentsA.push_back(atoi(a.substr(startIndex, a.length()-startIndex).c_str()));
            break;
        }
        semverComponentsA.push_back(atoi(a.substr(startIndex, foundIndex-startIndex).c_str()));
        startIndex = foundIndex+1;
    }

    // Parse B
    startIndex = 0;
    while (true) {
        size_t foundIndex = b.find('.', startIndex);
        if (foundIndex == b.npos) {
            semverComponentsB.push_back(atoi(b.substr(startIndex, b.length()-startIndex).c_str()));
            break;
        }
        semverComponentsB.push_back(atoi(b.substr(startIndex, foundIndex-startIndex).c_str()));
        startIndex = foundIndex+1;
    }

    for (size_t i=0; i < semverComponentsB.size(); i++) {
        if (i == semverComponentsA.size() || // If we reach the end of A but there are more elements in B, it means A was the same up to those extra elements, and since there are more, A is smaller
            semverComponentsA[i] < semverComponentsB[i]) { // If any component of A is smaller than B since LTR, all previous were the same that means A is smaller
            return false;
        }


        if (semverComponentsA[i] > semverComponentsB[i]) { // Since we are going LTR, the first component being larger means the overall semver is larger
            return true;
        }
    }

    return true; // They are the same OR b is empty
}

bool firmwareWithinRange(const std::string &current, const std::string &min, const std::string &max) {
    return (min.size() == 0 || compareSemverGTEQ(current, min)) && (max.size() == 0 || compareSemverGTEQ(max, Flags::GetInstance()->firmware_version));
}

PackageTarget parsePackageTarget(Database& database, const std::string &target) {
    /**
     * @brief Convert a string representation of a package name and constraints into a PackageTarget, the package MUST be in the database
     * 
     */
    // Package format is:
    // package_id
    // package_id=version_name
    // repository_id/package_id
    // repository_id/package_id>=version_name

    // Set default comparison type
    PackageTarget packageTarget = {
        .package_version_comparison_type = VersionComparisonType::NONE
    };

    int separatorIndex = target.find('/'); // Repo is specified
    if (separatorIndex != -1) {
        packageTarget.repository_id = target.substr(0, separatorIndex);
    }

    // Check for version name and version comparison
    const int gtIndex = target.find('>');
    const int ltIndex = target.find('<');
    const int eqIndex = target.find('=');

    if (gtIndex + ltIndex + eqIndex == -3) {
        packageTarget.package_id = target.substr(separatorIndex + 1);
        return packageTarget; // There is no desired version_name
    }

    int version_comparison_type_index = 0;
    int version_number_index = 0;

    if (eqIndex - gtIndex + ltIndex == 0) {
        packageTarget.package_version_comparison_type = VersionComparisonType::GTEQ;
        version_comparison_type_index = gtIndex;
        version_number_index = gtIndex + 2;
    } else if (eqIndex - ltIndex + gtIndex == 0) {
        packageTarget.package_version_comparison_type = VersionComparisonType::LTEQ;
        version_comparison_type_index = ltIndex;
        version_number_index = ltIndex + 2;
    } else if (ltIndex != -1 && gtIndex == -1 && eqIndex == -1) {
        packageTarget.package_version_comparison_type =VersionComparisonType::LT;
        version_comparison_type_index = ltIndex;
        version_number_index = ltIndex + 1;
    } else if (gtIndex != -1 && ltIndex == -1 && eqIndex == -1) {
        packageTarget.package_version_comparison_type = VersionComparisonType::GT;
        version_comparison_type_index = gtIndex;
        version_number_index = gtIndex + 1;
    } else if (eqIndex != -1 && ltIndex == -1 && gtIndex == -1) {
        packageTarget.package_version_comparison_type = VersionComparisonType::EQ;
        version_comparison_type_index = eqIndex;
        version_number_index = eqIndex + 1;
    } else {
        packageTarget.package_id = "";
    }

    packageTarget.package_id = target.substr(separatorIndex + 1, version_comparison_type_index - (separatorIndex + 1));
    packageTarget.package_version_number = database.ConvertVersionNameToNumber(packageTarget.package_id, packageTarget.repository_id, target.substr(version_number_index, target.npos));

    return packageTarget;
}

std::vector<PackageInstallCandidate> recursivelyGetPackagesFromInstallTarget(Database& database, const PackageTarget &packageTarget) {
    Log::I("Getting packages needed for: %s", packageTarget.package_id.c_str());
    const std::vector<PackageInstallCandidate> versionsAvailable = database.GetCompatiblePackageVersions(packageTarget.package_id, packageTarget.repository_id, packageTarget.package_version_number, packageTarget.package_version_comparison_type);
    std::vector<PackageInstallCandidate> packagesToInstall;

    // Get the optimum version
    for (PackageInstallCandidate version : versionsAvailable) {
        Log::D("Version %s supports %s -> %s", version.version_name.c_str(), version.min_firmware.c_str(), version.max_firmware.c_str());
        if (firmwareWithinRange(Flags::GetInstance()->firmware_version, version.min_firmware, version.max_firmware)) {
            packagesToInstall.push_back(version);
            break;
        }
    }

    if (versionsAvailable.size() == 0 || packagesToInstall.size() == 0) {
        Log::E("Could not find suitable version for package - ABORTING: '%s'", packageTarget.package_id.c_str());
        exit(1);
    }

    // Get dependencies for this package
    const std::vector<PackageDependency> dependencies = database.GetPackageVersionDependencies({
        .package_id = packagesToInstall[0].package_id,
        .repository_id = packagesToInstall[0].repository_id,
        .version_number = packagesToInstall[0].version_number,
        .version_name = packagesToInstall[0].version_name,
        .architecture = packagesToInstall[0].architecture,
        .min_firmware = packagesToInstall[0].min_firmware,
        .max_firmware = packagesToInstall[0].max_firmware
    });

    // Add each dependency to the list of packages to install alongside the recursive dependencies for it
    for (PackageDependency dependency : dependencies) {
        const std::vector<PackageInstallCandidate> installForDependency = recursivelyGetPackagesFromInstallTarget(database, {
            .repository_id = dependency.repository_id,
            .package_id = dependency.package_id,
            .package_version_number = dependency.version_number,
            .package_version_comparison_type = dependency.version_comparison_type
        });
        // Add deps to vector
        packagesToInstall.insert(packagesToInstall.begin(), installForDependency.begin(), installForDependency.end()); // Dependencies are inserted at start so they are installed BEFORE the package itself is
    }

    return packagesToInstall;
}

bool installSinglePackage(PackageInstallCandidate packageToInstall) {
    // Ensure this package does not intefere with any existing dependencies
    return true;
}