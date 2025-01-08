#pragma once
#include "database.hpp"
#include <string>

bool compareSemverGTEQ(const std::string& a, const std::string& b); // Will return true if b is greater than or equal to a
bool firmwareWithinRange(const std::string& current, const std::string& min, const std::string& max);

ParsedPackageTarget parsePackageTarget(const std::string& target);
std::vector<PackageInstallCandidate> getRecursiveDependencies(Database& database, const PackageVersion& target);
bool installPackage(Database& database, const std::filesystem::path& packageFilePath, const Repository& repository);