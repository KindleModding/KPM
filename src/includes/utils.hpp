#pragma once
#include "database.hpp"
#include <string>

bool compareSemverGTOE(const std::string& a, const std::string& b); // Will return true if b is greater than or equal to a
bool firmwareWithinRange(const std::string& current, const std::string& min, const std::string& max);

struct InstallTarget {
    std::string repo_id;
    std::string package_id;
    uint package_version_number;
    std::string package_version_comparison;
};

InstallTarget parsePackageTarget(Database& database, const std::string &target);
std::vector<PackageWithVersion> recursivelyGetPackagesFromInstallTarget(Database& database, const InstallTarget &installTarget);
bool installSinglePackage(PackageWithVersion packageToInstall);