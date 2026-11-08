#include "kpm/kpkg.h"
#include "kpm/kpm.h"

char *KPM_GetInstallTargetId(InstallTarget target)
{
    if (target.type == INSTALL_ARTIFACT)
        return target.artifact.id;
    else
        return KPKG_GetId(target.file);
}

SemVer KPM_GetInstallTargetVersion(InstallTarget target)
{
    if (target.type == INSTALL_ARTIFACT)
        return target.artifact.version;
    else
        return KPKG_GetVersion(target.file);
}

int KPM_GetInstallTargetDependencies(KPM *kpm, InstallTarget target, KPM_Dependency **dependencies)
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