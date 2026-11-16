#include "kpm_test_common.h"
#include "kpm.h"
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

KPM *initialise_kpm()
{
    remove("./repo_test.db");
    return KPM_InitialiseAdv("./repo_test.db", "./packages", "./kpm_tmp");
}

char *get_path(int argc, char *argv[])
{
    ASSERT_HD(argc > 0, 1);
    char cur_path[PATH_MAX];
    realpath(dirname(argv[0]), cur_path);
    return strdup(cur_path);
}