#include "dependencies.h"

#include "kpm/kpkg.h"
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal_utils.h"

Hashmap* Hashmap_Create(int size)
{
    assert((size & (size-1)) == 0); // Ensure power of 2
    Hashmap* h = malloc(sizeof(Hashmap));
    *h = (Hashmap) {
        .bucket_count = size,
        .buckets = malloc(size * sizeof(struct HashmapBucket))
    };
    memset(h->buckets, 0, sizeof(struct HashmapBucket) * size);
    return h;
}

int Hashmap_Hash(Hashmap* hashmap, char* key)
{
    int hash = 0;
    int len = strlen(key);
    for (int i=0; i < len; i++)
        hash += key[i]; // @TODO: More robust hashing algorithm lol
    return (hashmap->bucket_count - 1) & hash;
}

NodeIndex_t Hashmap_Get(Hashmap* hashmap, NodeIndex_t** nodes, char* key)
{
    *nodes = NULL;
    int hash = Hashmap_Hash(hashmap, key);
    
    for (
        struct HashmapBucket* bucket = hashmap->buckets + hash;
        bucket->key != NULL;
        bucket = bucket->pnext
    )
    {
        if (!strcmp(bucket->key, key))
        {
            *nodes = bucket->nodes;
            return bucket->node_count;
        }

        if (!bucket->pnext)
            break;
    }

    return NODE_NULL;
}

void Hashmap_Remove(Hashmap* hashmap, char* key)
{
    int hash = Hashmap_Hash(hashmap, key);

    for (
        struct HashmapBucket* bucket = hashmap->buckets + hash;
        bucket->key != NULL;
        bucket = bucket->pnext
    )
    {
        if (!strcmp(bucket->key, key))
        {
            free(bucket->key);
            free(bucket->nodes);
            bucket->key = NULL;
            bucket->nodes = NULL;
            if (bucket->pnext)
            {
                struct HashmapBucket* to_free = bucket->pnext;
                *bucket = *to_free;
                free(to_free);
            }
        }

        if (!bucket->pnext)
            break;
    }
}

void Hashmap_RemoveNode(Hashmap* hashmap, char* key, NodeIndex_t node)
{
    int hash = Hashmap_Hash(hashmap, key);

    for (
        struct HashmapBucket* bucket = hashmap->buckets + hash;
        bucket->key != NULL;
        bucket = bucket->pnext
    )
    {
        if (!strcmp(bucket->key, key))
        {
            for (int i=0; i < bucket->node_count-1; i++)
            {
                if (bucket->nodes[i] == node)
                {
                    memcpy(&bucket->nodes[i], &bucket->nodes[i+1], bucket->node_count - i - 1);
                    bucket->nodes = realloc(bucket->nodes, sizeof(NodeIndex_t) * --bucket->node_count);
                    return;
                }
            }
        }

        if (!bucket->pnext)
            break;
    }
}

void Hashmap_AddNode(Hashmap* hashmap, char* key, NodeIndex_t node)
{
    int hash = Hashmap_Hash(hashmap, key);
    struct HashmapBucket* bucket;
    for (
        bucket = hashmap->buckets + hash;
        bucket->key != NULL;
        bucket = bucket->pnext
    )
    {
        if (!strcmp(bucket->key, key))
            break;

        if (!bucket->key || !bucket->pnext)
        {
            if (bucket->key)
            {
                bucket->pnext = malloc(sizeof(struct HashmapBucket));
                bucket = bucket->pnext;
            }

            bucket->key = strdup(key);
            bucket->node_count = 0;
        }
    }

    bucket->nodes = realloc(bucket->nodes, sizeof(NodeIndex_t) * ++bucket->node_count);
    bucket->nodes[0] = node;
}

void KPM_FreeDependency(Dependency* dependency)
{
    free(dependency->id);
    dependency->id = NULL;
}

void KPM_FreeDependencyList(size_t dependencyCount, Dependency* dependencies)
{
    for (size_t i=0; i < dependencyCount; i++)
        KPM_FreeDependency(&dependencies[i]);
    
    free(dependencies);
}

/**
 * @brief Create a Dependency Graph object
 * 
 * @param graph Pointer to an empty graph struct to initialise
 * @param count Number of nodes to allocate in the graph object initially
 */
void CreateDependencyGraph(DependencyGraph* graph, int count)
{
    graph->nodeCount = 0;
    graph->nodes = NULL;
    graph->allocated = 0;

    if (count != 0)
    {
        graph->nodes = malloc(sizeof(GraphNode) * count);
        graph->allocated = count;
    }
}

/**
 * @brief Free a node object
 * 
 * @param node The node to free
 */
void FreeNode(GraphNode* node)
{
    if (node->gt.type == GT_DEPENDENCY)
    {
        free(node->gt.dependency.id);
        node->gt.dependency.id = NULL;
    }
    else
    {
        KPM_FreeInstallTarget(&node->gt.target);
    }
    node->connected = NULL;
    node->connectedCount = 0;
}

/**
 * @brief Free a dependency graph and all the nodes in it
 * 
 * @param graph The graph to free
 */
void FreeDependencyGraph(DependencyGraph* graph)
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
void ExtendDependencyGraph(DependencyGraph* graph, int allocate)
{
    graph->allocated += allocate;
    graph->nodes = realloc(graph->nodes, sizeof(GraphNode) * graph->allocated);
}

/**
 * @brief Add a node to the dependency graph
 * 
 * @param graph 
 * @param node 
 * @return NodeIndex_t 
 */
NodeIndex_t AddNode(DependencyGraph *graph, GraphNode node)
{
    if (graph->nodeCount == graph->allocated)
    {
        ExtendDependencyGraph(graph, graph->allocated/2 + 1); // ~1.5x growth factor
    }

    graph->nodes[graph->nodeCount++] = node;

    if (graph->nodes->gt.type == GT_DEPENDENCY)
        Hashmap_AddNode(graph->depmap, node.gt.dependency.id, graph->nodeCount-1);
    else if (graph->nodes->gt.type == GT_INSTALL_TARGET)
        Hashmap_AddNode(graph->depmap, KPM_GetInstallTargetId(node.gt.target), graph->nodeCount-1);
    else
        Hashmap_AddNode(graph->depmap, node.gt.installed.id, graph->nodeCount-1);

    return graph->nodeCount-1;
}

SemVer GetGraphTargetVersion(GraphTarget target)
{
    if (target.type == GT_DEPENDENCY)
        return target.dependency.min_version;
    if (target.type == GT_INSTALL_TARGET)
        return KPM_GetInstallTargetVersion(target.target);
    if (target.type == GT_INSTALLED)
        return target.installed.version;
}

char* GetGraphTargetId(GraphTarget target)
{
    if (target.type == GT_DEPENDENCY)
        return target.dependency.id;
    if (target.type == GT_INSTALL_TARGET)
        return KPM_GetInstallTargetId(target.target);
    if (target.type == GT_INSTALLED)
        return target.installed.id;
}

/**
 * @brief Add a dependency to an artifact
 * 
 * @param graph 
 * @param firstNodeIndex 
 * @param nextNodeIndex 
 */
void AddArtifactDep(DependencyGraph* graph, NodeIndex_t artNodeIdx, NodeIndex_t depNodeIdx)
{
    graph->nodes[artNodeIdx].connectedCount++;
    graph->nodes[artNodeIdx].connected = realloc(graph->nodes[artNodeIdx].connected, graph->nodes[artNodeIdx].connectedCount * sizeof(size_t));
    graph->nodes[artNodeIdx].connected[graph->nodes[artNodeIdx].connectedCount-1] = depNodeIdx;
}


void AddDepTarget(DependencyGraph* graph, NodeIndex_t depNodeIdx, NodeIndex_t target_node_idx)
{
    GraphNode target_node = graph->nodes[target_node_idx];
    assert(graph->nodes[depNodeIdx].gt.type == GT_DEPENDENCY);
    assert(target_node.gt.type == GT_INSTALL_TARGET || target_node.gt.type == GT_INSTALLED);

    graph->nodes[depNodeIdx].connectedCount++;
    graph->nodes[depNodeIdx].connected = realloc(graph->nodes[depNodeIdx].connected, graph->nodes[depNodeIdx].connectedCount * sizeof(size_t));
    
    NodeIndex_t insertionIndex = graph->nodes[depNodeIdx].connectedCount-1;
    while (insertionIndex > 0)
    {
        GraphNode prev_node = graph->nodes[graph->nodes[depNodeIdx].connected[insertionIndex-1]];
        if (prev_node.gt.priority > target_node.gt.priority)
            break; // Insert here

        if (SemVerCmp(
            GetGraphTargetVersion(prev_node.gt),
            GetGraphTargetVersion(target_node.gt)
        ) > 0)
            break; // Insert here

        graph->nodes[depNodeIdx].connected[insertionIndex] = graph->nodes[depNodeIdx].connected[insertionIndex-1];
        insertionIndex--;
    }
    
    graph->nodes[depNodeIdx].connected[insertionIndex] = target_node_idx;
}

/**
 * @brief Add an edge to the graph such that this is the first edge for the given firstNode
 * 
 * @param graph 
 * @param firstNodeIndex 
 * @param nextNodeIndex 
 */
void AddFirstEdge(DependencyGraph* graph, NodeIndex_t firstNodeIndex, NodeIndex_t nextNodeIndex)
{
    graph->nodes[firstNodeIndex].connectedCount++;
    graph->nodes[firstNodeIndex].connected = realloc(graph->nodes[firstNodeIndex].connected, graph->nodes[firstNodeIndex].connectedCount * sizeof(size_t));
    
    NodeIndex_t insertionIndex = graph->nodes[firstNodeIndex].connectedCount-1;
    while (insertionIndex > 0)
    {
        graph->nodes[firstNodeIndex].connected[insertionIndex] = graph->nodes[firstNodeIndex].connected[insertionIndex-1];
        insertionIndex--;
    }

    graph->nodes[firstNodeIndex].connected[insertionIndex] = nextNodeIndex;
}

/**
 * @brief Given a specified artifact, get the list of dependencies it needs
 * 
 * @param kpm 
 * @param target 
 * @param targetDependencyCount 
 * @param targetDependencies 
 * @return KPMResult 
 */
KPMResult Internal_GetGraphTargetDependencies(KPM* kpm, GraphTarget target, size_t* targetDependencyCount, Dependency** targetDependencies, KPMIO* kpm_io)
{
    if (target.type == GT_INSTALL_TARGET)
    {
        if (target.target.type == INSTALL_ARTIFACT)
            return KPM_ListArtifactDependencies(kpm, target.target.artifact.repository, target.target.artifact.id, target.target.artifact.url, targetDependencyCount, targetDependencies);

        *targetDependencyCount = KPKG_GetDependencies(target.target.file, (KPKG_DependencyEntry**) targetDependencies);
        return KPM_OK;
    }

    if (target.type == GT_INSTALLED)
        return KPM_ListInstalledPackageDependencies(kpm, target.installed.id, targetDependencyCount, targetDependencies);
}

void strcat_hd(char* dest, char* src, size_t* length, bool dry)
{
    *length += strlen(src);
    if (!dry)
        strcat(dest, src);
}

void node2str(DependencyGraph* graph, NodeIndex_t index, char** output, bool declaration)
{
    GraphNode node = graph->nodes[index];
    if (declaration)
    {
        if (node.gt.type == GT_INSTALL_TARGET)
        {
            if (node.gt.target.type == INSTALL_ARTIFACT)
            {
                *output = asprintf_hd(
                    "%i[\"%s\n%s\n%u.%u.%u\"]",
                    index,
                    node.gt.target.artifact.repository,
                    node.gt.target.artifact.id,
                    node.gt.target.artifact.version.major,
                    node.gt.target.artifact.version.minor,
                    node.gt.target.artifact.version.patch
                );
            }
            else
            {
                *output = asprintf_hd(
                    "%i[\"%s\n%s\n%u.%u.%u\"]",
                    index,
                    NULL,
                    KPKG_GetId(node.gt.target.file),
                    KPKG_GetVersion(node.gt.target.file).major,
                    KPKG_GetVersion(node.gt.target.file).minor,
                    KPKG_GetVersion(node.gt.target.file).patch
                );
            }
            return;
        }

        if (node.gt.type == GT_INSTALLED)
        {
            if (node.gt.target.type == INSTALL_ARTIFACT)
            {
                *output = asprintf_hd(
                    "%i[\"%s\n%s\n%u.%u.%u\"]",
                    index,
                    node.gt.installed.repository,
                    node.gt.installed.id,
                    node.gt.installed.version.major,
                    node.gt.installed.version.minor,
                    node.gt.installed.version.patch
                );
            }
            return;
        }

        if (node.gt.type == GT_DEPENDENCY)
        {
            *output = asprintf_hd(
                "%i([\"%s\n%u.%u.%u to %u.%u.%u\"])",
                index,
                node.gt.dependency.id,
                node.gt.dependency.min_version.major,
                node.gt.dependency.min_version.minor,
                node.gt.dependency.min_version.patch,
                node.gt.dependency.max_version.major,
                node.gt.dependency.max_version.minor,
                node.gt.dependency.max_version.patch
            );
            return;
        }
    }
    else 
    {
        *output = asprintf_hd("%i", index);
        return;
    }
}

int Internal_RenderGraph(DependencyGraph* graph, char* output, bool dry)
{
    size_t outputLength=0;
    strcat_hd(output, "flowchart TD\n", &outputLength, dry);

    for (NodeIndex_t i=0; i < graph->nodeCount; i++)
    {
        char* nodeName = NULL;
        node2str(graph, i, &nodeName, true);
        strcat_hd(output, "    ", &outputLength, dry);
        strcat_hd(output, nodeName, &outputLength, dry);
        strcat_hd(output, "\n", &outputLength, dry);
        free(nodeName);

        for (size_t j=0; j < graph->nodes[i].connectedCount; j++)
        {
            char* connectedName = NULL;
            node2str(graph, i, &nodeName, false);
            node2str(graph, graph->nodes[i].connected[j], &connectedName, false);
            strcat_hd(output, "    ", &outputLength, dry);
            strcat_hd(output, nodeName, &outputLength, dry);
            strcat_hd(output, " --> ", &outputLength, dry);
            strcat_hd(output, connectedName, &outputLength, dry);
            strcat_hd(output, "\n", &outputLength, dry);
            free(connectedName);
            free(nodeName);
        }
    }

    return outputLength;
}

void RenderGraph(DependencyGraph* graph, char** output)
{
    size_t targetSize = 1 + Internal_RenderGraph(graph, NULL, true);
    *output = malloc(targetSize);
    //memset(*output, 0, targetSize);
    *output[0] = '\0';
    Internal_RenderGraph(graph, *output, false);
}

NodeIndex_t CreateTargetNode(DependencyGraph* graph, InstallTarget target, GraphTargetPriority priority)
{
    return AddNode(graph, (GraphNode) {
        .connectedCount = 0,
        .connected = NULL,
        .gt = {
            .priority = priority,
            .type = GT_INSTALL_TARGET,
            .target = target
        }
    });
}

NodeIndex_t CreateInstalledNode(DependencyGraph* graph, InstalledPackage installed)
{
    return AddNode(graph, (GraphNode) {
        .connectedCount = 0,
        .connected = NULL,
        .gt = {
            .priority = GT_PRIORITY_INSTALLED,
            .type = GT_INSTALLED,
            .installed = installed
        }
    });
}

NodeIndex_t CreateDependencyNode(DependencyGraph* graph, Dependency dependency)
{
    return AddNode(graph, (GraphNode) {
        .connectedCount = 0,
        .connected = NULL,
        .gt = {
            .priority = GT_PRIORITY_NONE,
            .type = GT_DEPENDENCY,
            .dependency = dependency
        }
    });
}

NodeIndex_t Internal_FindDependencyInGraph(DependencyGraph* graph, Dependency dep)
{
    NodeIndex_t* deps_found;
    int dep_count = Hashmap_Get(graph->depmap, &deps_found, dep.id);
    for (int i=0; i < dep_count; i++)
    {
        NodeIndex_t idx = deps_found[i];
        GraphNode node = graph->nodes[idx];
        assert(node.gt.type == GT_DEPENDENCY);

        if (SemVerCmp(node.gt.dependency.min_version, dep.min_version))
            continue;

        if (SemVerCmp(node.gt.dependency.max_version, dep.max_version))
            continue;

        free(deps_found);
        return idx;
    }
    return NODE_NULL;
}

NodeIndex_t Internal_FindTargetInGraph(DependencyGraph* graph, GraphTarget gt)
{
    NodeIndex_t* targets_found;
    int target_count = Hashmap_Get(graph->targetmap, &targets_found, GetGraphTargetId(gt));
    for (int i=0; i < target_count; i++)
    {
        NodeIndex_t idx = targets_found[i];
        GraphNode node = graph->nodes[idx];
        assert(node.gt.type != GT_DEPENDENCY);

        if (SemVerCmp(GetGraphTargetVersion(gt), GetGraphTargetVersion(node.gt)))
            continue;

        free(targets_found);
        return idx;
    }

    return NODE_NULL;
}

NodeIndex_t Internal_FindArtifactInGraph(DependencyGraph* graph, Artifact artifact)
{
    NodeIndex_t* targets_found;
    int target_count = Hashmap_Get(graph->targetmap, &targets_found, artifact.id);
    for (int i=0; i < target_count; i++)
    {
        NodeIndex_t idx = targets_found[i];
        GraphNode node = graph->nodes[idx];
        assert(node.gt.type != GT_DEPENDENCY);

        if (SemVerCmp(artifact.version, GetGraphTargetVersion(node.gt)))
            continue;

        free(targets_found);
        return idx;
    }

    return NODE_NULL;
}

void Internal_AddExistingTargetsToDependency(DependencyGraph* graph, NodeIndex_t dep_idx)
{
    assert(graph->nodes[dep_idx].gt.type == GT_DEPENDENCY);
    Dependency dep = graph->nodes[dep_idx].gt.dependency;

    NodeIndex_t* target_node_idS;
    int node_count = Hashmap_Get(graph->targetmap, &target_node_idS, dep.id);
    for (int i=0; i < node_count; i++)
    {
        NodeIndex_t target_idx = target_node_idS[i];
        GraphNode node = graph->nodes[target_idx];
        assert(node.gt.type != GT_DEPENDENCY);

        if (
            SemVerCmp(
                GetGraphTargetVersion(node.gt),
                dep.min_version
            ) >= 0 &&
            SemVerCmp(
                GetGraphTargetVersion(node.gt),
                dep.max_version
            ) < 0
        )
        {
            AddDepTarget(graph, dep_idx, target_idx);
        }
    }
}

void Internal_AddTargetToExistingDependencies(DependencyGraph* graph, NodeIndex_t target_idx)
{
    GraphNode target_node = graph->nodes[target_idx];
    assert(target_node.gt.type != GT_DEPENDENCY);

    NodeIndex_t* dep_node_ids;
    int node_count = Hashmap_Get(graph->depmap, &dep_node_ids, GetGraphTargetId(target_node.gt));
    for (int i=0; i < node_count; i++)
    {
        NodeIndex_t dep_idx = dep_node_ids[i];
        GraphNode dep_node = graph->nodes[dep_idx];

        if (
            SemVerCmp(
                GetGraphTargetVersion(target_node.gt),
                dep_node.gt.dependency.min_version
            ) >= 0 &&
            SemVerCmp(
                GetGraphTargetVersion(target_node.gt),
                dep_node.gt.dependency.max_version
            ) < 0
        )
        {
            AddDepTarget(graph, dep_idx, target_idx);
        }
    }
}

bool Internal_FillGraphFromTarget(KPM* kpm, DependencyGraph* graph, NodeIndex_t target_node_id, KPMIO* kpm_io)
{
    GraphTarget graph_target = graph->nodes[target_node_id].gt;
    assert(graph_target.type == GT_INSTALL_TARGET);
    assert(graph_target.type == GT_INSTALLED);

    Internal_AddTargetToExistingDependencies(graph, target_node_id);

    // Go through this target's own dependencies
    size_t dep_count;
    Dependency* dependencies;
    if (Internal_GetGraphTargetDependencies(kpm, graph_target, &dep_count, &dependencies, kpm_io) != KPM_OK)
        return false;

    for (int i=0; i < dep_count; i++)
    {
        Dependency dep = dependencies[i];

        // Check if this dependency already exists in the graph
        NodeIndex_t dep_found = Internal_FindDependencyInGraph(graph, dep);
        if (dep_found != NODE_NULL)
        {
            AddArtifactDep(graph, target_node_id, dep_found);
            continue;
        }

        // Gotta make AND POPULATE the dep node ourselves!
        NodeIndex_t dep_idx = CreateDependencyNode(graph, dep);

        // For each target in the graph that fits, add it to our dependency
        Internal_AddExistingTargetsToDependency(graph, dep_idx);

        // Find all artifacts NOT in the graph that satisfy this dependency and recursively add them
        size_t artifact_count;
        Artifact* artifacts;
        if (!KPM_ListPackageArtifacts(kpm, NULL, dep.id,  &artifact_count, &artifacts))
            return false;

        for (int j = 0; j < artifact_count; j++)
        {            
            NodeIndex_t existing_artifact = Internal_FindArtifactInGraph(graph, artifacts[j]);
            if (existing_artifact != NODE_NULL)
                continue;

            // This artifact is new, we should add it to the graph...
            NodeIndex_t idx = CreateTargetNode(
                graph,
                (InstallTarget) {
                    .type = INSTALL_ARTIFACT,
                    .artifact = artifacts[j]
                },
                GT_PRIORITY_NONE
            );

            if (idx == NODE_NULL)
                return false; // @TODO: Cleanup

            AddDepTarget(graph, dep_idx, idx);

            Internal_FillGraphFromTarget(
                kpm,
                graph,
                idx,
                kpm_io
            );
        }
    }

    KPM_FreeDependencyList(dep_count, dependencies);
    return true;
}

void Internal_ArrayAddNode(size_t* traversedNodeCount, NodeIndex_t** traversedNodes, NodeIndex_t node)
{
    (*traversedNodeCount)++;
    *traversedNodes = realloc(*traversedNodes, (*traversedNodeCount) * sizeof(NodeIndex_t));
    (*traversedNodes)[*traversedNodeCount - 1] = node;
}

bool Internal_ResolveDependencyGraph(DependencyGraph* graph, NodeIndex_t root, NodeIndex_t currentNode, size_t* traversedNodeCount, NodeIndex_t** traversedNodes, KPMIO* kpmIO)
{

}
