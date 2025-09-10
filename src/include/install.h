/**
* @brief A dependency used internally for installation purposes
* 
*/
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <stdbool.h>

struct Internal_Dependency
{
    char* repository;
    char* id;
    struct SemVer min_version; // Inclusive
    struct SemVer max_version; // Inclusive
};

static int copy_data(struct archive *ar, struct archive *aw);
enum KPMResult Internal_ExtractArchive(char* path, char* out, KPMStatusCallback* statusCallback);
enum KPMResult Internal_GetManifest(char* path, char** outBuffer, KPMStatusCallback* statusCallback);
enum KPMResult Internal_GetDependencies(struct InstallTarget* target, size_t* dependencyCount, struct Dependency** dependencies, KPMStatusCallback *statusCallback);
bool Internal_NarrowDependency(struct Internal_Dependency* currentDependency, struct Internal_Dependency* targetDependency);