#include "kpm.h"
#include "dependencies.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    struct KPM kpm;
    KPM_Initialise(&kpm, "./repo_test.db");

    size_t artifactCount;
    struct IndexedArtifact* artifacts;
    assert(KPM_ListPackageArtifacts(&kpm, "", "com.kindlemodding.examplepackage", &artifactCount, &artifacts) == KPM_OK);

    size_t installedPackageCount;
    struct InstalledPackage* installedPackages;
    assert(KPM_ListInstalledPackages(&kpm, &installedPackageCount, &installedPackages) == KPM_OK);

    struct DependencyGraph graph;
    CreateDependencyGraph(&graph, 0);
    assert(Internal_ConstructGraphFromArtifact(&kpm, &graph, &artifacts[0], installedPackageCount, installedPackages) != -1);
    
    char* rendered;
    fprintf(stderr, "Rendering graph\n");
    RenderGraph(&graph, &rendered);

    fprintf(stderr, "Openning file\n");
    FILE* file = fopen("./rendered.txt", "w");
    fprintf(stderr, "Writing to file\n");
    fwrite(rendered, strlen(rendered), 1, file);
    fprintf(stderr, "Closing file\n");
    fclose(file);
    fprintf(stderr, "file written.\n");
    fprintf(stderr, "\n\n%s\n\n", rendered);

    FreeDependencyGraph(&graph);
    KPM_FreeIndexedArtifactList(artifactCount, artifacts);
    KPM_Cleanup(&kpm);

    return 0;
}
