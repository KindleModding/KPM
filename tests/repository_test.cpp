#include <cassert>
#include <string>

#include "kpm.hpp"

int main()
{
    const std::string dbPath = "./repo_test.db";

    if (std::filesystem::exists(dbPath))
    {
        std::filesystem::remove(dbPath);
    }
    
    KPM::KPM kpm(dbPath);
    
    assert(kpm.ListRepositories().size() == 0);

    return 0;
}