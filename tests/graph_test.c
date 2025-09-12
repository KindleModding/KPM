#include "dependencies.h"
#include "semver.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main()
{
    struct DependencyGraph graph;
    CreateDependencyGraph(&graph, 30);
    size_t koreader_100 = AddNode(&graph, (struct DependencyNode) {
        .type = NODE_ARTIFACT,
        .connected = NULL,
        .connectedCount = 0,
        .repository = strdup(""),
        .id = strdup("org.kindlemodding.koreader"),
        .min_version = {1, 0, 0},
        .max_version = {1, 0, 0},
    });

    size_t fbink = AddNode(&graph, (struct DependencyNode) {
        .type = NODE_DEPENDENCY,
        .connected = NULL,
        .connectedCount = 0,
        .repository = strdup(""),
        .id = strdup("org.kindlemodding.fbink"),
        .min_version = {0, 1, 2},
        .max_version = {0, 1, 5},
    });

    AddEdge(&graph, koreader_100, fbink);

    size_t fbink_012 = AddNode(&graph, (struct DependencyNode) {
        .type = NODE_ARTIFACT,
        .connected = NULL,
        .connectedCount = 0,
        .repository = strdup(""),
        .id = strdup("org.kindlemodding.fbink"),
        .min_version = {0, 1, 2},
        .max_version = {0, 1, 2}
    });

    size_t fbink_013 = AddNode(&graph, (struct DependencyNode) {
        .type = NODE_ARTIFACT,
        .connected = NULL,
        .connectedCount = 0,
        .repository = strdup(""),
        .id = strdup("org.kindlemodding.fbink"),
        .min_version = {0, 1, 3},
        .max_version = {0, 1, 3}
    });

    size_t fbink_014 = AddNode(&graph, (struct DependencyNode) {
        .type = NODE_ARTIFACT,
        .connected = NULL,
        .connectedCount = 0,
        .repository = strdup(""),
        .id = strdup("org.kindlemodding.fbink"),
        .min_version = {0, 1, 4},
        .max_version = {0, 1, 4}
    });

    fprintf(stderr, "Adding edges\n");
    AddEdge(&graph, fbink, fbink_012);
    AddEdge(&graph, fbink, fbink_013);
    AddEdge(&graph, fbink, fbink_014);


    size_t koreader_dep = AddNode(&graph, (struct DependencyNode) {
        .type = NODE_DEPENDENCY,
        .connected = NULL,
        .connectedCount = 0,
        .repository = strdup(""),
        .id = strdup("org.kindlemodding.koreader"),
        .min_version = {1, 0, 0},
        .max_version = {999999999, 999999999, 999999999}
    });

    fprintf(stderr, "Adding edges\n");
    AddEdge(&graph, fbink_012, koreader_dep);
    AddEdge(&graph, fbink_013, koreader_dep);
    AddEdge(&graph, fbink_014, koreader_dep);
    AddEdge(&graph, koreader_dep, koreader_100);

    size_t index;
    bool result = FindArtifactNode(&graph, "", "org.kindlemodding.koreader", (struct SemVer) { 1, 0, 0 }, &index);
    assert(result);
    assert(index == koreader_100);

    char* rendered = "";
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

    fprintf(stderr, "Freeing graph\n");
    FreeDependencyGraph(&graph);

    return 0;
}
