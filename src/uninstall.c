#include "kpm/kpm.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "internal_utils.h"

enum KPMResult KPM_UninstallPackage(struct KPM* kpm, const char* packageId, struct KPMLogging* kpmLogging)
{
    kpmLogging->log(KPM_VERBOSITY_INFO, "Uninstalling [%s]", packageId);

    size_t dependentCount;
    struct InstalledDependency* dependents;
    KPM_ListInstalledPackageDependents(kpm, packageId, &dependentCount, &dependents);
    if (dependentCount > 0)
    {
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Packages depend on [%s]", packageId);
        for (int i=0; i < dependentCount; i++)
        {
            kpmLogging->log(KPM_VERBOSITY_ERROR, "- %s (%zu.%zu.%zu - %zu.%zu.%zu)", dependents[i].dependent, dependents[i].min_version.major, dependents[i].min_version.minor, dependents[i].min_version.patch, dependents[i].max_version.major, dependents[i].max_version.minor, dependents[i].max_version.patch);
        }
        KPM_FreeInstalledPackageDependencyList(dependentCount, dependents);
        return KPM_GENERIC_ERROR;
    }

    struct InstalledPackage package;
    if (KPM_GetInstalledPackage(kpm, packageId, &package) != KPM_OK)
    {
        kpmLogging->log(KPM_VERBOSITY_ERROR, "Package [%s] is not installed", packageId);
        return KPM_GENERIC_ERROR;
    }
    KPM_FreeInstalledPackage(&package);

    char* outPath = asprintf_hd("%s/%s/", kpm->pkgPath, packageId);
    char* uninstallScriptPath = asprintf_hd( "%suninstall.sh", outPath);

    if (access(uninstallScriptPath, R_OK) == 0)
    {
        kpmLogging->log(KPM_VERBOSITY_DEBUG, "Running uninstall script for [%s]", packageId);
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

    kpmLogging->log(KPM_VERBOSITY_DEBUG, "Deleting [%s] files", packageId);
    rmdir_r(outPath);

    // Remove installed item from the database
    const char* zSQL = "DELETE FROM installed_packages WHERE id=?";
    sqlite3_stmt* statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, packageId, -1, SQLITE_STATIC);
    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        return KPM_SQLITE_ERROR; // Failure with adding it to the database - @TODO: This could be bad, we may need better error handling
    }
    sqlite3_finalize(statement);

    kpmLogging->log(KPM_VERBOSITY_INFO, "Uninstalled [%s]", packageId);
    return KPM_OK;
}