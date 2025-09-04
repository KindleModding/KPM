#pragma once

struct SemVer
{
    unsigned long int major;
    unsigned long int minor;
    unsigned long int patch;
};

inline int SemVerCmp(struct SemVer a, struct SemVer b)
{
    // @TODO: This is bad
    return a.major - b.major + a.minor - b.minor + a.patch - b.patch;
};