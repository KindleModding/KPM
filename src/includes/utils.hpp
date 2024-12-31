#pragma once
#include "database.hpp"
#include <string>

bool compareSemverGTEQ(const std::string& a, const std::string& b); // Will return true if b is greater than or equal to a
bool firmwareWithinRange(const std::string& current, const std::string& min, const std::string& max);

struct PackageTarget {
    std::string repository_id;
    std::string package_id;
    uint package_version_number;
    std::string package_version_comparison;
};

PackageTarget parsePackageTarget(Database& database, const std::string &target);
std::vector<PackageWithVersion> recursivelyGetPackagesFromInstallTarget(Database& database, const PackageTarget &packageTarget);
bool installSinglePackage(PackageWithVersion packageToInstall);