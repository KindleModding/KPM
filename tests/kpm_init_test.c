#include "kpm.h"
#include "common/kpm_test_common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    remove("./repo_test.db");
    KPM kpm = initialise_kpm();
    size_t repositoryCount;
    assert_hd(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert_hd(repositoryCount == 1);
    KPM_Cleanup(&kpm);

    assert_hd(access("./repo_test.db", R_OK) == 0);

    return 0;
}