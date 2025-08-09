#include <cassert>

#include "kpm.hpp"

int main()
{
    KPM::KPM kpm("./repo_test.db");
    
    fprintf(stderr, "Testing repository list is empty\n");
    assert(kpm.ListRepositories().size() == 0);

    fprintf(stderr, "Adding test repository\n");

    try {
        kpm.AddRepository("https://example.com");
    } catch (std::exception& e)
    {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }

    fprintf(stderr, "Testing repository list is not empty\n");
    assert(kpm.ListRepositories().size() == 1);

    fprintf(stderr, "Removing repository\n");
    kpm.RemoveRepository(kpm.ListRepositories()[0]);

    fprintf(stderr, "Testing repository list is empty\n");
    assert(kpm.ListRepositories().size() == 0);

    return 0;
}