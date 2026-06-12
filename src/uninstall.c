#include "kpm/kpm.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "internal_utils.h"

enum KPMResult KPM_UninstallPackage(struct KPM* kpm, const char* packageId, struct KPMLogging* kpmLogging)
{
    kpmLogging->log(KPM_VERBOSITY_INFO, "Preparing to uninstall [%s]", packageId);
    struct InstalledPackage package;
    enum KPMResult error;
    if ((error = KPM_GetInstalledPackage(kpm, packageId, &package)) != KPM_OK)
    {
        return error;
    }

    char* outPath = asprintf_hd("%s/%s/", kpm->pkgPath, packageId);
    char* uninstallScriptPath = asprintf_hd( "%suninstall.sh", outPath);

    if (access(uninstallScriptPath, R_OK) == 0)
    {
        kpmLogging->log(KPM_VERBOSITY_INFO, "Running uninstall script for [%s]", packageId);
        // Run uninstall script
        int result = -1;
        char* uninstallCommand = asprintf_hd("sh %s", uninstallScriptPath);
        chdir(outPath);
        free(outPath);
        free(uninstallScriptPath);
        FILE* stream = popen(uninstallCommand, "r");
        free(uninstallCommand);
        if (stream != NULL)
        {
            int c;
            while ((c = fgetc(stream)) != EOF)
            {
                kpmLogging->stream(fgetc(stream));
            }
            result = pclose(stream);
        }
        else
        {
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not run script - POPEN FAILURE");
        }

        if (result != 0)
        {
            // The uninstall hook failed
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not execute uninstall hook for [%s]", packageId);
            return KPM_GENERIC_ERROR;
        }
    }

    kpmLogging->log(KPM_VERBOSITY_INFO, "Deleting [%s] files", packageId);
    rmdir_r(outPath);
    return KPM_OK;
}