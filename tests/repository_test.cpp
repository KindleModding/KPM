#include <cassert>

#include "kpm.hpp"

int main()
{
    KPM::KPM kpm("./repo_test.db");
    
    assert(kpm.ListRepositories().size() == 0);

    return 0;
}