#include "kpm/semver.h"
#include <assert.h>

int main(void)
{
    SemVer zero = { 0, 0, 0 };
    SemVer one = { 1, 0, 0 };
    SemVer max = SEMVER_MAX;

    // Dependencies without "max" default to SEMVER_MAX; every real version must sort below it
    assert(SemVerCmp(one, max) < 0);
    assert(SemVerCmp(max, one) > 0);
    assert(SemVerCmp(zero, max) < 0);
    assert(SemVerCmp(max, max) == 0);

    assert(SemVerCmp((SemVer){ 1, 2, 3 }, (SemVer){ 1, 2, 4 }) < 0);
    assert(SemVerCmp((SemVer){ 1, 3, 0 }, (SemVer){ 1, 2, 9 }) > 0);
    assert(SemVerCmp((SemVer){ 2, 0, 0 }, (SemVer){ 1, 9, 9 }) > 0);
    assert(SemVerCmp(one, one) == 0);
    return 0;
}
