#pragma once
#include "database.hpp"
#include <string>

bool compareSemverGTEQ(const std::string& a, const std::string& b); // Will return true if b is greater than or equal to a
bool firmwareWithinRange(const std::string& current, const std::string& min, const std::string& max);

struct ParsedPackageTarget {
    std::string repository_id;
    std::string package_name;
    std::string package_version_name;
    VersionComparisonType version_comparison_type;
};

ParsedPackageTarget parsePackageTarget(const std::string& target);