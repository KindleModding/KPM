#include "kpm/kpm.h"
#include <stdlib.h>

/**
 * @brief Free the properties of a package - WILL NOT FREE THE POINTER ITSELF
 * 
 * @param package The package to free the properties of
 */
void KPM_FreeIndexedPackage(struct IndexedPackage* package)
{
    free(package->repository);
    free(package->id);
    free(package->name);
    free(package->description);
    free(package->author);
    free(package->icon);

    package->repository = NULL;
    package->id = NULL;
    package->name = NULL;
    package->description = NULL;
    package->author = NULL;
    package->icon = NULL;
}

/**
 * @brief Free an allocated list of packages - such as returned by KPM_ListRepositoryPackages
 * 
 * @param packageCount The number of packages in the array
 * @param packages The package array
 */
void KPM_FreeIndexedPackageList(size_t packageCount, struct IndexedPackage* packages)
{
    for (size_t i=0; i < packageCount; i++)
    {
        KPM_FreeIndexedPackage(&packages[i]);
    }
    free(packages);
}