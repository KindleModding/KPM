#pragma once

#include "kpm/kpm.h"

enum KPMResult Internal_RunUninstallHook(const char* outPath, const char* packageId, bool upgrading, struct KPMIO* kpm_io);
enum KPMResult Internal_UninstallPackage(struct KPM* kpm, const char* packageId, bool upgrading, struct KPMIO* kpmIO);