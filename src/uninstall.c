#include "kpm/kpm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "internal_utils.h"
#include "uninstall.h"

KPMResult Internal_RunUninstallHook(const char *outPath, const char *package_id, bool upgrading, KPMIO *kpm_io)
{
    char *uninstallScriptPath = asprintf_hd( "%suninstall.sh", outPath);

    if (access(uninstallScriptPath, R_OK) == 0)
    {
        free(uninstallScriptPath);
        kpm_io->log(KPM_VERBOSITY_DEBUG, "Running uninstall script for [%s]", package_id);
        // Run uninstall script
        int result = -1;

        char *uninstallCommand;
        if (upgrading)
            uninstallCommand = asprintf_hd("KPM_VERSION_MAJOR=%i KPM_VERSION_MINOR=%i KPM_VERSION_PATCH=%i KPM_PLATFORM=\"%s\" sh uninstall.sh upgrade 2>&1", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH, KPM_PLATFORM);
        else
            uninstallCommand = asprintf_hd("KPM_VERSION_MAJOR=%i KPM_VERSION_MINOR=%i KPM_VERSION_PATCH=%i KPM_PLATFORM=\"%s\" sh uninstall.sh 2>&1", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH, KPM_PLATFORM);

        char *old_path = getcwd(NULL, 0);
        chdir(outPath);
        kpm_io->log(KPM_VERBOSITY_INFO, "Running uninstall hooks for %s", package_id);
        FILE *stream = popen(uninstallCommand, "r");
        free(uninstallCommand);
        if (stream != NULL)
        {
            int c;
            while ((c = fgetc(stream)) != EOF)
            {
                kpm_io->stream((char) c);
            }
            result = pclose(stream);
        }
        else
        {
            kpm_io->log(KPM_VERBOSITY_ERROR, "Could not run script - POPEN FAILURE");
        }

        chdir(old_path);
        free(old_path);
        if (result != 0)
        {
            // The uninstall hook failed
            kpm_io->log(KPM_VERBOSITY_ERROR, "Could not execute uninstall hook for [%s]", package_id);
            return KPM_GENERIC_ERROR;
        }
    }

    return KPM_OK;
}

KPMResult Internal_UninstallPackage(KPM *kpm, const char *package_id, bool upgrading, KPMIO *kpm_io)
{
    char *outPath = asprintf_hd("%s/%s/", kpm->pkgPath, package_id);
    
    KPMResult result;
    if ((result = Internal_RunUninstallHook(outPath, package_id, upgrading, kpm_io)) != KPM_OK)
        return result;

    kpm_io->log(KPM_VERBOSITY_DEBUG, "Deleting [%s] files", package_id);
    rmdir_r(outPath);
    free(outPath);

    // Remove installed item from the database
    const char *zSQL = "DELETE FROM installed_packages WHERE id=?";
    sqlite3_stmt *statement;
    sqlite3_prepare_v2(kpm->db, zSQL, -1, &statement, NULL);
    sqlite3_bind_text(statement, 1, package_id, -1, SQLITE_STATIC);
    if (sqlite3_step(statement) != SQLITE_DONE)
    {
        sqlite3_finalize(statement);
        system("lipc-set-prop com.lab126.scanner doFullScan 1");
        return KPM_SQLITE_ERROR; // Failure with adding it to the database - @TODO: This could be bad, we may need better error handling
    }
    sqlite3_finalize(statement);

    return KPM_OK;
}

KPMResult KPM_UninstallPackages(KPM *kpm, size_t package_count, const char *package_ids[], KPMIO *kpm_io)
{
    kpm_io->log(KPM_VERBOSITY_INFO, "Uninstalling %zu packages.", package_count);

    for (int i=0; i < package_count; i++)
    {
        if (KPM_GetInstalledPackage(kpm, package_ids[i], NULL) != KPM_OK)
        {
            kpm_io->log(KPM_VERBOSITY_ERROR, "Package [%s] is not installed", package_ids[i]);
            return KPM_GENERIC_ERROR;
        }
    }

    for (int i=0; i < package_count; i++)
    {
        size_t dependent_count;
        Dependency *dependents;
        KPM_ListInstalledPackageDependents(kpm, package_ids[i], &dependent_count, &dependents);

        size_t unaccountedDependencies = dependent_count;
        for (int j=0; j < package_count; j++)
            for (int k=0; k < dependent_count; k++)
                if (strcmp(package_ids[j], dependents[k].id) == 0)
                    unaccountedDependencies--;

        if (unaccountedDependencies > 0)
        {
            kpm_io->log(KPM_VERBOSITY_ERROR, "Packages depend on [%s]", package_ids[i]);
            for (int i=0; i < dependent_count; i++)
            {
                for (int j=0; j < package_count; j++)
                    if (strcmp(package_ids[j], dependents[i].id) == 0)
                        continue;

                kpm_io->log(KPM_VERBOSITY_ERROR, "- %s (%u.%u.%u - %u.%u.%u)", dependents[i].id, dependents[i].min_version.major, dependents[i].min_version.minor, dependents[i].min_version.patch, dependents[i].max_version.major, dependents[i].max_version.minor, dependents[i].max_version.patch);
            }
            return KPM_GENERIC_ERROR;
        }
    }
    // @TODO: Sort uninstall list by dependents first

    int result = KPM_OK;
    for (int i=0; i < package_count; i++)
    {
        kpm_io->log(KPM_VERBOSITY_DEBUG, "Uninstalling %s", package_ids[i]);
        if ((result = Internal_UninstallPackage(kpm, package_ids[i], false, kpm_io)) != KPM_OK)
            break;
    }

    system("lipc-set-prop com.lab126.scanner doFullScan 1");
    return result;
}