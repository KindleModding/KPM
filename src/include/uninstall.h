#pragma once

#include "kpm/kpm.h"

KPMResult Internal_RunUninstallHook(const char *outPath, const char *package_id, bool upgrading, KPMIO *kpm_io);
KPMResult Internal_UninstallPackage(KPM *kpm, const char *package_id, bool upgrading, KPMIO *kpm_io);