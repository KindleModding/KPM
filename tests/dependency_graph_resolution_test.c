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

    fprintf(stderr, "Resolving graph...\n");
    size_t flattenedDependencyCount = 0;
    struct DependencyNode* flattenedDependencies = NULL;
    assert(Internal_ResolveDependencyGraph(&graph, 0, &flattenedDependencyCount, &flattenedDependencies));

    fprintf(stderr, "Resolved:\n");
    for (size_t i=0; i < flattenedDependencyCount; i++)
    {
        fprintf(stderr, "- %s (%u.%u.%u)\n", flattenedDependencies[i].id, flattenedDependencies[i].min_version.major, flattenedDependencies[i].min_version.minor, flattenedDependencies[i].min_version.patch);
    }

    KPM_Cleanup(&kpm);

    return 0;
}
