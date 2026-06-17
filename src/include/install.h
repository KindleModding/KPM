#pragma once

#include "kpm/kpm.h"
#include <stdbool.h>

enum KPMResult Internal_ExtractArchive(char* path, char* out, struct KPMIO* kpmIO);
enum KPMResult Internal_GetManifest(char* path, char** outBuffer, struct KPMIO* kpmIO);