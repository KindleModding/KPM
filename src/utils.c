#include "kpm/kpm.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void KPM_FreeInstallTarget(struct InstallTarget* target)
{
    free(target->id);
    free(target->repository);
    target->id = NULL;
    target->repository = NULL;
}