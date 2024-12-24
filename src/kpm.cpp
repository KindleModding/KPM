#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <vector>

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
    for (int i = 0; i < argc; i++) {
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
        } else if (i != argc-1) {
            targets.push_back(argv[i]);
        } else {
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

    if (operation == "update") {
        printf("Running UPDATE operation\n");
    } else if (operation == "install") {

    } else if (operation == "remove") {

    } else if (operation == "upgrade") {

    } else if (operation == "search") {

    } else {
        printf("No such operation [%s].\n", operation.c_str());
        return 1;
    }

    return 0;
}