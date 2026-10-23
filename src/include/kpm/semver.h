#pragma once

#include <limits.h>
#include <stdint.h>

#define VERSION_MAX UINT_MAX
#define SEMVER_MAX { .major = UINT_MAX, .minor = UINT_MAX, .patch = UINT_MAX }

/**
 * @file semver.h
 * @author Hackerdude (hackerdude@hackerdude.tech)
 * 
 * @copyright Copyright (c) 2026
 * 
 */


 /**
  * @brief Represent a semantic version
  * 
  */
typedef struct
{
    uint32_t major; /**< Major version */
    uint32_t minor; /**< Minor version */
    uint32_t patch; /**< Patch version */
} SemVer;

/**
 * @brief If a > b it returns a number > 0, if a < b it returns a number < 0, if a == b it returns 0
 * 
 * @param a 
 * @param b 
 * @return int 
 */
static inline long SemVerCmp(SemVer a, SemVer b)
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