#pragma once

#include "kpm/kpm.h"
#include <stdbool.h>

enum KPMResult Internal_ExtractArchive(char* path, char* out, struct KPMLogging* kpmLogging);
enum KPMResult Internal_GetManifest(char* path, char** outBuffer, struct KPMLogging* kpmLogging);