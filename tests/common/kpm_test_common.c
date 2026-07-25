#include "kpm.h"

struct KPM initialise_kpm()
{
    struct KPM kpm = {
        .dbPath = "./repo_test.db",
        .pkgPath = "./packages",
        .maxConnections = 5
    };

    KPM_Initialise(&kpm);
    return kpm;
}