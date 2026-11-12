#include "semver.h"
#include <assert.h>

int main()
{
    assert(SemVerCmp(
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 0
        },
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 0
        }
    ) == 0);

    assert(SemVerCmp(
        (SemVer) {
            .major = 1,
            .minor = 0,
            .patch = 0
        },
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 0
        }
    ) > 0);

    assert(SemVerCmp(
        (SemVer) {
            .major = 0,
            .minor = 1,
            .patch = 0
        },
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 0
        }
    ) > 0);

    assert(SemVerCmp(
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 1
        },
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 0
        }
    ) > 0);

    assert(SemVerCmp(
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 0
        },
        (SemVer) {
            .major = 1,
            .minor = 0,
            .patch = 0
        }
    ) < 0);

    assert(SemVerCmp(
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 0
        },
        (SemVer) {
            .major = 0,
            .minor = 1,
            .patch = 0
        }
    ) < 0);

    assert(SemVerCmp(
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 0
        },
        (SemVer) {
            .major = 0,
            .minor = 0,
            .patch = 1
        }
    ) < 0);

    assert(SemVerCmp(
        SEMVER_MAX,
        SEMVER_MAX
    ) == 0);

    assert(SemVerCmp(
        SEMVER_MIN,
        SEMVER_MIN
    ) == 0);

    assert(SemVerCmp(
        SEMVER_MAX,
        SEMVER_MIN
    ) > 0);

    assert(SemVerCmp(
        SEMVER_MIN,
        SEMVER_MAX
    ) < 0);
}