/**
* @brief A dependency used internally for installation purposes
* 
*/
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <stdbool.h>

enum KPMResult Internal_ExtractArchive(char* path, char* out, KPMStatusCallback* statusCallback);
enum KPMResult Internal_GetManifest(char* path, char** outBuffer, KPMStatusCallback* statusCallback);