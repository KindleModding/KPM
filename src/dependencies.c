#include "dependencies.h"

#include "cjson/cJSON.h"
#include "install.h"
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <stdlib.h>
#include <string.h>

void CreateDependencyGraph(struct DependencyGraph* graph, int count)
{
    graph->nodes = malloc(sizeof(struct DependencyNode) * count);
    graph->allocated = count;
}

void FreeNode(struct DependencyNode* node)
{
    free(node->repository);
    free(node->id);
    free(node->connected);
    free(node->min_version);
    free(node->max_version);
    node->repository = NULL;
    node->id = NULL;
    node->connected = NULL;
    node->connectedCount = 0;
    node->min_version = NULL;
    node->max_version = NULL;
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
    graph->nodes = realloc(graph->nodes, graph->allocated);
}

size_t AddNode(struct DependencyGraph *graph, struct DependencyNode node)
{
    if (graph->nodeCount == graph->allocated)
    {
        ExtendDependencyGraph(graph, graph->allocated/2);
    }

    graph->nodes[graph->nodeCount++] = node;
    return graph->nodeCount;
}

void AddEdge(struct DependencyGraph* graph, size_t firstNodeIndex, size_t nextNodeIndex)
{
    graph->nodes[firstNodeIndex].connectedCount++;
    if (graph->nodes[firstNodeIndex].connected == NULL)
    {
        graph->nodes[firstNodeIndex].connected = malloc(graph->nodes[firstNodeIndex].connectedCount);
    }
    else
    {
        graph->nodes[firstNodeIndex].connected = realloc(graph->nodes[firstNodeIndex].connected, graph->nodes[firstNodeIndex].connectedCount);
    }
    graph->nodes[firstNodeIndex].connected[graph->nodes[firstNodeIndex].connectedCount-1] = nextNodeIndex;
}

bool FindArtifactNode(struct DependencyGraph* graph, char* repository, char* id, struct SemVer* version, size_t* index)
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

        if (repository != graph->nodes[i].repository && (repository == NULL || graph->nodes[i].repository == NULL))
        {
            continue;
        }

        if (repository != NULL && strcmp(graph->nodes[i].repository, repository) != 0)
        {
            continue;
        }

        if (SemVerCmp(*version, *graph->nodes[i].max_version) != 0)
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

            (*targetDependencies)[i].artifact = target->id;
            (*targetDependencies)[i].id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "id"))); // 90% sure I don't need to free this
            (*targetDependencies)[i].repository = NULL;

            if (cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "repository")) != NULL)
            {
                (*targetDependencies[i]).repository = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "repository")));
            }

            cJSON* min = cJSON_GetObjectItem(dependencyJSON, "min");
            cJSON* max = cJSON_GetObjectItem(dependencyJSON, "max");
            if (min != NULL && cJSON_GetArraySize(min) == 3)
            {
                (*targetDependencies[i]).min_version->major = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 0));
                (*targetDependencies[i]).min_version->minor = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 1));
                (*targetDependencies[i]).min_version->patch = cJSON_GetNumberValue(cJSON_GetArrayItem(min, 2));
            }
            else
            {
                (*targetDependencies[i]).min_version = NULL;
            }

            if (max != NULL && cJSON_GetArraySize(max) == 3)
            {
                (*targetDependencies[i]).max_version->major = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 0));
                (*targetDependencies[i]).max_version->minor = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 1));
                (*targetDependencies[i]).max_version->patch = cJSON_GetNumberValue(cJSON_GetArrayItem(max, 2));
            }
            else
            {
                (*targetDependencies[i]).max_version = NULL;
            }
        }

        free(manifestData);
        cJSON_Delete(json);
    }
    else
    {
        KPM_ListArtifactDependencies(kpm, target->url, targetDependencyCount, targetDependencies);
    }

    return KPM_OK;
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
    if ((currentDependency->min_version != NULL && SemVerCmp(*currentDependency->min_version, *targetDependency->min_version) > 0) ||
        (currentDependency->max_version != NULL && SemVerCmp(*currentDependency->max_version, *targetDependency->max_version) <= 0))
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
        .repository = artifact->repository,
        .id = artifact->repository,
        .min_version = &artifact->version,
        .max_version = &artifact->version
    };
    size_t dependencyCount;
    struct ArtifactDependency* dependencies;
    Internal_GetArtifactDependencies(kpm, artifact, &dependencyCount, &dependencies);


    int root = AddNode(graph, node);

    for (size_t i=0; i < dependencyCount; i++)
    {
        struct DependencyNode dependencyNode = {
            .type = NODE_DEPENDENCY,
            .connected = NULL,
            .connectedCount = 0,
            .repository = dependencies[i].repository,
            .id = dependencies[i].id,
            .min_version = NULL,
            .max_version = NULL
        };

        if (dependencies[i].min_version != NULL)
        {
            dependencyNode.min_version = malloc(sizeof(struct SemVer));
            memcpy(dependencyNode.min_version, dependencies[i].min_version, sizeof(struct SemVer));
        }

        if (dependencies[i].max_version != NULL)
        {
            dependencyNode.max_version = malloc(sizeof(struct SemVer));
            memcpy(dependencyNode.max_version, dependencies[i].max_version, sizeof(struct SemVer));
        }

        size_t dependencyNodeId = AddNode(graph, dependencyNode);

        size_t artifactCount;
        struct IndexedArtifact* artifacts;
        KPM_ListPackageArtifacts(kpm, dependencies[i].repository, dependencies[i].id, &artifactCount, &artifacts);
        
        bool validArtifactFound = false;
        for (size_t j=0; j < artifactCount; j++)
        {
            if (SemVerCmp(artifacts[j].version, *dependencies[i].min_version) <= 0 || SemVerCmp(artifacts[j].version, *dependencies[i].max_version) >= 0)
            {
                continue;
            }

            validArtifactFound = true;

            size_t artifactNodeId;
            if (!FindArtifactNode(graph, artifacts[j].repository, artifacts[j].id, &artifacts[j].version, &artifactNodeId))
            {
                struct DependencyNode artifactNode = {
                    .type = NODE_ARTIFACT,
                    .connected = NULL,
                    .connectedCount = 0,
                    .repository = artifacts[j].repository,
                    .id = artifacts[j].id,
                    .min_version = malloc(sizeof(struct SemVer)),
                    .max_version = malloc(sizeof(struct SemVer))
                };
                memcpy(artifactNode.min_version, &artifacts[j].version, sizeof(struct SemVer));
                memcpy(artifactNode.max_version, &artifacts[j].version, sizeof(struct SemVer));
                artifactNodeId = AddNode(graph, artifactNode);
            }

            AddEdge(graph, dependencyNodeId, artifactNodeId);
            AddEdge(graph, artifactNodeId, Internal_ConstructGraphFromArtifact(kpm, graph, &artifacts[j]));
        }

        if (!validArtifactFound)
        {
            return -1; // @TODO: Free stuff, etc
        }
    }


    return root;
}

