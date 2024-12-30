#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <filesystem>
#include <vector>

#include "repositories.hpp"
#include "database.hpp"
#include "flags.hpp"
#include "log.hpp"

int main(int argc, char* argv[]) {
    std::vector<char*> targets;
    std::string operation;

    // Parse further args
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            const int flagCount = strlen(argv[i]) - 1;
            if (flagCount == 0) {
                Log::E("Invalid flags specified.");
                return 1;
            }

            if (argv[i][1] == '-') {
                // It's not a single-letter flag
                if (strcmp(argv[i], "--kpkg_dir") == 0) {
                    Flags::GetInstance()->kpkg_dir = std::string(argv[i+1]);
                    i++;
                } else if (strcmp(argv[i], "--force_architecture") == 0) {
                    Flags::GetInstance()->architecture = std::string(argv[i+1]);
                    i++;
                } else {
                    Log::E("Invalid flags specified.");
                    return 1; // For now we don't have any of these
                }
            } else {
                for (int j = 0; j < flagCount; j++) {
                    switch (argv[i][j+1]) {
                        case 'v':
                            Flags::GetInstance()->verbose = true;
                            break;
                        default:
                            Log::E("Invalid flags specified.");
                            return 1;
                    }
                }
            }
        } else if (operation != "") {
            targets.push_back(argv[i]);
        } else if (operation == "") {
            operation = argv[i];
        }
    }


    Log::D("Running with flags:");
    Log::D("architecture: [%s]", Flags::GetInstance()->architecture.c_str());
    Log::D("firmware_version: [%s]", Flags::GetInstance()->firmware_version.c_str());
    Log::D("kpkg_dir: [%s]", Flags::GetInstance()->kpkg_dir.c_str());
    Log::D("verbose: [%d]", Flags::GetInstance()->verbose);
    Log::D("operation: [%s]", operation.c_str());

    if (operation == "") {
        Log::E("No operator specified.");
        return 1;
    }

    // Try to create the kpkg directory if it doesn't already exist
    std::filesystem::create_directories(Flags::GetInstance()->kpkg_dir);
    
    Database database(Flags::GetInstance()->kpkg_dir + "/kpm.db");

    // Initialise curl
    curl_global_init(CURL_GLOBAL_ALL);

    if (operation == "update") {
        if (targets.size() != 0) {
            Log::E("UPDATE operation MUST not specify TARGETS");
            return 1;
        }
        const int updateResult = Repositories::updateRepositories(database);
        Log::I("Update complete - [%d] package(s) pulled!", updateResult);
    } else if (operation == "add-repo") {
        if (targets.size() == 0) {
            Log::E("Error: No targets specified for [add-repo]");
            return 1;
        }

        for (const std::string target : targets) {
            Log::I("* Adding repository - %s", target.c_str());
            const std::string result = Repositories::add(database, target);
            if (result == "") {
                Log::E("* Failed to add repository.");
            } else {
                Log::I("* Succesfully added repository - %s", result.c_str());
            }
        }

        const int updateResult = Repositories::updateRepositories(database);
        Log::I("Update complete - [%d] package(s) pulled!", updateResult);
    } else if (operation == "remove-repo") {
        if (targets.size() == 0) {
            Log::E("Error: No targets specified for [remove-repo]");
            return 1;
        }

        for (const std::string target : targets) {
            Log::I("* Removing repository - %s", target.c_str());
            const int changed = database.DeleteRepository(target);
            if (changed == 0) {
                Log::E("* Failed to remove repository.");
            } else {
                Log::D("[%d] rows affected.", changed);
                Log::I("* Succesfully removed repository");
            }
        }
    } else if (operation == "search") {
        std::string searchTerm;
        for (const std::string target : targets) {
            searchTerm += target + ' ';
        }
        Log::I("* Searching for [%s]", searchTerm.substr(0, searchTerm.length()-1).c_str());

        const std::vector<PackageWithVersion> packages = database.FindPackage('%' + searchTerm.substr(0, searchTerm.length()-1) + '%');
        if (packages.size() == 0) {
            Log::E("No packages found.");
            return 1;
        }

        for (PackageWithVersion package : packages) {
            Log::I("%s - %s @ %s", package.id.c_str(), package.name.c_str(), package.version_name.c_str());
        }
    } else {
        Log::E("No such operation [%s].", operation.c_str());
        return 1;
    }

    return 0;
}