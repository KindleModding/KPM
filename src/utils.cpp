#include "utils.hpp"
#include "database.hpp"
#include "flags.hpp"
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

ParsedPackageTarget parsePackageTarget(const std::string& target) {
    ParsedPackageTarget parsedTarget = {
        .repository_id = "",
        .package_version_name = "",
        .version_comparison_type = VersionComparisonType::NONE
    };

    // Get the indices
    const int repoEndIndex = target.find('/');
    const int LTIndex = target.find('<');
    const int GTIndex = target.find('>');
    const int EQIndex = target.find('=');

    if (repoEndIndex != -1) {
        parsedTarget.repository_id = target.substr(0, repoEndIndex);
    }

    int packageNameIndex = repoEndIndex+1;
    size_t packageNameEndIndex = std::string::npos;
    int versionNameStartIndex = -1;
    if (LTIndex != -1 && GTIndex == -1 && EQIndex == -1) { // <
        packageNameEndIndex = LTIndex;
        versionNameStartIndex = LTIndex+1;
        parsedTarget.version_comparison_type = VersionComparisonType::LT;
    } else if (GTIndex != -1 && LTIndex == -1 && EQIndex == -1) { // >
        packageNameEndIndex = GTIndex;
        versionNameStartIndex = GTIndex+1;
        parsedTarget.version_comparison_type = VersionComparisonType::GT;
    } else if (EQIndex != -1 && LTIndex == -1 && GTIndex == -1) { // =
        packageNameEndIndex = EQIndex;
        versionNameStartIndex = EQIndex+1;
        parsedTarget.version_comparison_type = VersionComparisonType::EQ;
    } else if (EQIndex - GTIndex == 1 && LTIndex == -1) { // >=
        packageNameEndIndex = GTIndex;
        versionNameStartIndex = EQIndex+1;
        parsedTarget.version_comparison_type = VersionComparisonType::GTEQ;
    } else if (EQIndex - LTIndex == 1 && GTIndex == -1) { // <=
        packageNameEndIndex = LTIndex;
        versionNameStartIndex = EQIndex+1;
        parsedTarget.version_comparison_type = VersionComparisonType::GTEQ;
    }

    parsedTarget.package_name = target.substr(packageNameIndex, packageNameEndIndex - packageNameIndex);

    if (versionNameStartIndex != -1) {
        parsedTarget.package_version_name = target.substr(versionNameStartIndex);
    }

    return parsedTarget;
}