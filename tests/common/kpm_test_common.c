#include "kpm.h"
#include <stdlib.h>

KPM initialise_kpm()
{
    KPM kpm = {
        .dbPath = "./repo_test.db",
        .pkgPath = "./packages",
        .maxConnections = 5
    };

    KPM_Initialise(&kpm);
    return kpm;
}