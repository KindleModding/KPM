#pragma once

#include <stdbool.h>
#include <limits.h>

#define VERSION_MAX UINT_MAX
#define SEMVER_MAX { .major = UINT_MAX, .minor = UINT_MAX, .patch = UINT_MAX }

struct SemVer
{
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
};

/**
 * @brief If a > b it returns a number > 0, if a < b it returns a number < 0, if a == b it returns 0
 * 
 * @param a 
 * @param b 
 * @return int 
 */
static inline long SemVerCmp(struct SemVer a, struct SemVer b)
{
    if (a.major != b.major)
    {
        return (long) a.major - (long) b.major;
    }
    if (a.major == b.major && a.minor != b.minor)
    {
        return (long) a.minor - (long) b.minor;
    }
    if (a.major == b.major && a.minor == b.minor && a.patch != b.patch)
    {
        return (long) a.patch - (long) b.patch;
    }
    return 0;
}