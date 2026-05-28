#include "kpm/kpm.h"
#include <limits.h>
#include <stdbool.h>
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

enum Internal_UserVersionComparison
{
    Internal_DTCMP_NONE,
    Internal_DTCMP_GT,
    Internal_DTCMP_GTEQ,
    Internal_DTCMP_EQ,
    Internal_DTCMP_LT,
    Internal_DTCMP_LTEQ
};