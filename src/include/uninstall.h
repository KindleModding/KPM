#pragma once

#include "kpm/kpm.h"

KPMResult Internal_RunUninstallHook(const char* outPath, const char* packageId, bool upgrading, KPMIO* kpm_io);
KPMResult Internal_UninstallPackage(KPM* kpm, const char* packageId, bool upgrading, KPMIO* kpmIO);