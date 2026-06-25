#include "kpm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cli.h"
#include "io.h"
#include "internal_utils.h"
#include "semver.h"

#define CLI_VERSION_MAJOR 1
#define CLI_VERSION_MINOR 0
#define CLI_VERSION_PATCH 0

#ifndef KPM_PLATFORM // We assume an unknown platform is a PC testing build of the CLI
#define KPM_PLATFORM "unknown"
#endif

struct CLIState cli_state = {
    .fbink = false,
    .confirm = true,
#ifdef NDEBUG
    .debug = false
#else
    .debug = true
#endif
};

void init_header()
{
    io_initialise();
    kpm_io.log(KPM_VERBOSITY_INFO, "Kindle Package Manager v%i.%i.%i", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH);
    kpm_io.log(KPM_VERBOSITY_INFO, "Created by Hackerdude (https://hackerdude.tech)\n");
}

int main(int argc, char* argv[])
{
    int error = KPM_OK;

    if (access(KPM_PKG_PATH, R_OK) != 0)
        mkdir_r(KPM_PKG_PATH, 0775);

    struct KPM kpm = {
        .maxConnections = 5, // @TODO
        .pkgPath = KPM_PKG_PATH,
        .dbPath = KPM_DB_PATH
    };
    KPM_Initialise(&kpm);

    int command_index = -1;
    for (int i=1; i < argc; i++)
    {
        char* arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
        {
            init_header();
            goto help;
        }
        else if (strcmp(arg, "--fbink") == 0)
            cli_state.fbink = true;
        else if (strcmp(arg, "-d") == 0 || strcmp(arg, "--debug") == 0)
            cli_state.debug = true;
        else if (strcmp(arg, "-y") == 0)
            cli_state.confirm = false;
        else if (strncmp(arg, "--", 2) == 0 || strncmp(arg, "-", 1) == 0)
        {
            init_header();
            kpm_io.log(KPM_VERBOSITY_INFO, "Could not parse argument [%s]\n\n", arg);
            goto help;
        }
        else
        {
            command_index = i;
            break;
        }
    }

    init_header();
    
    if (command_index == -1)
        goto err_no_command;

    if (strcmp(argv[command_index], "version") == 0 || strcmp(argv[command_index], "debug") == 0)
    {
        kpm_io.log(KPM_VERBOSITY_INFO, "cli v%i.%i.%i", CLI_VERSION_MAJOR, CLI_VERSION_MINOR, CLI_VERSION_PATCH);
        kpm_io.log(KPM_VERBOSITY_INFO, "libkpm v%i.%i.%i", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH);
        kpm_io.log(KPM_VERBOSITY_INFO, "built for platform: %s", KPM_PLATFORM);
#ifndef NDEBUG
        kpm_io.log(KPM_VERBOSITY_INFO, "built without NDEBUG");
#endif

        if (strcmp(argv[command_index], "debug") == 0) // Undocumented? Should we document this
        {
            kpm_io.log(KPM_VERBOSITY_INFO, "package path: %s", KPM_PKG_PATH);
            kpm_io.log(KPM_VERBOSITY_INFO, "db path: %s", KPM_DB_PATH);
        }
    }
    else if (strcmp(argv[command_index], "add-repo") == 0)
    {
        if (argc-(command_index+1) == 0)
        {
            error = 1;
            kpm_io.log(KPM_VERBOSITY_ERROR, "No repositories were passed to KPM to add\n");
            goto help;
        }

        error = KPM_OK;
        struct Repository repository;
        for (int i = command_index+1; i < argc; i++)
        {
            if ((error = KPM_AddRepository(&kpm, argv[i], &repository, &kpm_io)) != KPM_OK)
                kpm_io.log(KPM_VERBOSITY_ERROR, "Could not add repository '%s' (%i)", argv[i], error);
            else
            {
                kpm_io.log(KPM_VERBOSITY_INFO, "Added repository '%s'", repository.id);
                KPM_FreeRepository(&repository);
            }
            
        }

        if (error == KPM_OK)
            kpm_io.log(KPM_VERBOSITY_INFO, "\nAdded %i repositories. Package index will not update until 'kpm update' is run.", argc-(command_index+1));
        else
            kpm_io.log(KPM_VERBOSITY_WARN, "There were some errors. Package index will not update until 'kpm update' is run.");
    }
    else if (strcmp(argv[command_index], "remove-repo") == 0)
    {
        if (argc-(command_index+1) == 0)
        {
            error = 1;
            kpm_io.log(KPM_VERBOSITY_ERROR, "No repositories were passed to KPM to remove\n");
            goto help;
        }

        error = KPM_OK;
        for (int i = command_index+1; i < argc; i++)
        {
            int tmp_error = 0;
            if ((tmp_error = KPM_GetRepository(&kpm, argv[i], NULL)) != KPM_OK || (tmp_error = KPM_RemoveRepository(&kpm, argv[i])) != KPM_OK)
            {
                kpm_io.log(KPM_VERBOSITY_ERROR, "Could not remove repository '%s' (%i)", argv[i], tmp_error);
                error = tmp_error;
            }
            else
                kpm_io.log(KPM_VERBOSITY_INFO, "Removed repository '%s'", argv[i]);
        }

        if (error == KPM_OK)
            kpm_io.log(KPM_VERBOSITY_INFO, "\nRemoved %i repositories. Package index will not update until 'kpm update' is run.", argc-(command_index+1));
        else
            kpm_io.log(KPM_VERBOSITY_WARN, "There were some errors. Package index will not update until 'kpm update' is run.");
    }
    else if (strcmp(argv[command_index], "list-repo") == 0)
    {
        size_t repository_count = 0;
        struct Repository* repositories;
        if ((error = KPM_ListRepositories(&kpm, &repository_count, &repositories)) != KPM_OK)
            goto cleanup;

        kpm_io.log(KPM_VERBOSITY_INFO, "Repositories:");
        if (repository_count == 0)
            kpm_io.log(KPM_VERBOSITY_INFO, "There are no repositories to show");
        for (int i=0; i < repository_count; i++)
            kpm_io.log(KPM_VERBOSITY_INFO, "  - %s - %s (%s)", repositories[i].id, repositories[i].name, repositories[i].url);
        KPM_FreeRepositoryList(repository_count, repositories);
    }
    else if (strcmp(argv[command_index], "update") == 0)
    {
        if ((error = KPM_UpdateIndex(&kpm, &kpm_io)) != KPM_OK)
        {
            kpm_io.log(KPM_VERBOSITY_ERROR, "Could not update indexed packages (%i)", error);
            goto cleanup;
        }

        kpm_io.log(KPM_VERBOSITY_INFO, "Updated indexed packages.");
    }
    else if (strcmp(argv[command_index], "search") == 0)
    {
        size_t query_length = 0;
        for (int i=command_index+1; i < argc; i++)
        {
            query_length += strlen(argv[i]);
            if (i != argc-1)
                query_length += 1; // For the space
        }
        char* query = malloc(query_length + 1);
        size_t cur_len = 0;
        for (int i=command_index+1; i < argc; i++)
        {
            sprintf(query + cur_len, "%s", argv[i]);
            cur_len += strlen(argv[i]);
            if (i != argc-1)
            {
                sprintf(query + cur_len, " ");
                cur_len++;
            }
        }

        size_t package_count;
        struct IndexedPackage* packages;
        if ((error = KPM_SearchPackages(&kpm, query, &package_count, &packages)) != KPM_OK)
        {
            kpm_io.log(KPM_VERBOSITY_ERROR, "Could not search for '%s' (%i)", query, error);
            free(query);
            goto cleanup;
        }

        kpm_io.log(KPM_VERBOSITY_INFO, "Found %li package(s) for %s:", package_count, query);
        for (int i = 0; i < package_count; i++)
            kpm_io.log(KPM_VERBOSITY_INFO, "  - %s (%s): %s", packages[i].id, packages[i].name, packages[i].description);

        KPM_FreeIndexedPackageList(package_count, packages);
        if (package_count == 0)
            kpm_io.log(KPM_VERBOSITY_INFO, "Could not find any packages for '%s'", query);
        free(query);
    }
    else if (strcmp(argv[command_index], "install") == 0)
    {
        // Update index automatically
        KPM_UpdateIndex(&kpm, &kpm_io);

        struct InstallTarget* targets = malloc((argc - (command_index+1)) * sizeof(struct InstallTarget));
        for (int i=0; i < argc - (command_index+1); i++)
        {
            targets[i].repository = NULL;
            targets[i].version = NULL;
            targets[i].id = argv[command_index+1 + i];
        }

        if (argc - (command_index+1) == 0)
        {
            free(targets);
            kpm_io.log(KPM_VERBOSITY_ERROR, "No packages were passed to install");
            error = KPM_GENERIC_ERROR;
            goto cleanup;
        }

        if ((error = KPM_InstallPackages(&kpm, argc - (command_index+1), targets, &kpm_io)) != KPM_OK)
        {
            kpm_io.log(KPM_VERBOSITY_ERROR, "Failed to install all packages (%i)", error);
        }
        else
        {
            kpm_io.log(KPM_VERBOSITY_INFO, "Installed %i package(s) succesfully.", argc - (command_index+1));
        }
        free(targets);
    }
    else if (strcmp(argv[command_index], "uninstall") == 0)
    {
        if ((error = KPM_UninstallPackages(&kpm, argc - (command_index+1), (const char**) &argv[command_index+1], &kpm_io)) != KPM_OK)
        {
            kpm_io.log(KPM_VERBOSITY_ERROR, "Failed to uninstall packages (%i)", error);
        }
        else
        {
            kpm_io.log(KPM_VERBOSITY_INFO, "Uninstalled %i packages succesfully.", argc - (command_index+1));
        }
    }
    else if (strcmp(argv[command_index], "upgrade") == 0)
    {
        KPM_UpdateIndex(&kpm, &kpm_io);

        size_t installed_package_count;
        struct InstalledPackage* installed_packages;
        KPM_ListInstalledPackages(&kpm, &installed_package_count, &installed_packages);

        size_t target_count = 0;
        struct InstallTarget* targets = malloc(installed_package_count * sizeof(struct InstallTarget));
        for (int i=0; i < installed_package_count; i++)
        {
            size_t artifact_count;
            struct IndexedArtifact* artifacts;
            if (KPM_ListPackageArtifacts(&kpm, installed_packages[i].repository, installed_packages[i].id, &artifact_count, &artifacts) != KPM_OK || artifact_count == 0)
            {
                kpm_io.log(KPM_VERBOSITY_WARN, "Could not find an artifact for %s", installed_packages[i].id);
                KPM_FreeIndexedArtifactList(artifact_count, artifacts);
                continue;
            }

            if (SemVerCmp(artifacts[0].version, installed_packages[i].version) <= 0)
            {
                kpm_io.log(KPM_VERBOSITY_DEBUG, "Could not find an upgrade artifact for %s", installed_packages[i].id);
                KPM_FreeIndexedArtifactList(artifact_count, artifacts);
                continue;
            }

            targets[target_count].repository = NULL;
            if (artifacts[0].repository != NULL)
            {
                targets[target_count].repository = strdup(artifacts[0].repository);
                targets[target_count].id = strdup(artifacts[0].id);
            }
            targets[target_count].version = NULL;
            KPM_FreeIndexedArtifactList(artifact_count, artifacts);
            target_count++;
        }

        if (target_count == 0)
        {
            kpm_io.log(KPM_VERBOSITY_INFO, "No packages to upgrade.");
            KPM_FreeInstalledPackageList(installed_package_count, installed_packages);
            goto cleanup;
        }

        if ((error = KPM_InstallPackages(&kpm, target_count, targets, &kpm_io)) != KPM_OK)
            kpm_io.log(KPM_VERBOSITY_ERROR, "Failed to upgrade packages (%i)", error);
        else
            kpm_io.log(KPM_VERBOSITY_INFO, "Upgraded %i package(s) succesfully.", installed_package_count);
        KPM_FreeInstalledPackageList(installed_package_count, installed_packages);
        KPM_FreeInstallTargetList(target_count, targets);
    }
    else if (strcmp(argv[command_index], "launch") == 0)
    {
        if (argc - (command_index+1) < 1)
        {
            kpm_io.log(KPM_VERBOSITY_ERROR, "Expected at least 1 argument, received %i", argc - (command_index+1));
            error = KPM_GENERIC_ERROR;
            goto help;
        }

        char* id = argv[command_index+1];
        char* launch_path = asprintf_hd("%s/%s/launch.sh", KPM_PKG_PATH, id);
        kpm_io.log(KPM_VERBOSITY_DEBUG, "Checking for script at %s", launch_path);
        if (access(launch_path, R_OK) != 0)
        {
            kpm_io.log(KPM_VERBOSITY_ERROR, "Could not find package or launch script (checked %s)", launch_path);
            free(launch_path);
            error = KPM_GENERIC_ERROR;
            goto cleanup;
        }
        free(launch_path);

        char** launch_args = malloc(sizeof(char*) * (2 + argc - (command_index + 2)) + 1);
        launch_args[0] = strdup("/bin/sh");
        launch_args[1] = asprintf_hd("%s/%s/launch.sh", KPM_PKG_PATH, id);
        for (int i=0; i < argc - (command_index + 2); i++)
        {
            launch_args[i+2] = strdup(argv[i + command_index + 2]);
        }
        launch_args[(2 + argc - (command_index + 2))] = 0;
        kpm_io.log(KPM_VERBOSITY_DEBUG, "args:");
        for (int i=0; i < (2 + argc - (command_index + 2)) + 1; i++)
        {
            kpm_io.log(KPM_VERBOSITY_DEBUG, "\t- %s", launch_args[i]);
        }

        char* launch_folder = asprintf_hd("%s/%s", KPM_PKG_PATH, id);
        chdir(launch_folder);
        free(launch_folder);
        
        execv("/bin/sh", launch_args);
        return KPM_GENERIC_ERROR;
    }
    else
    {
        kpm_io.log(KPM_VERBOSITY_INFO, "Unknown command \"%s\" specified.\n", argv[command_index]);
        goto help;
err_no_command:
        kpm_io.log(KPM_VERBOSITY_INFO, "No command specified.\n");
        goto help;
help:
kpm_io.log(KPM_VERBOSITY_INFO, "usage: kpm [--help, -h] [--fbink] [-y] {version | add-repo | remove-repo | list-repo | update | search | install | uninstall | upgrade | launch} ... \n\
    --help, -h\tShow this help\n\
    --fbink\tLog to fbink\n\
    -y\tDo not ask for user confirmation\n\
\n\
version:\n\
    Returns the current version of the KPM library and CLI\n\
add-repo:\n\
    Add a repository from one or more URLs\n\
remove-repo:\n\
    Remove one or more repositories given one or more IDs\n\
list-repo:\n\
    List repositories\n\
update:\n\
    Update the package index\n\
search:\n\
    Searches the index for a package given a query\n\
install:\n\
    (Re)Install/Upgrade one or more packages\n\
uninstall:\n\
    Uninstall one or more packages\n\
upgrade:\n\
    Upgrade all installed packages\n\
launch:\n\
    Launch a package given its id by running its launch.sh\n\
");

        goto cleanup;
    }

cleanup:
    io_cleanup();
    KPM_Cleanup(&kpm);
    return error;
}