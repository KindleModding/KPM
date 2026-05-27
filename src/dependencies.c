#include "dependencies.h"

#include "cjson/cJSON.h"
#include "install.h"
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a Dependency Graph object
 * 
 * @param graph Pointer to an empty graph struct to initialise
 * @param count Number of nodes to allocate in the graph object initially
 */
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

/**
 * @brief Free a node object
 * 
 * @param node The node to free
 */
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

/**
 * @brief Free a dependency graph and all the nodes in it
 * 
 * @param graph The graph to free
 */
void FreeDependencyGraph(struct DependencyGraph* graph)
{
    for (NodeIndex_t i=0; i < graph->nodeCount; i++)
    {
        FreeNode(&graph->nodes[i]);
    }

    free(graph->nodes);
    graph->nodes = NULL;
    graph->nodeCount = 0;
    graph->allocated = 0;
}

/**
 * @brief Allocate more space for a dependency grpah
 * 
 * @param graph The dependency graph object
 * @param allocate How many nodes to expand it by
 */
void ExtendDependencyGraph(struct DependencyGraph* graph, int allocate)
{
    graph->allocated += allocate;
    graph->nodes = realloc(graph->nodes, sizeof(struct DependencyNode) * graph->allocated);
}

/**
 * @brief Add a node to the dependency graph
 * 
 * @param graph 
 * @param node 
 * @return NodeIndex_t 
 */
NodeIndex_t AddNode(struct DependencyGraph *graph, struct DependencyNode node)
{
    if (graph->nodeCount == graph->allocated)
    {
        ExtendDependencyGraph(graph, graph->allocated/2 + 1);
    }

    graph->nodes[graph->nodeCount++] = node;
    return graph->nodeCount-1;
}

/**
 * @brief Add a connection between two nodes in the graph
 * 
 * @param graph 
 * @param firstNodeIndex 
 * @param nextNodeIndex 
 */
void AddEdge(struct DependencyGraph* graph, NodeIndex_t firstNodeIndex, NodeIndex_t nextNodeIndex)
{
    graph->nodes[firstNodeIndex].connectedCount++;
    graph->nodes[firstNodeIndex].connected = realloc(graph->nodes[firstNodeIndex].connected, graph->nodes[firstNodeIndex].connectedCount * sizeof(size_t));
    
    // Insert it into the list such that dependency artifacts are ordered newest to oldest
    NodeIndex_t insertionIndex = graph->nodes[firstNodeIndex].connectedCount-1;
    if (graph->nodes[nextNodeIndex].type == NODE_ARTIFACT)
    {
        while (insertionIndex > 0)
        {
            if (SemVerCmp(graph->nodes[graph->nodes[firstNodeIndex].connected[insertionIndex-1]].min_version, graph->nodes[nextNodeIndex].min_version) > 0)
            {
                break;
            }

            graph->nodes[firstNodeIndex].connected[insertionIndex] = graph->nodes[firstNodeIndex].connected[insertionIndex-1];
            insertionIndex--;
        }
    }

    graph->nodes[firstNodeIndex].connected[insertionIndex] = nextNodeIndex;
}

/**
 * @brief Add an edge to the graph such that this is the first edge for the given firstNode
 * 
 * @param graph 
 * @param firstNodeIndex 
 * @param nextNodeIndex 
 */
void AddFirstEdge(struct DependencyGraph* graph, NodeIndex_t firstNodeIndex, NodeIndex_t nextNodeIndex)
{
    graph->nodes[firstNodeIndex].connectedCount++;
    graph->nodes[firstNodeIndex].connected = realloc(graph->nodes[firstNodeIndex].connected, graph->nodes[firstNodeIndex].connectedCount * sizeof(size_t));
    
    NodeIndex_t insertionIndex = graph->nodes[firstNodeIndex].connectedCount-1;
    if (graph->nodes[nextNodeIndex].type == NODE_ARTIFACT)
    {
        while (insertionIndex > 0)
        {
            graph->nodes[firstNodeIndex].connected[insertionIndex] = graph->nodes[firstNodeIndex].connected[insertionIndex-1];
            insertionIndex--;
        }
    }

    graph->nodes[firstNodeIndex].connected[insertionIndex] = nextNodeIndex;
}

/**
 * @brief Search the graph for an artifact
 * 
 * @param graph 
 * @param repository 
 * @param id 
 * @param version 
 * @param index The index of the artifact's node
 * @return true Returned if the node is found and index is set
 * @return false Returned if the node is not found and index is not set
 */
bool FindArtifactNode(struct DependencyGraph* graph, char* repository, char* id, struct SemVer version, NodeIndex_t* index)
{
    for (NodeIndex_t i=0; i < graph->nodeCount; i++)
    {
        if (graph->nodes[i].type == NODE_DEPENDENCY)
        {
            continue;
        }

        if (strcmp(graph->nodes[i].id, id) != 0)
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

/**
 * @brief Search the graph for a dependency node
 * 
 * @param graph 
 * @param repository 
 * @param id 
 * @param min_version 
 * @param max_version 
 * @param index 
 * @return true 
 * @return false 
 */
bool FindDependencyNode(struct DependencyGraph* graph, char* id, struct SemVer min_version, struct SemVer max_version, NodeIndex_t* index)
{
    for (NodeIndex_t i=0; i < graph->nodeCount; i++)
    {
        if (graph->nodes[i].type == NODE_ARTIFACT)
        {
            continue;
        }

        if (strcmp(graph->nodes[i].id, id) != 0)
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

/**
 * @brief Given a specified artifact, get the list of dependencies it needs
 * 
 * @param kpm 
 * @param target 
 * @param targetDependencyCount 
 * @param targetDependencies 
 * @return enum KPMResult 
 */
enum KPMResult Internal_GetArtifactDependencies(struct KPM* kpm, struct IndexedArtifact* target, size_t* targetDependencyCount, struct ArtifactDependency** targetDependencies)
{
    if (strlen(target->url) >= strlen("file://") && strncmp(target->url, "file://", strlen("file://")) == 0)
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
            if (cJSON_GetObjectItem(json, "dependencies") == NULL || cJSON_IsArray(cJSON_GetObjectItem(json, "dependencies")))
            {
                free(manifestData);
                cJSON_Delete(json);
                return KPM_OK;
            }
            else {
                free(manifestData);
                cJSON_Delete(json);
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

            (*targetDependencies)[i].artifact_repository = strdup(target->id);
            (*targetDependencies)[i].artifact_id = strdup(target->id);
            (*targetDependencies)[i].artifact_url = strdup(target->id);
            (*targetDependencies)[i].id = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(dependencyJSON, "id"))); // 90% sure I don't need to free this

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
        enum KPMResult status = KPM_OK;

        // If there is no repository then the package was installed locally
        if (strlen(target->repository) == 0)
        {
            struct InstalledPackage installedPackage;
            KPM_GetInstalledPackage(kpm, target->id, &installedPackage);
            if (SemVerCmp(installedPackage.version, target->version) == 0)
            {
                size_t extraDependencyCount;
                struct InstalledDependency* extraDependencies;
                if ((status = KPM_ListInstalledPackageDependencies(kpm, target->id, &extraDependencyCount, &extraDependencies)) != KPM_OK)
                {
                    KPM_FreeInstalledPackageDependencyList(extraDependencyCount, extraDependencies);
                    return status;
                }

                *targetDependencyCount = extraDependencyCount;
                *targetDependencies = malloc(extraDependencyCount * sizeof(struct ArtifactDependency));
                for (size_t i=0; i < *targetDependencyCount; i++)
                {
                    (*targetDependencies)[i].artifact_id = strdup(target->id);
                    (*targetDependencies)[i].artifact_repository = strdup(target->repository);
                    (*targetDependencies)[i].artifact_url = strdup(target->url);
                    (*targetDependencies)[i].id = strdup(extraDependencies[i].dependency_id);
                    (*targetDependencies)[i].min_version = extraDependencies[i].min_version;
                    (*targetDependencies)[i].max_version = extraDependencies[i].max_version;
                }
                KPM_FreeInstalledPackageDependencyList(extraDependencyCount, extraDependencies);
            }
            KPM_FreeInstalledPackage(&installedPackage);
        }

        // Otherwise... nah
        if ((status = KPM_ListArtifactDependencies(kpm, target->repository, target->id, target->url, targetDependencyCount, targetDependencies)) != KPM_OK)
        {
            return status;
        }
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

void getNameFromNode(struct DependencyGraph* graph, NodeIndex_t index, char** output, bool declaration)
{
    if (declaration)
    {
        if (graph->nodes[index].type == NODE_ARTIFACT)
        {
            asprintf(output, "%zu[\"%s\n%s\n%u.%u.%u\"]", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch);
            return;
        }

        if (graph->nodes[index].type == NODE_DEPENDENCY)
        {
            asprintf(output, "%zu([\"%s\n%s\n%u.%u.%u to %zu.%zu.%zu\"])", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch, graph->nodes[index].max_version.major, graph->nodes[index].max_version.minor, graph->nodes[index].max_version.patch);
            return;
        }
    }
    else 
    {
        asprintf(output, "%zu", index);
        return;
    }
}

int Internal_RenderGraph(struct DependencyGraph* graph, char* output, bool dry)
{
    size_t outputLength=0;
    stringConcatenate(output, "flowchart TD\n", &outputLength, dry);

    for (NodeIndex_t i=0; i < graph->nodeCount; i++)
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

    size_t dependencyCount = 0;
    struct ArtifactDependency* dependencies = NULL;
    if (Internal_GetArtifactDependencies(kpm, artifact, &dependencyCount, &dependencies) != KPM_OK)
    {
        KPM_FreeArtifactDependencyList(dependencyCount, dependencies);
        fprintf(stderr, "Could not list dependencies for %s (%u.%u.%u)\n", artifact->id, artifact->version.major, artifact->version.minor, artifact->version.patch);
        return -1;
    }
    
    for (size_t i=0; i < dependencyCount; i++)
    {
        // Check if this dependency is already present
        NodeIndex_t dependencyNodeId;
        if (FindDependencyNode(graph, dependencies[i].id, dependencies[i].min_version, dependencies[i].max_version, &dependencyNodeId))
        {
            AddEdge(graph, root, dependencyNodeId);
            continue; // We can skip this dependency if it already exists in the graph
        }

        struct DependencyNode dependencyNode = {
            .type = NODE_DEPENDENCY,
            .connected = NULL,
            .connectedCount = 0,
            .repository = NULL,
            .id = strdup(dependencies[i].id),
            .min_version = dependencies[i].min_version,
            .max_version = dependencies[i].max_version
        };

        dependencyNodeId = AddNode(graph, dependencyNode);
        AddEdge(graph, root, dependencyNodeId);

        bool validArtifactFound = false;

        // Add indexed artifacts that match the dependency
        size_t artifactCount;
        struct IndexedArtifact* artifacts;
        KPM_ListPackageArtifacts(kpm, NULL, dependencies[i].id, &artifactCount, &artifacts);
        
        for (size_t j=0; j < artifactCount; j++)
        {
            if (SemVerCmp(artifacts[j].version, dependencies[i].min_version) < 0 || SemVerCmp(artifacts[j].version, dependencies[i].max_version) >= 0)
            {
                continue;
            }

            validArtifactFound = true;

            NodeIndex_t artifactNodeId;
            if (!FindArtifactNode(graph, artifacts[j].repository, artifacts[j].id, artifacts[j].version, &artifactNodeId))
            {
                artifactNodeId = Internal_ConstructGraphFromArtifact(kpm, graph, &artifacts[j]);
                if (artifactNodeId == (NodeIndex_t) -1)
                {
                    KPM_FreeArtifactDependencyList(dependencyCount, dependencies);
                    return -1;
                }
            }

            AddEdge(graph, dependencyNodeId, artifactNodeId);
            //AddEdge(graph, artifactNodeId, Internal_ConstructGraphFromArtifact(kpm, graph, &artifacts[j]));
        }
        KPM_FreeIndexedArtifactList(artifactCount, artifacts);

        // Add locally installed package if present
        struct InstalledPackage installedPackage;
        KPM_GetInstalledPackage(kpm, dependencies[i].id, &installedPackage);
        if (installedPackage.id != NULL && SemVerCmp(installedPackage.version, dependencies[i].min_version) >= 0 && SemVerCmp(installedPackage.version, dependencies[i].max_version) < 0)
        {
            validArtifactFound = true;

            NodeIndex_t artifactNodeId;
            if (!FindArtifactNode(graph, "", installedPackage.id, installedPackage.version, &artifactNodeId))
            {
                struct IndexedArtifact fakeArtifact = {
                    .id = strdup(installedPackage.id),
                    .repository = installedPackage.repository,
                    .url = strdup(""),
                    .version = installedPackage.version
                };
                artifactNodeId = Internal_ConstructGraphFromArtifact(kpm, graph, &fakeArtifact);
                KPM_FreeIndexedArtifact(&fakeArtifact);
                if (artifactNodeId == (NodeIndex_t) -1)
                {
                    KPM_FreeArtifactDependencyList(dependencyCount, dependencies);
                    return -1;
                }
            }

            AddFirstEdge(graph, dependencyNodeId, artifactNodeId);
            //AddEdge(graph, artifactNodeId, Internal_ConstructGraphFromArtifact(kpm, graph, &artifacts[j]));
        }
        KPM_FreeInstalledPackage(&installedPackage);
        
        if (!validArtifactFound)
        {
            fprintf(stderr, "Could not find artifact for %s (%u.%u.%u - %u.%u.%u)\n", dependencyNode.id, dependencyNode.min_version.major, dependencyNode.min_version.minor, dependencyNode.min_version.patch, dependencyNode.max_version.major, dependencyNode.max_version.minor, dependencyNode.max_version.patch);
            KPM_FreeArtifactDependencyList(dependencyCount, dependencies);
            return -1;
        }
    }

    KPM_FreeArtifactDependencyList(dependencyCount, dependencies);
    return root;
}

bool CheckIdInDependencyList(char* id, NodeIndex_t* needleIndex, size_t haystackSize, struct DependencyNode* haystack)
{
    for (*needleIndex = 0; *needleIndex < haystackSize; (*needleIndex)++)
    {
        if (strcmp(id, haystack[*needleIndex].id) == 0)
        {
            return true;
        }
    }

    return false;
}

void Internal_ArrayAddNode(size_t* traversedNodeCount, NodeIndex_t** traversedNodes, NodeIndex_t node)
{
    (*traversedNodeCount)++;
    *traversedNodes = realloc(*traversedNodes, (*traversedNodeCount) * sizeof(NodeIndex_t));
    (*traversedNodes)[*traversedNodeCount - 1] = node;
}

bool Internal_ResolveDependencyGraph(struct DependencyGraph* graph, NodeIndex_t root, size_t* traversedNodeCount, NodeIndex_t** traversedNodes, struct KPMLogging* kpmLogging)
{
    *traversedNodes = NULL;
    *traversedNodeCount = 0;

    NodeIndex_t currentNode = root;
    for (;;)
    {
        kpmLogging->log(KPM_VERBOSITY_DEBUG, "Traversing node: %zu - %s", currentNode, graph->nodes[currentNode].id);
        // Check if this artifact is already traversed
        bool alreadyTraversed=false;
        for (size_t i=0; i < *traversedNodeCount; i++)
        {
            if ((*traversedNodes)[i] == currentNode)
            {
                kpmLogging->log(KPM_VERBOSITY_DEBUG, "Already traversed!");
                alreadyTraversed=true;
                break;
            }
        }

        Internal_ArrayAddNode(traversedNodeCount, traversedNodes, currentNode);

        kpmLogging->log(KPM_VERBOSITY_DEBUG, "Currently traversed:");
        for (size_t i=0; i < *traversedNodeCount; i++)
        {
            kpmLogging->log(KPM_VERBOSITY_DEBUG, "- %i\t%s (%u.%u.%u)", (*traversedNodes)[i], graph->nodes[(*traversedNodes)[i]].id, graph->nodes[(*traversedNodes)[i]].min_version.major, graph->nodes[(*traversedNodes)[i]].min_version.minor, graph->nodes[(*traversedNodes)[i]].min_version.patch);
        }

        if (!alreadyTraversed)
        {
            // Ensure this NEW node doesn't conflict with ANY previously traversed artifacts
            bool conflicts = false;
            NodeIndex_t conflictingArtifactId;
            NodeIndex_t conflictingDependencyId;
            for (size_t i=0; i < *traversedNodeCount; i++)
            {
                if (graph->nodes[(*traversedNodes)[i]].type != NODE_ARTIFACT || (*traversedNodes)[i] == root)
                {
                    continue;
                }

                conflictingArtifactId = (*traversedNodes)[i];
                conflictingDependencyId = (*traversedNodes)[i-1];
                if (currentNode != conflictingArtifactId && strcmp(graph->nodes[currentNode].id, graph->nodes[conflictingArtifactId].id) == 0)
                {
                    kpmLogging->log(KPM_VERBOSITY_DEBUG, "Node conflicts with: %zu - %s", conflictingArtifactId, graph->nodes[conflictingArtifactId].id);
                    // Don't need to check if version conflicts because if the node wasn't already traversed
                    // It must be conflicting since it wasn't merged
                    conflicts=true;
                    break;
                }
            }

            // Find an alternative artifact that matches the conflicting artifact
            if (conflicts)
            {
                NodeIndex_t dependencyId = (*traversedNodes)[*traversedNodeCount - 2]; // Dependency for current artifact (traversedNodeCount must be at least 2 for a conflict to occur) (REMEMBER THAT -1 IS THE CURRENT -> -2 IS THE LAST!)
                // Check if dependency has an artifact that matches the already resolved conflicting one
                for (size_t i=0; i < graph->nodes[dependencyId].connectedCount; i++)
                {
                    if (conflictingArtifactId == graph->nodes[dependencyId].connected[i]) // We can compare nodes directly bc artifact merging
                    {
                        (*traversedNodeCount)--; // Remove this node from the traversed nodes
                        currentNode = conflictingArtifactId; // Prepare to add the proper one
                        conflicts = false;
                        break;
                    }
                }

                if (!conflicts)
                {
                    continue; // Try again but with the now-current next artifact candidate
                }
            }

            // Try to rollback to first encounter of the conflicting dependency
            if (conflicts)
            {
                NodeIndex_t dependencyId = (*traversedNodes)[*traversedNodeCount - 2]; // Dependency for current artifact (traversedNodeCount must be at least 2 for a conflict to occur) (REMEMBER THAT -1 IS THE CURRENT -> -2 IS THE LAST!)
                
                bool circular=false; // Detect a circular dependency by checking if the conflicting artifact is on our current path
                for (size_t i=(*traversedNodeCount)-1; (*traversedNodes)[i] != root; i--)
                {
                    if ((*traversedNodes)[i] == conflictingArtifactId)
                    {
                        circular = true;
                        break;
                    }
                }

                if (!circular)
                {
                    // If the dependency isn't circular then we can just find a dependency that fits both branches and recompute from there
                    // There's a fairly high chance that this should work in such scenarios
                    for (size_t i=0; i < graph->nodes[dependencyId].connectedCount; i++)
                    {
                        for (size_t j=0; j < graph->nodes[conflictingDependencyId].connectedCount; j++)
                        {
                            if (graph->nodes[dependencyId].connected[i] == graph->nodes[conflictingDependencyId].connected[j])
                            {
                                // Find the point to rewind to
                                for (size_t k=0; k < *traversedNodeCount; k++)
                                {
                                    if ((*traversedNodes)[k] == conflictingDependencyId)
                                    {
                                        *traversedNodeCount = k+1; // Rewind to dependency node (remember the count is index+1)
                                        currentNode = graph->nodes[conflictingDependencyId].connected[j]; // Set new artifact as current node
                                        conflicts=false; // Run from this new state
                                        break;
                                    }
                                }
                                break;
                            }
                        }

                        if (!conflicts)
                        {
                            break;
                        }
                    }

                    if (!conflicts)
                    {
                        continue; // Re-compute from new point
                    }
                }


                // It's circular OR we couldn't find a trivial solution
                // This is essentially the nuclear option - we TRY to eliminate the conflicting dependency from this point

                (*traversedNodeCount)--; // We are no longer looking at artifacts! (No need to bounds check because a circular dependency cannot just happen like that)
                NodeIndex_t currentDependencyId = dependencyId;
                while (conflicts)
                {
                    // Does a parent artifact really exist?
                    if (*traversedNodeCount <= 2 || (*traversedNodes)[*traversedNodeCount - 2] == root) // We cannot rewind further
                    {
                        break;
                    }
                    
                    NodeIndex_t blacklistedDependency = currentDependencyId;
                    // Go back to the dependent dependency :p
                    (*traversedNodeCount) -= 2;
                    currentDependencyId = (*traversedNodes)[*traversedNodeCount - 1];

                    // Find a resolution to the dependency that doesn't directly require our now-blacklisted dependency
                    bool blacklistPassed=false;
                    for (size_t i=0; i < graph->nodes[currentDependencyId].connectedCount; i++)
                    {
                        bool artifactBlacklisted = false;
                        NodeIndex_t candidateArtifact = graph->nodes[currentDependencyId].connected[i];
                        for (size_t j=0; j < graph->nodes[candidateArtifact].connectedCount; j++)
                        {
                            if (graph->nodes[candidateArtifact].connected[j] == blacklistedDependency)
                            {
                                artifactBlacklisted = true;
                                blacklistPassed=true;
                                break;
                            }
                        }

                        // Get the first artifact that doesn't have our blacklisted dependency
                        // We ignore any before the blacklisted dep as we assume it has already been checked
                        if (blacklistPassed && !artifactBlacklisted)
                        {
                            currentNode = candidateArtifact;
                            conflicts=false;
                            break;
                        }
                    }
                }

                if (!conflicts)
                {
                    continue;
                }

                // Somehow that didn't work
                // Nuke it.
                // Just nuke the original dependency

                // Find the first time we saw this dependency
                // And rewind there - then blacklist it!
                for (size_t i=0; i < *traversedNodeCount; i++)
                {
                    if ((*traversedNodes)[i] == dependencyId)
                    {
                        currentDependencyId = (*traversedNodes)[i];
                        *traversedNodeCount = i+1;
                        break;
                    }
                }

                while (conflicts)
                {
                    // Does a parent artifact really exist?
                    if (*traversedNodeCount <= 2 || (*traversedNodes)[*traversedNodeCount - 2] == root) // We cannot rewind further
                    {
                        break;
                    }
                    
                    NodeIndex_t blacklistedDependency = currentDependencyId;
                    // Go back to the dependent dependency :p
                    (*traversedNodeCount) -= 2;
                    currentDependencyId = (*traversedNodes)[*traversedNodeCount - 1];

                    bool blacklistPassed=false;
                    for (size_t i=0; i < graph->nodes[currentDependencyId].connectedCount; i++)
                    {
                        bool artifactBlacklisted = false;
                        NodeIndex_t candidateArtifact = graph->nodes[currentDependencyId].connected[i];
                        for (size_t j=0; j < graph->nodes[candidateArtifact].connectedCount; j++)
                        {
                            if (graph->nodes[candidateArtifact].connected[j] == blacklistedDependency)
                            {
                                artifactBlacklisted = true;
                                blacklistPassed=true;
                                break;
                            }
                        }

                        // Get the first artifact that doesn't have our blacklisted dependency
                        // We ignore any before the blacklisted dep as we assume it has already been checked
                        if (blacklistPassed && !artifactBlacklisted)
                        {
                            currentNode = candidateArtifact;
                            conflicts=false;
                            break;
                        }
                    }
                }

                if (!conflicts)
                {
                    continue;
                }
            }

            if (conflicts)
            {
                return false;
            }

            // No traversal issue AND no conflict?
            // Move to the next item in this node
            if (graph->nodes[currentNode].connectedCount > 0)
            {
                // Go to first dependency
                currentNode = graph->nodes[currentNode].connected[0];
                Internal_ArrayAddNode(traversedNodeCount, traversedNodes, currentNode); // Add dependency to traversal list
                // Go to first artifact candidate
                currentNode = graph->nodes[currentNode].connected[0]; // It's guaranteed that a dependency will have viable candidates if it is in the graph
                continue;
            }
        }

        if (alreadyTraversed || graph->nodes[currentNode].connectedCount == 0) // Move towards the next dependency artifact
        {
            // Construct a new path for the next dependency
            NodeIndex_t rootIndex;
            for (rootIndex=(*traversedNodeCount)-1; (*traversedNodes)[rootIndex] != root; rootIndex--)
            {
                continue;
            }

            *traversedNodes = realloc(*traversedNodes, (*traversedNodeCount + (*traversedNodeCount - rootIndex)) * sizeof(size_t));
            for (size_t i=0; i < (*traversedNodeCount - rootIndex); i++)
            {
                (*traversedNodes)[*traversedNodeCount + i] = (*traversedNodes)[rootIndex + i];
            }
            *traversedNodeCount+=(*traversedNodeCount-rootIndex);

            // Move up-right until we find the next dependency to traverse
            bool foundNextDependency=false;
            while(!foundNextDependency)
            {
                // Does a parent artifact really exist?
                if ((*traversedNodes)[*traversedNodeCount - 1]  == root) // We are the top
                {
                    // We are done
                    return true;
                }

                // Nothing to do for this node, move up back to our dependent's artifact
                NodeIndex_t traversedDependency = (*traversedNodes)[*traversedNodeCount - 2]; // Dependency we just resolved
                (*traversedNodeCount) -= 2; // Move up
                currentNode = (*traversedNodes)[*traversedNodeCount - 1]; // Set current node to the dependent artifact

                // Find the next dependency
                for (size_t i=0; i < graph->nodes[currentNode].connectedCount; i++)
                {
                    if (graph->nodes[currentNode].connected[i] == traversedDependency)
                    {
                        if (i == graph->nodes[currentNode].connectedCount-1) // Nothing next!
                        {
                            break;
                        }

                        foundNextDependency = true;
                        currentNode = graph->nodes[currentNode].connected[i+1]; // Set currentNode to the next dependency
                        Internal_ArrayAddNode(traversedNodeCount, traversedNodes, currentNode); // Add dependency to traversal list
                        currentNode = graph->nodes[currentNode].connected[0]; // Set currentNode to our first artifact candidate in the dependency
                        break;
                    }
                }
            }
        }
    }

    return true;
}
