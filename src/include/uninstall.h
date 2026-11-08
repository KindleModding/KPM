#ifndef UNINSTALL_H
#define UNINSTALL_H

#include "kpm/kpm.h"

KPM_Result Internal_RunUninstallHook(const char *outPath, const char *package_id, bool upgrading, KPMIO *kpm_io);
KPM_Result Internal_UninstallPackage(KPM *kpm, const char *package_id, bool upgrading, KPMIO *kpm_io);

#endif