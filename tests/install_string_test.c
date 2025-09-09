#include "kpm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    struct InstallTarget target;
    fprintf(stderr, "Testing only ID\n");
    assert(KPM_ResolveInstallString("fbink", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(target.repository == NULL);
    assert(target.dependency_type == KPM_DT_NONE);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + version equal to\n");
    assert(KPM_ResolveInstallString("fbink==1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(target.repository == NULL);
    assert(target.dependency_type == KPM_DT_EQUAL_TO);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + version GTEQ to\n");
    assert(KPM_ResolveInstallString("fbink>=1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(target.repository == NULL);
    assert(target.repository == NULL);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + version LTEQ to\n");
    assert(KPM_ResolveInstallString("fbink<=1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(target.repository == NULL);
    assert(target.dependency_type == KPM_DT_LESS_THAN_OR_EQUAL_TO);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + version GT to\n");
    assert(KPM_ResolveInstallString("fbink>1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(target.repository == NULL);
    assert(target.dependency_type == KPM_DT_GREATER_THAN);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + version LT to\n");
    assert(KPM_ResolveInstallString("fbink<1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(target.repository == NULL);
    assert(target.dependency_type == KPM_DT_LESS_THAN);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + repository\n");
    assert(KPM_ResolveInstallString("com.kindlemodding.repo/fbink", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(strcmp(target.repository, "com.kindlemodding.repo") == 0);
    assert(target.dependency_type == KPM_DT_NONE);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + repo + version equal to\n");
    assert(KPM_ResolveInstallString("com.kindlemodding.repo/fbink==1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(strcmp(target.repository, "com.kindlemodding.repo") == 0);
    assert(target.dependency_type == KPM_DT_EQUAL_TO);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + repo + version GTEQ to\n");
    assert(KPM_ResolveInstallString("com.kindlemodding.repo/fbink>=1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(strcmp(target.repository, "com.kindlemodding.repo") == 0);
    assert(target.dependency_type == KPM_DT_GREATER_THAN_OR_EQUAL_TO);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + repo + version LTEQ to\n");
    assert(KPM_ResolveInstallString("com.kindlemodding.repo/fbink<=1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(strcmp(target.repository, "com.kindlemodding.repo") == 0);
    assert(target.dependency_type == KPM_DT_LESS_THAN_OR_EQUAL_TO);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + repo + version GT to\n");
    assert(KPM_ResolveInstallString("com.kindlemodding.repo/fbink>1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(strcmp(target.repository, "com.kindlemodding.repo") == 0);
    assert(target.dependency_type == KPM_DT_GREATER_THAN);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    KPM_FreeInstallTarget(&target);
    fprintf(stderr, "Testing ID + repo + version LT to\n");
    assert(KPM_ResolveInstallString("com.kindlemodding.repo/fbink<1.2.3", &target) == KPM_OK);

    fprintf(stderr, "\trepository: %s\n", target.repository);
    fprintf(stderr, "\tid: %s\n", target.id);
    fprintf(stderr, "\tDT: %i\n", target.dependency_type);
    fprintf(stderr, "\tVM: %zu\n", target.version.major);
    fprintf(stderr, "\tVm: %zu\n", target.version.minor);
    fprintf(stderr, "\tVp: %zu\n", target.version.patch);
    assert(strcmp(target.id, "fbink") == 0);
    assert(strcmp(target.repository, "com.kindlemodding.repo") == 0);
    assert(target.dependency_type == KPM_DT_LESS_THAN);
    assert(target.version.major == 1);
    assert(target.version.minor == 2);
    assert(target.version.patch == 3);

    return 0;
}