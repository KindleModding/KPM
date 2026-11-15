#include "kpm.h"

int main()
{
    KPM *kpm = KPM_Initialise();
    KPM_Cleanup(kpm);
}