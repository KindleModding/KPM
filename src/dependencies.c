#include "dependencies.h"

#include "cjson/cJSON.h"
#include "install.h"
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void CreateDependencyGraph(struct DependencyGraph* graph, int count)
{
    graph->nodeCount = 0;
    graph->nodes = NULL;
    graph->allocated = 0;

    if (count != 0)
    {
        graph->nodes = malloc(sizeof(struct DependencyNode) * count);
        graph->allocated = count;
    }
}

void FreeNode(struct DependencyNode* node)
{
    free(node->repository);
    free(node->id);
    free(node->connected);
    node->repository = NULL;
    node->id = NULL;
    node->connected = NULL;
    node->connectedCount = 0;
}

void FreeDependencyGraph(struct DependencyGraph* graph)
{
    for (size_t i=0; i < graph->nodeCount; i++)
    {
        FreeNode(&graph->nodes[i]);
    }

    free(graph->nodes);
    graph->nodes = NULL;
    graph->nodeCount = 0;
    graph->allocated = 0;
}

void ExtendDependencyGraph(struct DependencyGraph* graph, int allocate)
{
    graph->allocated += allocate;
    graph->nodes = realloc(graph->nodes, sizeof(struct DependencyNode) * graph->allocated);
}

size_t AddNode(struct DependencyGraph *graph, struct DependencyNode node)
{
    if (graph->nodeCount == graph->allocated)
    {
        ExtendDependencyGraph(graph, graph->allocated/2 + 1);
    }

    graph->nodes[graph->nodeCount++] = node;
    return graph->nodeCount-1;
}

void AddEdge(struct DependencyGraph* graph, size_t firstNodeIndex, size_t nextNodeIndex)
{
    graph->nodes[firstNodeIndex].connectedCount++;
    graph->nodes[firstNodeIndex].connected = realloc(graph->nodes[firstNodeIndex].connected, graph->nodes[firstNodeIndex].connectedCount);
    graph->nodes[firstNodeIndex].connected[graph->nodes[firstNodeIndex].connectedCount-1] = nextNodeIndex;
}

bool FindArtifactNode(struct DependencyGraph* graph, char* repository, char* id, struct SemVer version, size_t* index)
{
    for (size_t i=0; i < graph->nodeCount; i++)
    {
        if (graph->nodes[i].type == NODE_DEPENDENCY)
        {
            continue;
        }

        if (strcmp(graph->nodes[i].id, id) != 0)
        {
            continue;
        }

        if (strcmp(graph->nodes[i].repository, repository) != 0)
        {
            continue;
        }

        if (SemVerCmp(version, graph->nodes[i].min_version) != 0) // NODE_ARTIFACT is a fixed version
        {
            continue;
        }

        *index = i;
        return true;
    }

    return false;
}

bool FindDependencyNode(struct DependencyGraph* graph, char* repository, char* id, struct SemVer min_version, struct SemVer max_version, size_t* index)
{
    for (size_t i=0; i < graph->nodeCount; i++)
    {
        if (graph->nodes[i].type == NODE_ARTIFACT)
        {
            continue;
        }

        if (strcmp(graph->nodes[i].id, id) != 0)
        {
            continue;
        }

        if (strcmp(graph->nodes[i].repository, repository) != 0)
        {
            continue;
        }

        if (SemVerCmp(min_version, graph->nodes[i].min_version) != 0)
        {
            continue;
        }

        if (SemVerCmp(max_version, graph->nodes[i].max_version) != 0)
        {
            continue;
        }

        *index = i;
        return true;
    }

    return false;
}

enum KPMResult Internal_GetArtifactDependencies(struct KPM* kpm, struct IndexedArtifact* target, size_t* targetDependencyCount, struct ArtifactDependency** targetDependencies)
{
    if (strncmp(target->url, "file://", strlen("file://")) == 0)
    {
        char* manifestData;
        enum KPMResult result = Internal_GetManifest(target->url+strlen("file://"), &manifestData, NULL);
        if (result != KPM_OK)
        {
            free(manifestData);
            return result;
        }

        cJSON* json = cJSON_Parse(manifestData);
        if (!cJSON_IsArray(cJSON_GetObjectItem(json, "dependencies")) ||
            cJSON_GetArraySize(cJSON_GetObjectItem(json, "dependencies")) == 0)
        {
            free(manifestData);
            cJSON_Delete(json);

            if (cJSON_GetObjectItem(json, "dependencies") == NULL || cJSON_IsArray(cJSON_GetObjectItem(json, "dependencies")))
            {
                return KPM_OK;
            }
            else {
                return KPM_PARSE_ERROR;
            }
        }

        *targetDependencyCount = cJSON_GetArraySize(cJSON_GetObjectItem(json, "dependencies"));
        targetDependencies = malloc(*targetDependencyCount * sizeof(struct ArtifactDependency));
        for (size_t i=0; i < *targetDependencyCount; i++)
        {
            cJSON* dependencyJSON = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "dependencies"), i);
            if (!cJSON_IsString(cJSON_GetObjectItem(dependencyJSON, "id")))
            {
                free(manifestData);
                cJSON_Delete(json);
                return KPM_PARSE_ERROR;
            }

            (*targetDependencies)[i].artifact = strdup(target->id);
            (*targetDependencies)[i].id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "id"))); // 90% sure I don't need to free this

            if (cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "repository")) != NULL)
            {
                (*targetDependencies[i]).repository = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "repository")));
            }
            else
            {
                (*targetDependencies)[i].repository = strdup("");
            }

            cJSON* min = cJSON_GetObjectItem(dependencyJSON, "min");
            cJSON* max = cJSON_GetObjectItem(dependencyJSON, "max");
            if (min != NULL && cJSON_GetArraySize(min) == 3)
            {
                (*targetDependencies[i]).min_version.major = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 0));
                (*targetDependencies[i]).min_version.minor = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 1));
                (*targetDependencies[i]).min_version.patch = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 2));
            }
            else
            {
                (*targetDependencies[i]).min_version.major = 0;
                (*targetDependencies[i]).min_version.minor = 0;
                (*targetDependencies[i]).min_version.patch = 0;
            }

            if (max != NULL && cJSON_GetArraySize(max) == 3)
            {
                (*targetDependencies[i]).max_version.major = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 0));
                (*targetDependencies[i]).max_version.minor = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 1));
                (*targetDependencies[i]).max_version.patch = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 2));
            }
            else
            {
                (*targetDependencies[i]).max_version.major = VERSION_MAX;
                (*targetDependencies[i]).max_version.minor = VERSION_MAX;
                (*targetDependencies[i]).max_version.patch = VERSION_MAX;
            }
        }

        free(manifestData);
        cJSON_Delete(json);
    }
    else
    {
        return KPM_ListArtifactDependencies(kpm, target->url, targetDependencyCount, targetDependencies);
    }

    return KPM_OK;
}

void stringConcatenate(char* dest, char* src, size_t* length, bool dry)
{
    *length += strlen(src);
    if (!dry)
    {
        strcat(dest, src);
    }
}

void getNameFromNode(struct DependencyGraph* graph, size_t index, char** output, bool declaration)
{
    if (declaration)
    {
        if (graph->nodes[index].type == NODE_ARTIFACT)
        {
            *output = malloc(1 + snprintf(NULL, 0, "%zu[\"%s\n%s\n%zu.%zu.%zu\"]", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch));
            sprintf(*output, "%zu[\"%s\n%s\n%zu.%zu.%zu\"]", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch);
            return;
        }

        if (graph->nodes[index].type == NODE_DEPENDENCY)
        {
            *output = malloc(1 + snprintf(NULL, 0, "%zu([\"%s\n%s\n%zu.%zu.%zu to %zu.%zu.%zu\"])", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch, graph->nodes[index].max_version.major, graph->nodes[index].max_version.minor, graph->nodes[index].max_version.patch));
            sprintf(*output, "%zu([\"%s\n%s\n%zu.%zu.%zu to %zu.%zu.%zu\"])", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch, graph->nodes[index].max_version.major, graph->nodes[index].max_version.minor, graph->nodes[index].max_version.patch);
            return;
        }
    }
    else 
    {
        *output = malloc(1 + snprintf(NULL, 0, "%zu", index));
        sprintf(*output, "%zu", index);
        return;
    }
}

int Internal_RenderGraph(struct DependencyGraph* graph, char* output, bool dry)
{
    size_t outputLength=0;
    stringConcatenate(output, "flowchart\n", &outputLength, dry);

    for (size_t i=0; i < graph->nodeCount; i++)
    {
        char* nodeName = NULL;
        getNameFromNode(graph, i, &nodeName, true);
        stringConcatenate(output, "    ", &outputLength, dry);
        stringConcatenate(output, nodeName, &outputLength, dry);
        stringConcatenate(output, "\n", &outputLength, dry);
        free(nodeName);

        for (size_t j=0; j < graph->nodes[i].connectedCount; j++)
        {
            char* connectedName = NULL;
            getNameFromNode(graph, i, &nodeName, false);
            getNameFromNode(graph, graph->nodes[i].connected[j], &connectedName, false);
            stringConcatenate(output, "    ", &outputLength, dry);
            stringConcatenate(output, nodeName, &outputLength, dry);
            stringConcatenate(output, " --> ", &outputLength, dry);
            stringConcatenate(output, connectedName, &outputLength, dry);
            stringConcatenate(output, "\n", &outputLength, dry);
            free(connectedName);
            free(nodeName);
        }
    }

    return outputLength;
}

void RenderGraph(struct DependencyGraph* graph, char** output)
{
    size_t targetSize = 1 + Internal_RenderGraph(graph, NULL, true);
    *output = malloc(targetSize);
    //memset(*output, 0, targetSize);
    *output[0] = '\0';
    Internal_RenderGraph(graph, *output, false);
}

/**
 * @brief Alters the current dependency to be restricted so that the targetDependency is ALSO met (if possible)
 * 
 * @param currentDependency 
 * @param targetDependency 
 * @return enum KPMResult 
 */
bool Internal_NarrowDependency(struct ArtifactDependency* currentDependency, struct ArtifactDependency* targetDependency)
{
    if (SemVerCmp(currentDependency->min_version, targetDependency->min_version) > 0 ||
        SemVerCmp(currentDependency->max_version, targetDependency->max_version) <= 0)
    {
        return false;
    }
    else
    {
        currentDependency->min_version = targetDependency->min_version;
        currentDependency->max_version = targetDependency->max_version;
        return true;
    }
}

int Internal_ConstructGraphFromArtifact(struct KPM* kpm, struct DependencyGraph* graph, struct IndexedArtifact* artifact)
{
    struct DependencyNode node = {
        .type = NODE_ARTIFACT,
        .connected = NULL,
        .connectedCount = 0,
        .repository = strdup(artifact->repository),
        .id = strdup(artifact->id),
        .min_version = artifact->version,
        .max_version = artifact->version
    };
    // Add this artifact to the graph
    int root = AddNode(graph, node);

    size_t dependencyCount;
    struct ArtifactDependency* dependencies;
    Internal_GetArtifactDependencies(kpm, artifact, &dependencyCount, &dependencies);

    for (size_t i=0; i < dependencyCount; i++)
    {
        // Check if this dependency is already present
        size_t dependencyNodeId;
        if (FindDependencyNode(graph, dependencies[i].repository, dependencies[i].id, dependencies[i].min_version, dependencies[i].max_version, &dependencyNodeId))
        {
            AddEdge(graph, root, dependencyNodeId);
            continue; // We can skip this dependency if it already exists in the graph
        }

        struct DependencyNode dependencyNode = {
            .type = NODE_DEPENDENCY,
            .connected = NULL,
            .connectedCount = 0,
            .repository = strdup(dependencies[i].repository),
            .id = strdup(dependencies[i].id),
            .min_version = dependencies[i].min_version,
            .max_version = dependencies[i].max_version
        };

        dependencyNodeId = AddNode(graph, dependencyNode);
        AddEdge(graph, root, dependencyNodeId);

        size_t artifactCount;
        struct IndexedArtifact* artifacts;
        KPM_ListPackageArtifacts(kpm, dependencies[i].repository, dependencies[i].id, &artifactCount, &artifacts);
        
        bool validArtifactFound = false;
        for (size_t j=0; j < artifactCount; j++)
        {
            if (SemVerCmp(artifacts[j].version, dependencies[i].min_version) < 0 || SemVerCmp(artifacts[j].version, dependencies[i].max_version) >= 0)
            {
                continue;
            }

            validArtifactFound = true;

            size_t artifactNodeId;
            if (!FindArtifactNode(graph, artifacts[j].repository, artifacts[j].id, artifacts[j].version, &artifactNodeId))
            {
                artifactNodeId = Internal_ConstructGraphFromArtifact(kpm, graph, &artifacts[j]);
            }

            AddEdge(graph, dependencyNodeId, artifactNodeId);
            //AddEdge(graph, artifactNodeId, Internal_ConstructGraphFromArtifact(kpm, graph, &artifacts[j]));
        }

        KPM_FreeIndexedArtifactList(artifactCount, artifacts);
        
        if (!validArtifactFound)
        {
            KPM_FreeArtifactDependencyList(dependencyCount, dependencies);
            return -1;
        }
    }

    KPM_FreeArtifactDependencyList(dependencyCount, dependencies);


    return root;
}