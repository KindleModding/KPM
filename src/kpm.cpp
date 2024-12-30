#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <vector>

#include "repositories.h"
#include "database.h"
#include "flags.h"
#include "log.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        Log::E("No operator specified.");
        return 1;
    }

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
                } else {
                    Log::E("Invalid flags specified.");
                    return 1; // For now we don't have any of these
                }
            } else {
                for (int j = 0; j < flagCount; j++) {
                    switch (argv[i][j+1]) {
                        case 'd':
                            Flags::GetInstance()->dry = true;
                            break;
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
    Log::D("kpkg_dir: [%s]", Flags::GetInstance()->kpkg_dir.c_str());
    Log::D("dry: [%d]", Flags::GetInstance()->dry);
    Log::D("verbose: [%d]", Flags::GetInstance()->verbose);
    Log::D("operation: [%s]", operation.c_str());
    
    
    Database database(Flags::GetInstance()->kpkg_dir + "/kpm.db");

    // Initialise curl
    curl_global_init(CURL_GLOBAL_ALL);

    if (operation == "update") {
        const int updateResult = Repositories::updateRepositories(database);
        Log::I("Update complete - [%d] package(s) pulled!", updateResult);
    } else if (operation == "install") {

    } else if (operation == "remove") {

    } else if (operation == "upgrade") {

    } else if (operation == "search") {

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
    } else {
        Log::E("No such operation [%s].", operation.c_str());
        return 1;
    }

    return 0;
}