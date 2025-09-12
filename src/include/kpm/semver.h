#pragma once

#include <stdbool.h>
#include <limits.h>

#define VERSION_MAX UINT_MAX
struct SemVer
{
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
};

/**
 * @brief a - b
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

/*
// Testament to frogginess
inline bool IsSemVerLessThan(struct SemVer a, struct SemVer b)
{
    return SemVerCmp(a, b) < 0;
}

inline bool IsSemVerGreaterThan(struct SemVer a, struct SemVer b)
{
    return SemVerCmp(a, b) > 0;
}

inline bool IsSemVerLessThanEqualTo(struct SemVer a, struct SemVer b)
{
    return SemVerCmp(a, b) <= 0;
}

inline bool IsSemVerGreaterThanEqualTo(struct SemVer a, struct SemVer b)
{
    return SemVerCmp(a, b) >= 0;
}
*/