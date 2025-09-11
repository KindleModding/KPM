#pragma once

#include <stdbool.h>
struct SemVer
{
    unsigned long int major;
    unsigned long int minor;
    unsigned long int patch;
};

/**
 * @brief a - b
 * 
 * @param a 
 * @param b 
 * @return int 
 */
static inline int SemVerCmp(struct SemVer a, struct SemVer b)
{
    if (a.major != b.major)
    {
        return a.major - b.major;
    }
    if (a.major == b.major && a.minor != b.minor)
    {
        return a.minor - b.minor;
    }
    if (a.major == b.major && a.minor == b.minor && a.patch != b.patch)
    {
        return a.patch - b.patch;
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