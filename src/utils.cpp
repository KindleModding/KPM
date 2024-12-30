#include "utils.hpp"
#include "log.hpp"
#include <vector>

bool compareSemverGTOE(const std::string& a, const std::string& b) { // Will return true if a is greater than or equal to b
    std::vector<uint> semverComponentsA;
    std::vector<uint> semverComponentsB;

    size_t startIndex = 0;
    // Parse A
    while (true) {
        size_t foundIndex = a.find('.', startIndex);
        if (foundIndex == a.npos) {
            semverComponentsA.push_back(atoi(a.substr(startIndex, a.length()-startIndex).c_str()));
            break;
        }
        semverComponentsA.push_back(atoi(a.substr(startIndex, foundIndex-startIndex).c_str()));
        startIndex = foundIndex+1;
    }

    // Parse B
    startIndex = 0;
    while (true) {
        size_t foundIndex = b.find('.', startIndex);
        if (foundIndex == b.npos) {
            semverComponentsB.push_back(atoi(b.substr(startIndex, b.length()-startIndex).c_str()));
            break;
        }
        semverComponentsB.push_back(atoi(b.substr(startIndex, foundIndex-startIndex).c_str()));
        startIndex = foundIndex+1;
    }

    for (size_t i=0; i < semverComponentsB.size(); i++) {
        if (i == semverComponentsA.size() || // If we reach the end of A but there are more elements in B, it means A was the same up to those extra elements, and since there are more, A is smaller
            semverComponentsA[i] < semverComponentsB[i]) { // If any component of A is smaller than B since LTR, all previous were the same that means A is smaller
            Log::D("%i LESS THAN %i", semverComponentsA[i], semverComponentsB[i]);
            return false;
        }


        if (semverComponentsA[i] > semverComponentsB[i]) { // Since we are going LTR, the first component being larger means the overall semver is larger
            return true;
        }
    }

    return true; // They are the same OR b is empty
}