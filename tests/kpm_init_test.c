#include "kpm.h"
#include "common/kpm_test_common.h"
#include <assert.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    struct KPM kpm = initialise_kpm();
    size_t repositoryCount;
    assert(KPM_ListRepositories(&kpm, &repositoryCount, NULL) == KPM_OK);
    assert(repositoryCount == 1);
    KPM_Cleanup(&kpm);

    assert(access("./repo_test.db", R_OK) == 0);
}