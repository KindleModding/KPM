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

    const char* operation = argv[1];
    std::vector<char*> targets;
    Flags flags;

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
                printf("Invalid flags specified.\n");
                return 1; // For now we don't have any of these
            } else {
                for (int j = 0; j < flagCount; j++) {
                    switch (argv[i][j+1]) {
                        case 'd':
                            flags.dry = true;
                            break;
                        default:
                            printf("Invalid flags specified.\n");
                            return 1;
                    }
                }
            }
        } else {
            targets.push_back(argv[i]);
        }
    }

    if (strcmp(operation, "update") == 0) {
        printf("Running INSTALL operation");
    } else if (strcmp(operation, "install") == 0) {

    } else if (strcmp(operation, "remove") == 0) {

    } else if (strcmp(operation, "upgrade") == 0) {

    } else if (strcmp(operation, "search") == 0) {

    } else {
        printf("No such operation [%s].\n", argv[1]);
        return 1;
    }

    return 0;
}