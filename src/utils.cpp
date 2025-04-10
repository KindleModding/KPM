#include "utils.hpp"
#include "database.hpp"
#include "flags.hpp"
#include "log.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include <archive.h>
#include <archive_entry.h>

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
        .version_name = "",
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
        parsedTarget.version_comparison_type = VersionComparisonType::LTEQ;
    }

    parsedTarget.package_name = target.substr(packageNameIndex, packageNameEndIndex - packageNameIndex);

    if (versionNameStartIndex != -1) {
        parsedTarget.version_name = target.substr(versionNameStartIndex);
    }

    return parsedTarget;
}


std::vector<PackageInstallCandidate> getRecursiveDependencies(Database& database, const PackageVersion& target) {
    std::vector<PackageDependency> dependencies = database.GetPackageDependencies({
        .package_id = target.package_id,
        .repository_id = target.repository_id,
        .version_name = target.version_name,
        .version_number = target.version_number,
        .architecture = target.architecture,
        .min_firmware = target.min_firmware,
        .max_firmware = target.max_firmware
    });

    std::vector<PackageInstallCandidate> dependencyInstallCandidates;
    for (PackageDependency dependency : dependencies) {
        const std::vector<PackageInstallCandidate> installationCandidates = database.FindInstallationCandidates({
            .repository_id = dependency.repository_id,
            .package_name = dependency.package_name,
            .version_name = dependency.version_name,
            .version_comparison_type = dependency.version_comparison_type
        });
        if (installationCandidates.size() == 0) {
            Log::E("Could not find installation candidate for dependency %s!", dependency.package_name.c_str());
            exit(1);
        }

        int candidateIndex = 0;
        if (installationCandidates.size() > 1) {
            // If multiple installation candidates are found we check if any of them are already installed and use that one
            // (safest option to avoid conflicts)
            for (size_t i=0; i < installationCandidates.size(); i++) {
                if (database.GetInstalledPackage(installationCandidates[i].package_id).package_id.length() != 0) {
                    candidateIndex = i;
                }
            }
        }

        // Now add the dependencies for this to the list
        const std::vector<PackageInstallCandidate> subDependencies = getRecursiveDependencies(database, {
            .package_id = installationCandidates[candidateIndex].package_id,
            .repository_id = installationCandidates[candidateIndex].repository_id,
            .version_name = installationCandidates[candidateIndex].version_name,
            .version_number = installationCandidates[candidateIndex].version_number,
            .architecture = installationCandidates[candidateIndex].architecture,
            .min_firmware = installationCandidates[candidateIndex].min_firmware,
            .max_firmware = installationCandidates[candidateIndex].max_firmware
        });
        dependencyInstallCandidates.insert(dependencyInstallCandidates.end(), subDependencies.begin(), subDependencies.end()); // Add the dependency's dependencies
        dependencyInstallCandidates.push_back(installationCandidates[candidateIndex]); // Add the dependency itself
    }

    return dependencyInstallCandidates;
}

bool installPackage(Database& database, const std::filesystem::path& packageFilePath, const Repository& repository) {
    // Assume checks have been done beforehand and that the kpkg file exists in our cache
    Log::I("Installing %s", packageFilePath.c_str());
    std::string tmpPath = Flags::GetInstance()->kpm_dir + "/tmp/" + packageFilePath.filename().string();//"/tmp/kpm/" + packageFilePath.filename().string();
    std::filesystem::remove_all(tmpPath);
    std::filesystem::create_directories(tmpPath);
    

    Log::D("Unpacking %s", packageFilePath.c_str());
    struct archive* a;
    struct archive* ext;
    struct archive_entry* entry;
    int flags;
    int r;

    flags = ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_ACL;
    flags |= ARCHIVE_EXTRACT_FFLAGS;

    a = archive_read_new();
    archive_read_support_format_all(a);
    archive_read_support_filter_all(a);
    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);
    if ((r = archive_read_open_filename(a, packageFilePath.c_str(), 10240))) {
        return false;
    }

    while (true) {
        r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) {
            break;
        }

        if (r < ARCHIVE_OK && r >= ARCHIVE_WARN) {
            Log::W("Reading kpkg: %s", archive_error_string(a));
        } else if (r < ARCHIVE_WARN) {
            Log::E("Failed to read kpkg: %s", archive_error_string(a));
            return false;
        }

        archive_entry_set_pathname(entry, (tmpPath + '/' + archive_entry_pathname(entry)).c_str());
        r = archive_write_header(ext, entry);
        if (r < ARCHIVE_OK && r >= ARCHIVE_WARN) {
            Log::W("Reading kpkg: %s", archive_error_string(ext));
        } else if (r < ARCHIVE_WARN) {
            Log::E("Failed to read kpkg header: %s", archive_error_string(ext));
            return false;
        } else if (archive_entry_size(entry) > 0) {
            // Copy archive data to location
            while (true) {
                const void* buff;
                size_t size;
                la_int64_t offset;
                r = archive_read_data_block(a, &buff, &size, &offset);
                if (r == ARCHIVE_EOF) {
                    break;
                }
                if (r < ARCHIVE_OK && r >= ARCHIVE_WARN) {
                    Log::W("Reading kpkg: %s", archive_error_string(a));
                } else if (r < ARCHIVE_WARN) {
                    Log::E("Failed to read kpkg data block: %s", archive_error_string(a));
                    return false;
                }

                // Write the data
                r = archive_write_data_block(ext, buff, size, offset);
                if (r < ARCHIVE_OK) {
                    Log::E("Failed to write kpkg data block: %s", archive_error_string(ext));
                    return false;
                }
            }
        }

        r = archive_write_finish_entry(ext);
        if (r < ARCHIVE_OK && r >= ARCHIVE_WARN) {
            Log::W("Reading kpkg: %s", archive_error_string(ext));
        } else if (r < ARCHIVE_WARN) {
            Log::E("Failed to read kpkg: %s", archive_error_string(ext));
            return false;
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    
    Log::D("Reading manifest.");
    std::ifstream manifestFile((tmpPath + "/manifest.json").c_str());
    nlohmann::json manifest = nlohmann::json::parse(manifestFile);
    Log::I("Package: %s (%s)", manifest["id"].get<std::string>().c_str(), manifest["version_name"].get<std::string>().c_str());
    std::string installPath = Flags::GetInstance()->kpm_dir + "/packages/" + manifest["id"].get<std::string>();
    std::filesystem::remove_all(installPath);
    std::filesystem::rename(tmpPath, installPath);

    Log::D("Running install actions");
    if (std::filesystem::exists(installPath + "/install.sh")) {
        int returnCode = system(("/bin/sh \"" + installPath + "/install.sh\"").c_str());
        if (returnCode != 0) {
            Log::E("An error occured whilst running install actions for %s", manifest["id"].get<std::string>().c_str());
            std::filesystem::remove_all(installPath);
            return false;
        }
    } else {
        Log::D("No install actions to run for this package.");
    }

    Log::D("Adding package to database");

    uint screenshots = 0;
    for (const std::filesystem::directory_entry& dir_entry : std::filesystem::directory_iterator(installPath + "/screenshots")) {
        if (dir_entry.is_regular_file()) {
            screenshots++;
        }
    }

    database.InstallPackage({
        .package_id = manifest["id"],
        .alias = manifest["alias"].is_null() ? "" : manifest["alias"].get<std::string>(),
        .repository_id = repository.id,
        .repository_url = repository.url,
        .display_name = manifest["display_name"],
        .description = manifest["description"],
        .screenshots = screenshots,
        .version_number = manifest["version_number"],
        .version_name = manifest["version_name"],
        .architecture = manifest["architecture"],
        .min_firmware = manifest["min_firmware"],
        .max_firmware = manifest["max_firmware"]
    });
    return true;
}