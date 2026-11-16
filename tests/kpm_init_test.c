#include "kpm.h"
#include "kpm_test_common.h"
#include <assert.h>

int main()
{
    KPM *kpm = KPM_Initialise();
    ASSERT_HD(kpm != NULL, 1);
    KPM_Cleanup(kpm);
}