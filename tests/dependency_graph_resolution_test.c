#include "kpm.h"
#include "dependencies.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void statusCallback(enum Verbosity verbosity, char * format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

int main()
{
    struct KPM kpm;
    KPM_Initialise(&kpm, "./repo_test.db");

    size_t artifactCount;
    struct IndexedArtifact* artifacts;
    assert(KPM_ListPackageArtifacts(&kpm, NULL, "com.kindlemodding.examplepackage", &artifactCount, &artifacts) == KPM_OK);

    size_t installedPackageCount;
    struct InstalledPackage* installedPackages;
    assert(KPM_ListInstalledPackages(&kpm, &installedPackageCount, &installedPackages) == KPM_OK);

    struct DependencyGraph graph;
    CreateDependencyGraph(&graph, 0);
    
    assert(Internal_ConstructGraphFromArtifact(&kpm, &graph, &artifacts[0]) != -1);
    KPM_FreeIndexedArtifactList(artifactCount, artifacts);
    
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

    fprintf(stderr, "Resolving graph...\n");
    size_t traversedDependencyCount = 0;
    size_t* traversedDependencies = NULL;
    struct KPMLogging logger = 
    {
        .log = statusCallback

    };
    assert(Internal_ResolveDependencyGraph(&graph, 0, &traversedDependencyCount, &traversedDependencies, &logger));

    fprintf(stderr, "Traversed:\n");
    for (size_t i=0; i < traversedDependencyCount; i++)
    {
        if (traversedDependencies[i] == 0)
        {
            fprintf(stderr, "%s (%u.%u.%u - %u.%u.%u)\n", graph.nodes[traversedDependencies[i]].id, graph.nodes[traversedDependencies[i]].min_version.major, graph.nodes[traversedDependencies[i]].min_version.minor, graph.nodes[traversedDependencies[i]].min_version.patch, graph.nodes[traversedDependencies[i]].max_version.major, graph.nodes[traversedDependencies[i]].max_version.minor, graph.nodes[traversedDependencies[i]].max_version.patch);
        }

        if (graph.nodes[traversedDependencies[i]].type == NODE_DEPENDENCY)
        {
            fprintf(stderr, "  - %s (%u.%u.%u - %u.%u.%u)\n", graph.nodes[traversedDependencies[i]].id, graph.nodes[traversedDependencies[i]].min_version.major, graph.nodes[traversedDependencies[i]].min_version.minor, graph.nodes[traversedDependencies[i]].min_version.patch, graph.nodes[traversedDependencies[i]].max_version.major, graph.nodes[traversedDependencies[i]].max_version.minor, graph.nodes[traversedDependencies[i]].max_version.patch);
        }
        else
        {
            fprintf(stderr, "  - %s (%u.%u.%u)\n", graph.nodes[traversedDependencies[i]].id, graph.nodes[traversedDependencies[i]].min_version.major, graph.nodes[traversedDependencies[i]].min_version.minor, graph.nodes[traversedDependencies[i]].min_version.patch);
        }
    }

    FreeDependencyGraph(&graph);
    free(traversedDependencies);
    KPM_Cleanup(&kpm);

    return 0;
}
