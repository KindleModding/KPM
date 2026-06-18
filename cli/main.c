#include "kpm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cli.h"
#include "io.h"

#define CLI_VERSION_MAJOR 1
#define CLI_VERSION_MINOR 0
#define CLI_VERSION_PATCH 0

#ifndef KPM_PLATFORM // We assume an unknown platform is a PC testing build of the CLI
#define KPM_PLATFORM "unknown"
#define KPM_PKG_PATH "/tmp/packages/"
#define KPM_DB_PATH "./repo_test.db"
#else
#define KPM_PKG_PATH "/mnt/us/kmc/kpm/packages"
#define KPM_DB_PATH "/mnt/us/kmc/kpm/kpm.db"
#endif

struct CLIState cli_state = {
    .fbink = false,
    .confirm = true
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
        else if (strcmp(arg, "--sc") == 0) // Undocumented "search command", for internal use only
        {
            cli_state.search_command = true;
            cli_state.fbink = true;
        }
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

    if (cli_state.search_command)
        usleep(500000);

    init_header();
    
    if (command_index == -1)
        goto err_no_command;

    if (strcmp(argv[command_index], "version") == 0)
    {
        kpm_io.log(KPM_VERBOSITY_INFO, "cli v%i.%i.%i", CLI_VERSION_MAJOR, CLI_VERSION_MINOR, CLI_VERSION_PATCH);
        kpm_io.log(KPM_VERBOSITY_INFO, "libkpm v%i.%i.%i", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH);
        kpm_io.log(KPM_VERBOSITY_INFO, "built for platform: %s", KPM_PLATFORM);
#ifndef NDEBUG
        kpm_io.log(KPM_VERBOSITY_INFO, "DEBUG BUILD");
#endif
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
            if ((error = KPM_AddRepository(&kpm, argv[i], &repository)) != KPM_OK)
                kpm_io.log(KPM_VERBOSITY_ERROR, "Could not add repository '%s' (%i)", argv[i], error);
            else
                kpm_io.log(KPM_VERBOSITY_INFO, "Added repository '%s'", repository.id);
            KPM_FreeRepository(&repository);
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
        size_t repository_count;
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
            kpm_io.log(KPM_VERBOSITY_INFO, "  - %s (%s): %s", packages[i].name, packages[i].id, packages[i].description);

        KPM_FreeIndexedPackageList(package_count, packages);
        if (package_count == 0)
            kpm_io.log(KPM_VERBOSITY_INFO, "Could not find any packages for '%s'", query);
        free(query);
    }
    else if (strcmp(argv[command_index], "install") == 0)
    {
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
            kpm_io.log(KPM_VERBOSITY_ERROR, "Failed to install packages (%i)", error);
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
        size_t installed_package_count;
        struct InstalledPackage* installed_packages;
        KPM_ListInstalledPackages(&kpm, &installed_package_count, &installed_packages);

        struct InstallTarget* targets = malloc(installed_package_count * sizeof(struct InstallTarget));
        for (int i=0; i < installed_package_count; i++)
        {
            targets[i].repository = installed_packages[i].repository;
            targets[i].version = NULL;
            targets[i].id = installed_packages[i].id;
        }

        if (installed_package_count == 0)
        {
            kpm_io.log(KPM_VERBOSITY_INFO, "No packages to upgrade.");
            goto cleanup;
        }

        if ((error = KPM_InstallPackages(&kpm, installed_package_count, targets, &kpm_io)) != KPM_OK)
            kpm_io.log(KPM_VERBOSITY_ERROR, "Failed to upgrade packages (%i)", error);
        else
            kpm_io.log(KPM_VERBOSITY_INFO, "Upgraded %i package(s) succesfully.", argc - (command_index+1));
    }
    else
    {
        kpm_io.log(KPM_VERBOSITY_INFO, "Unknown command \"%s\" specified.\n", argv[command_index]);
        goto help;
err_no_command:
        kpm_io.log(KPM_VERBOSITY_INFO, "No command specified.\n");
        goto help;
help:
kpm_io.log(KPM_VERBOSITY_INFO, "usage: kpm [--help, -h] [--fbink] [-y] {version | add-repo | remove-repo | list-repo | update | search | install | uninstall | upgrade} ... \n\
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
");

        goto cleanup;
    }

cleanup:
    io_cleanup();
    KPM_Cleanup(&kpm);
    if (cli_state.search_command)
        usleep(2000000);
    return error;
}