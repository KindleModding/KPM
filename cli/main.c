#include "kpm.h"
#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "logging.h"

#define CLI_VERSION_MAJOR 1
#define CLI_VERSION_MINOR 0
#define CLI_VERSION_PATCH 0

struct CLIState cli_state = {
    .fbink = false,
    .dry = false,
    .confirm = true
};

int main(int argc, char* argv[])
{
    int error = KPM_OK;

    logging.log(KPM_VERBOSITY_INFO, "Kindle Package Manager v%i.%i.%i", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH);
    logging.log(KPM_VERBOSITY_INFO, "Created by Hackerdude (https://hackerdude.tech)\n");

    if (argc <= 1)
        goto err_no_command;

    int command_index = 1;
    for (int i=1; i < argc; i++)
    {
        command_index = i;
        char* arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
            goto help;
        else if (strcmp(arg, "--fbink") == 0)
            cli_state.fbink = true;
        else if (strcmp(arg, "--dry") == 0)
            cli_state.dry = true;
        else if (strcmp(arg, "-y") == 0)
            cli_state.confirm = false;
        else if (strncmp(arg, "--", 2) == 0 || strncmp(arg, "-", 1) == 0)
        {
            logging.log(KPM_VERBOSITY_INFO, "Could not parse argument [%s]\n\n", arg);
            goto help;
        }
        else
            break;
    }

    struct KPM kpm = {
        .maxConnections = 5, // @TODO
        .pkgPath = "/tmp/packages/"
    };
    KPM_Initialise(&kpm, "./repo-test.db");

    if (strcmp(argv[command_index], "version") == 0)
    {
        logging.log(KPM_VERBOSITY_INFO, "cli v%i.%i.%i", CLI_VERSION_MAJOR, CLI_VERSION_MINOR, CLI_VERSION_PATCH);
        logging.log(KPM_VERBOSITY_INFO, "libkpm v%i.%i.%i", KPM_VERSION_MAJOR, KPM_VERSION_MINOR, KPM_VERSION_PATCH);
    }
    else if (strcmp(argv[command_index], "add-repo") == 0)
    {
        if (argc-(command_index+1) == 0)
        {
            error = 1;
            logging.log(KPM_VERBOSITY_ERROR, "No repositories were passed to KPM to add\n");
            goto help;
        }

        error = KPM_OK;
        struct Repository repository;
        for (int i = command_index+1; i < argc; i++)
        {
            if ((error = KPM_AddRepository(&kpm, argv[i], &repository)) != KPM_OK)
                logging.log(KPM_VERBOSITY_ERROR, "Could not add repository '%s' (%i)", argv[i], error);
            else
                logging.log(KPM_VERBOSITY_INFO, "Added repository '%s'", repository.id);
        }

        if (error == KPM_OK)
            logging.log(KPM_VERBOSITY_INFO, "\nAdded %i repositories. Package index will not update until 'kpm update' is run.", argc-(command_index+1));
        else
            logging.log(KPM_VERBOSITY_WARN, "There were some errors. Package index will not update until 'kpm update' is run.");
    }
    else if (strcmp(argv[command_index], "remove-repo") == 0)
    {
        if (argc-(command_index+1) == 0)
        {
            error = 1;
            logging.log(KPM_VERBOSITY_ERROR, "No repositories were passed to KPM to remove\n");
            goto help;
        }

        error = KPM_OK;
        for (int i = command_index+1; i < argc; i++)
        {
            if (KPM_GetRepository(&kpm, argv[i], NULL) != KPM_OK || (error = KPM_RemoveRepository(&kpm, argv[i])) != KPM_OK)
                logging.log(KPM_VERBOSITY_ERROR, "Could not remove repository '%s' (%i)", argv[i], error);
            else
                logging.log(KPM_VERBOSITY_INFO, "Removed repository '%s'", argv[i]);
        }

        if (error == KPM_OK)
            logging.log(KPM_VERBOSITY_INFO, "\nRemoved %i repositories. Package index will not update until 'kpm update' is run.", argc-(command_index+1));
        else
            logging.log(KPM_VERBOSITY_WARN, "There were some errors. Package index will not update until 'kpm update' is run.");
    }
    else if (strcmp(argv[command_index], "list-repo") == 0)
    {
        size_t repository_count;
        struct Repository* repositories;
        if ((error = KPM_ListRepositories(&kpm, &repository_count, &repositories)) != KPM_OK)
            return error;

        logging.log(KPM_VERBOSITY_INFO, "Repositories:");
        if (repository_count == 0)
            logging.log(KPM_VERBOSITY_INFO, "There are no repositories to show");
        for (int i=0; i < repository_count; i++)
            logging.log(KPM_VERBOSITY_INFO, "  - %s - %s (%s)", repositories[i].id, repositories[i].name, repositories[i].url);
    }
    else
    {
        logging.log(KPM_VERBOSITY_INFO, "Unknown argv[command_index] \"%s\" specified.\n", argv[command_index]);
        goto help;
err_no_command:
        logging.log(KPM_VERBOSITY_INFO, "No argv[command_index] specified.\n");
        goto help;
help:
logging.log(KPM_VERBOSITY_INFO, "usage: kpm [--help, -h] [--fbink] [--dry] [-y] {version | add-repo | remove-repo | list-repo | update | search} ... \n\
    --help, -h\tShow this help\n\
    --fbink\tLog to fbink\n\
    --dry\tDry run only\n\
    -y\tDo not ask for user confirmation\n\
\n\
version:\n\
    Returns the current version of the KPM library and CLI\n\
\n\
add-repo:\n\
    Add a repository from one or more URLs\n\
remove-repo:\n\
    Remove one or more repositories given one or more IDs\n\
list-repo:\n\
    List repositories\n\
update:\n\
    Update the package index\n\
search:\n\
    Searches for a package given a query\n\
");
        return error;
    }

    return error;
}