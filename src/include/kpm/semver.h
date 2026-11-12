#ifndef SEMVER_H
#define SEMVER_H

#include <limits.h>
#include <stdint.h>

#define SEMVER_VERSION_MAX UINT_MAX
#define SEMVER_MAX { .major = UINT_MAX, .minor = UINT_MAX, .patch = UINT_MAX }
#define SEMVER_MIN { .major = 0, .minor = 0, .patch = 0 }

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
 * @brief If a > b it returns 1, if a < b it returns -1, if a == b it returns 0
 * 
 * @param a 
 * @param b 
 * @return int 
 */
static inline uint8_t SemVerCmp(SemVer a, SemVer b)
{
    if (a.major != b.major)
        return a.major > b.major ? 1 : -1;
    if (a.minor != b.minor)
        return a.minor > b.minor ? 1 : -1;
    if (a.patch != b.patch)
        return a.patch > b.patch ? 1 : -1;
    return 0;
}

#endif