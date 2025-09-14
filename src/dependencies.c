#include "dependencies.h"

#include "cjson/cJSON.h"
#include "install.h"
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <limits.h>
#include <stddef.h>
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
    
    // Insert it into the list such that the connected node list is ordered newest to oldest
    size_t insertionIndex = graph->nodes[firstNodeIndex].connectedCount-1;
    while (insertionIndex > 0)
    {
        if (SemVerCmp(graph->nodes[graph->nodes[firstNodeIndex].connected[insertionIndex-1]].min_version, graph->nodes[nextNodeIndex].min_version) > 0)
        {
            break;
        }

        graph->nodes[firstNodeIndex].connected[insertionIndex] = graph->nodes[firstNodeIndex].connected[insertionIndex-1];
        insertionIndex--;
    }
    graph->nodes[firstNodeIndex].connected[insertionIndex] = nextNodeIndex;
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

            (*targetDependencies)[i].artifact_repository = strdup(target->id);
            (*targetDependencies)[i].artifact_id = strdup(target->id);
            (*targetDependencies)[i].artifact_url = strdup(target->id);
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
        return KPM_ListArtifactDependencies(kpm, target->repository, target->id, target->url, targetDependencyCount, targetDependencies);
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
            *output = malloc(1 + snprintf(NULL, 0, "%zu[\"%s\n%s\n%u.%u.%u\"]", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch));
            sprintf(*output, "%zu[\"%s\n%s\n%u.%u.%u\"]", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch);
            return;
        }

        if (graph->nodes[index].type == NODE_DEPENDENCY)
        {
            *output = malloc(1 + snprintf(NULL, 0, "%zu([\"%s\n%s\n%u.%u.%u to %u.%u.%u\"])", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch, graph->nodes[index].max_version.major, graph->nodes[index].max_version.minor, graph->nodes[index].max_version.patch));
            sprintf(*output, "%zu([\"%s\n%s\n%u.%u.%u to %zu.%zu.%zu\"])", index, graph->nodes[index].repository, graph->nodes[index].id, graph->nodes[index].min_version.major, graph->nodes[index].min_version.minor, graph->nodes[index].min_version.patch, graph->nodes[index].max_version.major, graph->nodes[index].max_version.minor, graph->nodes[index].max_version.patch);
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
    if (Internal_GetArtifactDependencies(kpm, artifact, &dependencyCount, &dependencies) != KPM_OK)
    {
        return -1;
    }

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

bool CheckIdInDependencyList(char* id, size_t* needleIndex, size_t haystackSize, struct DependencyNode* haystack)
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

void Internal_ArrayAddNode(size_t* traversedNodeCount, size_t** traversedNodes, size_t node)
{
    (*traversedNodeCount)++;
    *traversedNodes = realloc(*traversedNodes, *traversedNodeCount * sizeof(size_t));
    (*traversedNodes)[*traversedNodeCount - 1] = node;
}

bool Internal_ResolveDependencyGraph(struct DependencyGraph* graph, size_t root, size_t* traversedNodeCount, size_t** traversedNodes)
{
    *traversedNodes = NULL;
    *traversedNodeCount = 0;

    size_t currentNode = root;
    for (;;)
    {
        // Check if this artifact is already traversed
        bool alreadyTraversed=false;
        for (size_t i=0; i < *traversedNodeCount; i++)
        {
            if (*traversedNodes[i] == currentNode)
            {
                alreadyTraversed=true;
                break;
            }
        }

        Internal_ArrayAddNode(traversedNodeCount, traversedNodes, currentNode);

        if (!alreadyTraversed)
        {
            // Ensure this NEW node doesn't conflict with any previously traversed artifacts
            bool conflicts = false;
            size_t conflictingArtifactId;
            size_t conflictingDependencyId;
            for (size_t i=1; i < *traversedNodeCount; i+=2)
            {
                conflictingDependencyId = (*traversedNodes)[i];
                conflictingArtifactId = (*traversedNodes)[i+1];
                if (currentNode != conflictingArtifactId && strcmp(graph->nodes[currentNode].id, graph->nodes[conflictingArtifactId].id) == 0)
                {
                    // Don't need to check if version conflicts because if the node wasn't already traversed
                    // It must be conflicting since it wasn't merged
                    conflicts=true;
                    break;
                }
            }

            // Find an alternative artifact that matches the conflicting artifact
            size_t dependencyId = (*traversedNodes)[*traversedNodeCount - 2]; // Dependency for current artifact
            if (conflicts)
            {
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
                                    if (*(traversedNodes)[k] == conflictingArtifactId)
                                    {
                                        *traversedNodeCount = k+1; // Rewind to dependency node
                                        currentNode = graph->nodes[conflictingArtifactId].connected[j]; // Set new artifact as current node
                                        conflicts=false; // Run from there
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

                traversedNodeCount--; // We are no longer looking at artifacts!
                size_t currentDependencyId = dependencyId;
                while (conflicts)
                {
                    // Does a parent artifact really exist?
                    if ((*traversedNodes)[*traversedNodeCount - 2]  == root) // We cannot rewind further
                    {
                        break;
                    }
                    
                    size_t blacklistedDependency = currentDependencyId;
                    // Go back to the dependent artifact
                    traversedNodeCount -= 2;
                    currentDependencyId = (*traversedNodes)[*traversedNodeCount - 1];

                    for (size_t i=0; i < graph->nodes[currentDependencyId].connectedCount; i++)
                    {
                        bool blacklistPassed=false;
                        size_t candidateArtifact = graph->nodes[currentDependencyId].connected[i];
                        for (size_t j=0; j < graph->nodes[candidateArtifact].connectedCount; j++)
                        {
                            if (graph->nodes[candidateArtifact].connected[j] == blacklistedDependency)
                            {
                                blacklistPassed=true;
                                continue;
                            }

                            // Get the first artifact that doesn't have our blacklisted dependency
                            // We ignore any before the blacklisted dep as we assume it has already been checked
                            if (blacklistPassed)
                            {
                                currentNode = candidateArtifact;
                                conflicts=false;
                                break;
                            }
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
                    if ((*traversedNodes)[*traversedNodeCount - 2]  == root) // We cannot rewind further
                    {
                        break;
                    }
                    
                    size_t blacklistedDependency = currentDependencyId;
                    // Go back to the dependent artifact
                    traversedNodeCount -= 2;
                    currentDependencyId = (*traversedNodes)[*traversedNodeCount - 1];

                    for (size_t i=0; i < graph->nodes[currentDependencyId].connectedCount; i++)
                    {
                        bool blacklistPassed=false;
                        size_t candidateArtifact = graph->nodes[currentDependencyId].connected[i];
                        for (size_t j=0; j < graph->nodes[candidateArtifact].connectedCount; j++)
                        {
                            if (graph->nodes[candidateArtifact].connected[j] == blacklistedDependency)
                            {
                                blacklistPassed=true;
                                continue;
                            }

                            // Get the first artifact that doesn't have our blacklisted dependency
                            // We ignore any before the blacklisted dep as we assume it has already been checked
                            if (blacklistPassed)
                            {
                                currentNode = candidateArtifact;
                                conflicts=false;
                                break;
                            }
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
                currentNode = graph->nodes[currentNode].connected[0];
                continue;
            }
        }

        if (alreadyTraversed || graph->nodes[currentNode].connectedCount == 0) // Move towards the next dependency artifact
        {
            // Construct a new path for the next dependency
            size_t rootIndex;
            for (rootIndex=(*traversedNodeCount)-1; (*traversedNodes)[rootIndex] != root; rootIndex--)
            {
                continue;
            }

            *traversedNodes = realloc(*traversedNodes, *traversedNodeCount + (*traversedNodeCount - rootIndex));
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
                size_t traversedDependency = (*traversedNodes)[*traversedNodeCount - 2]; // Dependency we just resolved
                *traversedNodeCount -= 2;
                currentNode = (*traversedNodes)[*traversedNodeCount - 1]; // Set current node to the dependent artifact

                // Find the next dependency
                for (size_t i=0; i < graph->nodes[currentNode].connectedCount; i++)
                {
                    if (graph->nodes[currentNode].connected[i] == traversedDependency)
                    {
                        if (i == graph->nodes[currentNode].connectedCount-1)
                        {
                            break;
                        }

                        foundNextDependency = true;
                        currentNode = graph->nodes[currentNode].connected[i+1]; // Set currentNode to the dependency
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
