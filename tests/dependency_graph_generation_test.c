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
    //assert(KPM_ListPackageArtifacts(&kpm, "", "com.kindlemodding.examplepackage", &artifactCount, &artifacts) == KPM_OK);

    struct DependencyGraph graph;
    CreateDependencyGraph(&graph, 0);

    struct IndexedArtifact artifact = {
        .id = "com.kindlemodding.examplepackage",
        .repository = "org.kindlemodding.examplerepo",
        .url = "packages/com.kindlemodding.examplepackage/artifacts/com.kindlemodding.examplepackage_1.2.3_armhf-armel.kpkg",
        .version = { 1, 2, 3 }
    };
    assert(Internal_ConstructGraphFromArtifact(&kpm, &graph, &artifact) != -1);//&artifacts[0]);
    
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

    KPM_Cleanup(&kpm);

    return 0;
}
