#include "kpm/kpkg.h"
#include "kpm/kpm.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void KPM_FreeInstallTarget(InstallTarget *target)
{
    if (target->type == INSTALL_FILE)
    {
        KPKG_CloseFile(target->file);
        target->file = NULL;
    }
    else
    {
        KPM_FreeIndexedArtifact(&target->artifact);
    }
}

void KPM_FreeInstallTargetList(size_t target_count, InstallTarget *targets)
{
    for (size_t i=0; i < target_count; i++)
    {
        KPM_FreeInstallTarget(&targets[i]);
    }
    free(targets);
}