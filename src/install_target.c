#include "kpm/kpkg.h"
#include "kpm/kpm.h"
#include <stdlib.h>

char *KPM_GetInstallTargetId(KPM_InstallTarget target)
{
    if (target.type == INSTALL_ARTIFACT)
        return target.artifact.id;
    else
        return KPKG_GetId(target.file);
}

SemVer KPM_GetInstallTargetVersion(KPM_InstallTarget target)
{
    if (target.type == INSTALL_ARTIFACT)
        return target.artifact.version;
    else
        return KPKG_GetVersion(target.file);
}

int KPM_GetInstallTargetDependencies(KPM *kpm, KPM_InstallTarget target, KPM_Dependency **dependencies)
{
    size_t dependency_count = 0;
    *dependencies = NULL;
    if (target.type == INSTALL_ARTIFACT)
    {
        KPM_ListArtifactDependencies(kpm, target.artifact.repository, target.artifact.id, target.artifact.url, &dependency_count, dependencies);
        return dependency_count;
    }
    else
    {
        return KPKG_GetDependencies(target.file, (KPKG_DependencyEntry**) dependencies);
    }
}

void KPM_FreeInstallTarget(KPM_InstallTarget *target)
{
    if (target->type == INSTALL_FILE)
    {
        KPKG_CloseFile(target->file);
        free(target->file);
        target->file = NULL;
    }
    else
    {
        KPM_FreeArtifact(&target->artifact);
    }
}

void KPM_FreeInstallTargetList(size_t target_count, KPM_InstallTarget *targets)
{
    for (size_t i=0; i < target_count; i++)
    {
        KPM_FreeInstallTarget(&targets[i]);
    }
    free(targets);
}