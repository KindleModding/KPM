#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <vector>

#include "repositories.h"
#include "database.h"
#include "flags.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("No operator specified.\n");
        return 1;
    }

    std::vector<char*> targets;
    Flags flags;
    std::string operation;

    // Parse further args
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            const int flagCount = strlen(argv[i]) - 1;
            if (flagCount == 0) {
                printf("Invalid flags specified.\n");
                return 1;
            }

            if (argv[i][1] == '-') {
                // It's not a single-letter flag
                if (strcmp(argv[i], "--kpkg_dir") == 0) {
                    flags.kpkg_dir = std::string(argv[i+1]);
                    i++;
                } else {
                    printf("Invalid flags specified.\n");
                    return 1; // For now we don't have any of these
                }
            } else {
                for (int j = 0; j < flagCount; j++) {
                    switch (argv[i][j+1]) {
                        case 'd':
                            flags.dry = true;
                            break;
                        case 'v':
                            flags.verbose = true;
                            break;
                        default:
                            printf("Invalid flags specified.\n");
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

    if (flags.verbose) {
        printf("Running with flags:\n");
        printf("kpkg_dir: [%s]\n", flags.kpkg_dir.c_str());
        printf("dry: [%d]\n", flags.dry);
        printf("verbose: [%d]\n", flags.verbose);
        printf("operation: [%s]\n", operation.c_str());
    }

    Database database(flags.kpkg_dir + "/kpm.db");

    // Initialise curl
    curl_global_init(CURL_GLOBAL_ALL);

    if (operation == "update") {
        printf("Running UPDATE operation\n");
        printf("* Retreiving repositories from db\n");
    } else if (operation == "install") {

    } else if (operation == "remove") {

    } else if (operation == "upgrade") {

    } else if (operation == "search") {

    } else if (operation == "add-repo") {
        if (targets.size() == 0) {
            printf("Error: No targets specified for [add-repo]\n");
            return 1;
        }

        for (const std::string target : targets) {
            printf("* Adding repository - %s\n", target.c_str());
            const std::string result = Repositories::add(database, target);
            if (result == "") {
                printf("* Failed to add repository.\n");
            } else {
                printf("* Succesfully added repository - %s\n", result.c_str());
            }
        }
    } else {
        printf("No such operation [%s].\n", operation.c_str());
        return 1;
    }

    return 0;
}