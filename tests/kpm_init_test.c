#include "kpm.h"
#include "kpm_test_common.h"

int main()
{
    KPM *kpm = initialise_kpm();
    KPM_Cleanup(kpm);
}