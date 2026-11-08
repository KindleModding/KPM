#include "kpm.h"
#include "common/kpm_test_common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    remove("./repo_test.db");
    KPM kpm = initialise_kpm();
    size_t repository_count;
    assert_hd(KPM_ListRepositories(&kpm, &repository_count, NULL) == KPM_OK);
    assert_hd(repository_count == 1);
    KPM_Cleanup(&kpm);

    assert_hd(access("./repo_test.db", R_OK) == 0);

    return 0;
}