#pragma once
#include "database.hpp"
#include <string>

bool compareSemverGTOE(const std::string& a, const std::string& b); // Will return true if b is greater than or equal to a
bool firmwareWithinRange(const std::string& current, const std::string& min, const std::string& max);

struct InstallPackage {
    std::string repo_id;
    std::string package_id;
    std::string package_version_name;
    std::string package_version_comparison;
};

InstallPackage parsePackageTarget(const std::string& target);
std::vector<PackageWithVersion> recursivelyGetPackagesFromTarget(Database& database, const std::string& target);