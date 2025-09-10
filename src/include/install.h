/**
* @brief A dependency used internally for installation purposes
* 
*/
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <stdbool.h>

static int copy_data(struct archive *ar, struct archive *aw);
enum KPMResult Internal_ExtractArchive(char* path, char* out, KPMStatusCallback* statusCallback);
enum KPMResult Internal_GetManifest(char* path, char** outBuffer, KPMStatusCallback* statusCallback);
enum KPMResult Internal_GetDependencies(struct InstallTarget* target, size_t* dependencyCount, struct Dependency** dependencies, KPMStatusCallback *statusCallback);
bool Internal_NarrowDependency(struct ArtifactDependency* currentDependency, struct ArtifactDependency* targetDependency);