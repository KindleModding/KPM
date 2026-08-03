#pragma once

#include "kpm/kpm.h"

KPMResult Internal_ExtractArchive(char* path, char* out, KPMIO* kpmIO);
KPMResult Internal_GetManifest(char* path, char** outBuffer, KPMIO* kpmIO);