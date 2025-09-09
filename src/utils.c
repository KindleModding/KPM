#include "kpm/kpm.h"
#include <stdlib.h>
#include <string.h>

enum KPMResult PM_ResolveInstallString(char* installString, char* out)
{
    char* repository = malloc(strlen(installString) + 1);
    char* package = malloc(strlen(installString) + 1);
}