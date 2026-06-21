#pragma once

#include "kpm/kpm.h"

enum KPMResult Internal_UninstallPackage(struct KPM* kpm, const char* packageId, bool upgrading, struct KPMIO* kpmIO);