#pragma once

struct SemVer
{
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
};

inline int SemVerCmp(struct SemVer a, struct SemVer b)
{
    // @TODO: This is bad
    return a.major - b.major + a.minor - b.minor + a.patch - b.patch;
};