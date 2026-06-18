#include "kpm.h"
#include "dependencies.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
    struct KPM kpm = {
        .dbPath = "./repo_test.db",
        .pkgPath = "/tmp/packages",
        .maxConnections = 5
    };
    KPM_Initialise(&kpm);

    size_t artifactCount;
    struct IndexedArtifact* artifacts;
    assert(KPM_ListPackageArtifacts(&kpm, NULL, "examplepackage", &artifactCount, &artifacts) == KPM_OK);
    assert(artifactCount > 0);

    size_t installedPackageCount;
    struct InstalledPackage* installedPackages;
    assert(KPM_ListInstalledPackages(&kpm, &installedPackageCount, &installedPackages) == KPM_OK);

    struct DependencyGraph graph;
    CreateDependencyGraph(&graph, 0);
    assert(Internal_ConstructGraphFromArtifact(&kpm, &graph, &artifacts[0]) != -1);
    
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
