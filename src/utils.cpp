#include "utils.hpp"
#include "database.hpp"
#include "log.hpp"
#include <algorithm>
#include <vector>

bool compareSemverGTOE(const std::string& a, const std::string& b) { // Will return true if a is greater than or equal to b
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
    return (min.size() == 0 || compareSemverGTOE(current, min)) && (max.size() == 0 || compareSemverGTOE(max, Flags::GetInstance()->firmware_version));
}

InstallPackage parsePackageTarget(const std::string &target) {
    // Package format is:
    // package_id
    // package_id=version_name
    // repo_id/package_id
    // repo_id/package_id>=version_name

    // Version comparisons are same as SQL, they are mapped directly
    InstallPackage installPackage;

    int separatorIndex = target.find('/');
    if (separatorIndex != -1) {
        installPackage.repo_id = target.substr(0, separatorIndex);
    }

    const int gtIndex = target.find('>');
    const int ltIndex = target.find('<');
    const int eqIndex = target.find('=');

    if (std::max(std::max(gtIndex, ltIndex), eqIndex) == static_cast<int>(target.length()-1)) {
        return installPackage; // There is no desired version_name
    }

    if (eqIndex - gtIndex + ltIndex == 0) {
        installPackage.package_version_comparison = ">=";
        installPackage.package_id = target.substr(separatorIndex + 1, gtIndex - (separatorIndex+1));
        installPackage.package_version_name = target.substr(eqIndex+1, target.npos);
    } else if (eqIndex - ltIndex + gtIndex == 0) {
        installPackage.package_version_comparison = "<=";
        installPackage.package_id = target.substr(separatorIndex + 1, ltIndex - (separatorIndex+1));
        installPackage.package_version_name = target.substr(eqIndex+1, target.npos);
    } else if (ltIndex != -1 && gtIndex == -1 && eqIndex == -1) {
        installPackage.package_version_comparison = "<";
        installPackage.package_id = target.substr(separatorIndex + 1, ltIndex - (separatorIndex+1));
        installPackage.package_version_name = target.substr(ltIndex+1, target.npos);
    } else if (gtIndex != -1 && ltIndex == -1 && eqIndex == -1) {
        installPackage.package_version_comparison = ">";
        installPackage.package_id = target.substr(separatorIndex + 1, gtIndex - (separatorIndex+1));
        installPackage.package_version_name = target.substr(gtIndex+1, target.npos);
    } else if (eqIndex != -1 && ltIndex == -1 && gtIndex == -1) {
        installPackage.package_version_comparison = "=";
        installPackage.package_id = target.substr(separatorIndex + 1, eqIndex - (separatorIndex+1));
        installPackage.package_version_name = target.substr(eqIndex+1, target.npos);
    } else {
        installPackage.package_id = target.substr(separatorIndex + 1, std::max(gtIndex - (separatorIndex+1), std::max(ltIndex - (separatorIndex + 1), std::max(eqIndex - (separatorIndex + 1), static_cast<int>(target.length() - (separatorIndex + 1))))));
    }

    return installPackage;
}

std::vector<PackageWithVersion> recursivelyGetPackagesFromTarget(Database& database, const std::string &target) {
    Log::I("Getting packages needed for: %s", target.c_str());
    const InstallPackage installPackage = parsePackageTarget(target);
    const std::vector<PackageWithVersion> versionsAvailable = database.GetCompatiblePackageVersions(installPackage.package_id, installPackage.repo_id, installPackage.package_version_name, installPackage.package_version_comparison);
    std::vector<PackageWithVersion> packagesToInstall;

    // Get the optimum version
    for (PackageWithVersion version : versionsAvailable) {
        Log::D("Version %s supports %s -> %s", version.version_name.c_str(), version.min_firmware.c_str(), version.max_firmware.c_str());
        if (firmwareWithinRange(Flags::GetInstance()->firmware_version, version.min_firmware, version.max_firmware)) {
            packagesToInstall.push_back(version);
            break;
        }
    }

    if (versionsAvailable.size() == 0 || packagesToInstall.size() == 0) {
        Log::E("Could not find suitable version for package - ABORTING: '%s'", installPackage.package_id.c_str());
        exit(1);
    }

    // Get dependencies
    const std::vector<PackageVersionDependency> dependencies = database.GetPackageVersionDependencies({
        .package_id = packagesToInstall[0].package_id,
        .repo_id = packagesToInstall[0].repo_id,
        .version_number = packagesToInstall[0].version_number,
        .version_name = packagesToInstall[0].version_name,
        .architecture = packagesToInstall[0].architecture,
        .min_firmware = packagesToInstall[0].min_firmware,
        .max_firmware = packagesToInstall[0].max_firmware
    });

    for (PackageVersionDependency dependency : dependencies) {
        const std::vector<PackageWithVersion> installForDependency = recursivelyGetPackagesFromTarget(database, dependency.dependency_install_string);
        // Add dep deps to vector
        packagesToInstall.insert(packagesToInstall.begin(), installForDependency.begin(), installForDependency.end()); // Dependencies are inserted at start so they are installed BEFORE the package itself is
    }

    return packagesToInstall;
}