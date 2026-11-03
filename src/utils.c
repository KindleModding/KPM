#include "kpm/kpkg.h"
#include "kpm/kpm.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void KPM_FreeInstallTarget(InstallTarget* target)
{
    if (target->type == INSTALL_FILE)
    {
        KPKG_CloseFile(target->file);
        target->file = NULL;
    }
    else
    {
        free(target->repository);
        free(target->id);
        free(target->version);
        target->repository = NULL;
        target->id = NULL;
        target->version = NULL;
    }
}

void KPM_FreeInstallTargetList(size_t targetCount, InstallTarget* targets)
{
    for (size_t i=0; i < targetCount; i++)
    {
        KPM_FreeInstallTarget(&targets[i]);
    }
    free(targets);
}