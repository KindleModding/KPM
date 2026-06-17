#include "kpm/kpm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "internal_utils.h"

enum KPMResult KPM_UninstallPackages(struct KPM* kpm, size_t packageCount, const char* packageIds[], struct KPMLogging* kpmLogging)
{
    kpmLogging->log(KPM_VERBOSITY_INFO, "Uninstalling %zu packages.", packageCount);

    for (int i=0; i < packageCount; i++)
    {
        if (KPM_GetInstalledPackage(kpm, packageIds[i], NULL) != KPM_OK)
        {
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Package [%s] is not installed", packageIds[i]);
            return KPM_GENERIC_ERROR;
        }
    }

    for (int i=0; i < packageCount; i++)
    {
        size_t dependentCount;
        struct InstalledDependency* dependents;
        KPM_ListInstalledPackageDependents(kpm, packageIds[i], &dependentCount, &dependents);

        size_t unaccountedDependencies = dependentCount;
        for (int j=0; j < packageCount; j++)
            for (int k=0; k < dependentCount; k++)
                if (strcmp(packageIds[j], dependents[k].dependent) == 0)
                    unaccountedDependencies--;

        if (unaccountedDependencies > 0)
        {
            kpmLogging->log(KPM_VERBOSITY_ERROR, "Packages depend on [%s]", packageIds[i]);
            for (int i=0; i < dependentCount; i++)
            {
                for (int j=0; j < packageCount; j++)
                    if (strcmp(packageIds[j], dependents[i].dependent) == 0)
                        continue;

                kpmLogging->log(KPM_VERBOSITY_ERROR, "- %s (%u.%u.%u - %u.%u.%u)", dependents[i].dependent, dependents[i].min_version.major, dependents[i].min_version.minor, dependents[i].min_version.patch, dependents[i].max_version.major, dependents[i].max_version.minor, dependents[i].max_version.patch);
            }
            KPM_FreeInstalledPackageDependencyList(dependentCount, dependents);
            return KPM_GENERIC_ERROR;
        }
    }
    // @TODO: Sort uninstall list by dependencies first

    for (int i=0; i < packageCount; i++)
    {
        char* outPath = asprintf_hd("%s/%s/", kpm->pkgPath, packageIds[i]);
        char* uninstallScriptPath = asprintf_hd( "%suninstall.sh", outPath);

        if (access(uninstallScriptPath, R_OK) == 0)
        {
            kpmLogging->log(KPM_VERBOSITY_DEBUG, "Running uninstall script for [%s]", packageIds[i]);
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
                kpmLogging->log(KPM_VERBOSITY_ERROR, "Could not execute uninstall hook for [%s]", packageIds[i]);
                return KPM_GENERIC_ERROR;
            }
        }

        kpmLogging->log(KPM_VERBOSITY_DEBUG, "Deleting [%s] files", packageIds[i]);
        rmdir_r(outPath);

        // Remove installed item from the database
        const char* zSQL = "DELETE FROM installed_packages WHERE id=?";
        sqlite3_stmt* statement;
        sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
        sqlite3_bind_text(statement, 1, packageIds[i], -1, SQLITE_STATIC);
        if (sqlite3_step(statement) != SQLITE_DONE)
        {
            sqlite3_finalize(statement);
            return KPM_SQLITE_ERROR; // Failure with adding it to the database - @TODO: This could be bad, we may need better error handling
        }
        sqlite3_finalize(statement);
    }
    return KPM_OK;
}