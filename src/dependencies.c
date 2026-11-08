#include "dependencies.h"

#include "kpm/kpkg.h"
#include "kpm/kpm.h"
#include "kpm/semver.h"
#include <__stddef_unreachable.h>
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "internal_utils.h"

NodeIdxArray* NodeIdxArray_Create()
{
    NodeIdxArray* array = malloc(sizeof(NodeIdxArray));
    array->count = 0;
    array->items = NULL;
    return array;
}

void NodeIdxArray_Free(NodeIdxArray* array)
{
    free(array->items);
    array->items = NULL;
    array->count = 0;
}

void NodeIdxArray_Add(NodeIdxArray* array, NodeIdx_t node_idx)
{
    array->items = realloc(array->items, ++array->count);
    array->items[array->count-1] = node_idx;
}

size_t NodeIdxArray_FindFirst(NodeIdxArray* array, NodeIdx_t item)
{
    NodeIdxArray_ForEachIndex(array, i, current)
        if (current == item)
            return i;
    
    return -1;
}

void NodeIdxArray_Insert(NodeIdxArray* array, size_t index, NodeIdx_t node_idx)
{
    array->items = realloc(array->items, ++array->count);
    memcpy(array->items + index + 1, array->items + index, array->count - index);
}

int NodeIdxArray_Size(NodeIdxArray* array)
{
    return array->count;
}

NodeIdx_t NodeIdxArray_Get(NodeIdxArray* array, size_t index)
{
    if (index > array->count)
        return NODE_NULL;
    
    return array->items[index];
}

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

bool Hashmap_Get(Hashmap* hashmap, char* key, NodeIdxArray** array)
{
    *array = NULL;
    int hash = Hashmap_Hash(hashmap, key);
    
    for (
        struct HashmapBucket* bucket = hashmap->buckets + hash;
        bucket->key != NULL;
        bucket = bucket->pnext
    )
    {
        if (!strcmp(bucket->key, key))
        {
            if (array != NULL)
                *array = bucket->payload;
            return true;
        }

        if (!bucket->pnext)
            return false;
    }

    return false;
}

void Hashmap_AddNode(Hashmap* hashmap, char* key, NodeIdx_t idx)
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

        if (!bucket->pnext)
        {
            bucket->pnext = malloc(sizeof(struct HashmapBucket));
            bucket = bucket->pnext;
            bucket->key = strdup(key);
            bucket->payload = NodeIdxArray_Create();
            break;
        }
    }

    NodeIdxArray_Add(bucket->payload, idx);
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
            bucket->key = NULL;
            NodeIdxArray_Free(bucket->payload);
            if (bucket->pnext)
            {
                struct HashmapBucket* to_free = bucket->pnext;
                *bucket = *to_free;
                free(to_free);
            }
        }

        if (!bucket->pnext)
            return;
    }
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
    NodeIdxArray_Free(node->connected);
}

/**
 * @brief Free a dependency graph and all the nodes in it
 * 
 * @param graph The graph to free
 */
void FreeDependencyGraph(DependencyGraph* graph)
{
    for (NodeIdx_t i=0; i < graph->nodeCount; i++)
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
NodeIdx_t AddNode(DependencyGraph *graph, GraphNode node)
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
    switch (target.type)
    {
    case GT_DEPENDENCY:
        return target.dependency.min_version;
    case GT_INSTALL_TARGET:
        return KPM_GetInstallTargetVersion(target.target);
    case  GT_INSTALLED:
        return target.installed.version;
    };
}

char* GetGraphTargetId(GraphTarget target)
{
    switch (target.type)
    {
    case GT_DEPENDENCY:
        return target.dependency.id;
    case GT_INSTALL_TARGET:
        return KPM_GetInstallTargetId(target.target);
    case  GT_INSTALLED:
        return target.installed.id;
    };
}

/**
 * @brief Add a dependency to an artifact
 * 
 * @param graph 
 * @param firstNodeIndex 
 * @param nextNodeIndex 
 */
void AddArtifactDep(DependencyGraph* graph, NodeIdx_t artNodeIdx, NodeIdx_t depNodeIdx)
{
    NodeIdxArray_Add(graph->nodes[artNodeIdx].connected, depNodeIdx);
}


void AddDepTarget(DependencyGraph* graph, NodeIdx_t depNodeIdx, NodeIdx_t target_node_idx)
{
    GraphNode target_node = graph->nodes[target_node_idx];
    assert(graph->nodes[depNodeIdx].gt.type == GT_DEPENDENCY);
    assert(target_node.gt.type == GT_INSTALL_TARGET || target_node.gt.type == GT_INSTALLED);

    graph->nodes[depNodeIdx].connected->count++;
    graph->nodes[depNodeIdx].connected->items = realloc(graph->nodes[depNodeIdx].connected, graph->nodes[depNodeIdx].connected->count * sizeof(size_t));
    
    NodeIdx_t insertionIndex = graph->nodes[depNodeIdx].connected->count-1;
    while (insertionIndex > 0)
    {
        GraphNode prev_node = graph->nodes[graph->nodes[depNodeIdx].connected->items[insertionIndex-1]];
        if (prev_node.gt.priority > target_node.gt.priority)
            break; // Insert here

        if (SemVerCmp(
            GetGraphTargetVersion(prev_node.gt),
            GetGraphTargetVersion(target_node.gt)
        ) > 0)
            break; // Insert here
    }
    
    NodeIdxArray_Insert(graph->nodes[depNodeIdx].connected, 0, target_node_idx);
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
    switch (target.type)
    {
    case GT_DEPENDENCY:
        unreachable(); // UB
    case GT_INSTALL_TARGET:
        if (target.target.type == INSTALL_ARTIFACT)
            return KPM_ListArtifactDependencies(kpm, target.target.artifact.repository, target.target.artifact.id, target.target.artifact.url, targetDependencyCount, targetDependencies);

        *targetDependencyCount = KPKG_GetDependencies(target.target.file, (KPKG_DependencyEntry**) targetDependencies);
        return KPM_OK;
    case GT_INSTALLED:
        return KPM_ListInstalledPackageDependencies(kpm, target.installed.id, targetDependencyCount, targetDependencies);
    };
}

void strcat_hd(char* dest, char* src, size_t* length, bool dry)
{
    *length += strlen(src);
    if (!dry)
        strcat(dest, src);
}

void node2str(DependencyGraph* graph, NodeIdx_t index, char** output, bool declaration)
{
    GraphNode node = graph->nodes[index];
    if (declaration)
    {
        if (node.gt.type == GT_INSTALL_TARGET)
        {
            if (node.gt.target.type == INSTALL_ARTIFACT)
            {
                *output = asprintf_hd(
                    "%i[\"ART\n%s\n%s\n%u.%u.%u\n(P:%i)\"]",
                    index,
                    node.gt.target.artifact.repository,
                    node.gt.target.artifact.id,
                    node.gt.target.artifact.version.major,
                    node.gt.target.artifact.version.minor,
                    node.gt.target.artifact.version.patch,
                    node.gt.priority
                );
            }
            else
            {
                *output = asprintf_hd(
                    "%i[\"FILE\n(NO REPO)\n%s\n%u.%u.%u\n(P:%i)\"]",
                    index,
                    KPKG_GetId(node.gt.target.file),
                    KPKG_GetVersion(node.gt.target.file).major,
                    KPKG_GetVersion(node.gt.target.file).minor,
                    KPKG_GetVersion(node.gt.target.file).patch,
                    node.gt.priority
                );
            }
            return;
        }

        if (node.gt.type == GT_INSTALLED)
        {
            if (node.gt.target.type == INSTALL_ARTIFACT)
            {
                *output = asprintf_hd(
                    "%i[\"INST\n%s\n%s\n%u.%u.%u\n(P:%i)\"]",
                    index,
                    node.gt.installed.repository,
                    node.gt.installed.id,
                    node.gt.installed.version.major,
                    node.gt.installed.version.minor,
                    node.gt.installed.version.patch,
                    node.gt.priority
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

    for (NodeIdx_t i=0; i < graph->nodeCount; i++)
    {
        char* nodeName = NULL;
        node2str(graph, i, &nodeName, true);
        strcat_hd(output, "    ", &outputLength, dry);
        strcat_hd(output, nodeName, &outputLength, dry);
        strcat_hd(output, "\n", &outputLength, dry);
        free(nodeName);

        NodeIdxArray_ForEach(graph->nodes[i].connected, connected_idx)
        {
            char* connectedName = NULL;
            node2str(graph, i, &nodeName, false);
            node2str(graph, connected_idx, &connectedName, false);
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

NodeIdx_t CreateTargetNode(DependencyGraph* graph, InstallTarget target, GraphTargetPriority priority)
{
    return AddNode(graph, (GraphNode) {
        .connected = NodeIdxArray_Create(),
        .gt = {
            .priority = priority,
            .type = GT_INSTALL_TARGET,
            .target = target
        }
    });
}

NodeIdx_t CreateInstalledNode(DependencyGraph* graph, InstalledPackage installed)
{
    return AddNode(graph, (GraphNode) {
        .connected = NodeIdxArray_Create(),
        .gt = {
            .priority = GT_PRIORITY_INSTALLED,
            .type = GT_INSTALLED,
            .installed = installed
        }
    });
}

NodeIdx_t CreateDependencyNode(DependencyGraph* graph, Dependency dependency)
{
    return AddNode(graph, (GraphNode) {
        .connected = NodeIdxArray_Create(),
        .gt = {
            .priority = GT_PRIORITY_NONE,
            .type = GT_DEPENDENCY,
            .dependency = dependency
        }
    });
}

NodeIdx_t Internal_FindDependencyInGraph(DependencyGraph* graph, Dependency dep)
{
    NodeIdxArray* deps_found_array;
    Hashmap_Get(graph->depmap, dep.id, &deps_found_array);
    NodeIdxArray_ForEach(deps_found_array, idx)
    {
        GraphNode node = graph->nodes[idx];
        assert(node.gt.type == GT_DEPENDENCY);

        if (SemVerCmp(node.gt.dependency.min_version, dep.min_version))
            continue;

        if (SemVerCmp(node.gt.dependency.max_version, dep.max_version))
            continue;

        return idx;
    }
    return NODE_NULL;
}

NodeIdx_t Internal_FindTargetInGraph(DependencyGraph* graph, GraphTarget gt)
{
    NodeIdxArray* targets_found_array;
    Hashmap_Get(graph->targetmap, GetGraphTargetId(gt), &targets_found_array);
    NodeIdxArray_ForEach(targets_found_array, idx)
    {
        GraphNode node = graph->nodes[idx];
        assert(node.gt.type != GT_DEPENDENCY);

        if (SemVerCmp(GetGraphTargetVersion(gt), GetGraphTargetVersion(node.gt)))
            continue;

        return idx;
    }

    return NODE_NULL;
}

NodeIdx_t Internal_FindArtifactInGraph(DependencyGraph* graph, Artifact artifact)
{
    NodeIdxArray* targets_found_array;
    Hashmap_Get(graph->targetmap, artifact.id, &targets_found_array);
    NodeIdxArray_ForEach(targets_found_array, idx)
    {
        GraphNode node = graph->nodes[idx];
        assert(node.gt.type != GT_DEPENDENCY);

        if (SemVerCmp(artifact.version, GetGraphTargetVersion(node.gt)))
            continue;

        return idx;
    }

    return NODE_NULL;
}

void Internal_AddExistingTargetsToDependency(DependencyGraph* graph, NodeIdx_t dep_idx)
{
    assert(graph->nodes[dep_idx].gt.type == GT_DEPENDENCY);
    Dependency dep = graph->nodes[dep_idx].gt.dependency;

    NodeIdxArray* targets_found_array;
    Hashmap_Get(graph->targetmap, dep.id, &targets_found_array);
    NodeIdxArray_ForEach(targets_found_array, target_idx)
    {
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

void Internal_AddTargetToExistingDependencies(DependencyGraph* graph, NodeIdx_t target_idx)
{
    GraphNode target_node = graph->nodes[target_idx];
    assert(target_node.gt.type != GT_DEPENDENCY);

    NodeIdxArray* dep_node_ids;
    Hashmap_Get(graph->depmap, GetGraphTargetId(target_node.gt), &dep_node_ids);
    NodeIdxArray_ForEach(dep_node_ids, dep_idx)
    {
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

bool Internal_FillGraphFromTarget(KPM* kpm, DependencyGraph* graph, NodeIdx_t target_node_id, KPMIO* kpm_io)
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
        NodeIdx_t dep_found = Internal_FindDependencyInGraph(graph, dep);
        if (dep_found != NODE_NULL)
        {
            AddArtifactDep(graph, target_node_id, dep_found);
            continue;
        }

        // Gotta make AND POPULATE the dep node ourselves!
        NodeIdx_t dep_idx = CreateDependencyNode(graph, dep);

        // For each target in the graph that fits, add it to our dependency
        Internal_AddExistingTargetsToDependency(graph, dep_idx);

        // Find all artifacts NOT in the graph that satisfy this dependency and recursively add them
        size_t artifact_count;
        Artifact* artifacts;
        if (!KPM_ListPackageArtifacts(kpm, NULL, dep.id,  &artifact_count, &artifacts))
            return false;

        for (int j = 0; j < artifact_count; j++)
        {            
            NodeIdx_t existing_artifact = Internal_FindArtifactInGraph(graph, artifacts[j]);
            if (existing_artifact != NODE_NULL)
                continue;

            // This artifact is new, we should add it to the graph...
            NodeIdx_t idx = CreateTargetNode(
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

TraversedNodeArray* TraversedNodeArray_Create()
{
    TraversedNodeArray* array = malloc(sizeof(TraversedNodeArray));
    *array = (TraversedNodeArray) {
        .count = 0,
        .items = NULL
    };
    return array;
}

void TraversedNodeArray_Free(TraversedNodeArray* array)
{
    free(array->items);
    array->count = 0;
}

void TraversedNodeArray_Truncate(TraversedNodeArray* array, size_t size)
{
    array->items = realloc(array->items, size);
    array->count = size;
}

void TraversedNodeArray_Add(TraversedNodeArray* array, struct TraversedNode item)
{
    TraversedNodeArray_Truncate(array, ++array->count);
    array->items[array->count - 1] = item;
}

int TraversedNodeArray_FindFirstByNodeIdx(TraversedNodeArray* array, size_t node_idx)
{
    TraversedNodeArray_ForEachIndex(array, i, current)
        if (current.node_idx == node_idx)
            return i;
    
    return -1;
}

int TraversedNodeArray_Size(TraversedNodeArray* array)
{
    return array->count;
}

const struct TraversedNode TraversedNodeArray_Get(TraversedNodeArray* array, size_t idx)
{
    return array->items[idx];
}

bool Resolver_Next(struct ResolverState* state)
{
    size_t current_traversed_node_idx = TraversedNodeArray_Size(state->traversed_nodes) - 1;
    
    struct TraversedNode current_traversed_node;
    while ((current_traversed_node = TraversedNodeArray_Get(state->traversed_nodes, current_traversed_node_idx)).traversed_parent_idx != -1)
    {
        GraphNode current_node = state->graph->nodes[current_traversed_node.node_idx];
        if (current_traversed_node.connected_idx == NodeIdxArray_Size(current_node.connected) - 1)
        {
            current_traversed_node_idx = current_traversed_node.traversed_parent_idx;
            continue;
        }

        // Add node
        TraversedNodeArray_Add(state->traversed_nodes, (struct TraversedNode) {
            .connected_idx = current_traversed_node.connected_idx + 1,
            .node_idx = NodeIdxArray_Get(current_node.connected, current_traversed_node.connected_idx + 1),
            .traversed_parent_idx = current_traversed_node_idx
        });
        return true;
    }

    return false;
}

/**
 * @brief Backtrack the dependency graph, targeting a specific dependency_idx that conflicted
 * 
 * @param graph 
 * @param dependency 
 * @return true 
 * @return false 
 */
bool Resolver_BacktrackDependencyGraph(struct ResolverState* state, NodeIdx_t dependency_idx)
{
    NodeIdx_t dep_traversed_idx = dependency_idx;
    for (;;)
    {
        // Get first instance of the dependency
        dep_traversed_idx = TraversedNodeArray_FindFirstByNodeIdx(state->traversed_nodes, dependency_idx);
        struct TraversedNode traversed_dep = TraversedNodeArray_Get(state->traversed_nodes, dep_traversed_idx);
        struct TraversedNode traversed_picked_target = TraversedNodeArray_Get(state->traversed_nodes, dep_traversed_idx + 1); // First instance of a dep is ALWAYS followed by its picked artifact

        GraphNode dep_node = state->graph->nodes[traversed_dep.node_idx];
        if (traversed_picked_target.connected_idx == NodeIdxArray_Size(dep_node.connected) - 1)
        {
            if (traversed_picked_target.traversed_parent_idx == -1)
                return false; // It's dependover, there are no more parents

            // Backtrack to that parent dependency!
            dep_traversed_idx = traversed_picked_target.traversed_parent_idx;
            continue;
        }

        // Oh goody, we have backtracked enough
        NodeIdx_t connected_idx = traversed_picked_target.connected_idx + 1;
        TraversedNodeArray_Truncate(state->traversed_nodes, dep_traversed_idx + 1);
        TraversedNodeArray_Add(state->traversed_nodes, (struct TraversedNode) {
            .node_idx = NodeIdxArray_Get(dep_node.connected, connected_idx),
            .connected_idx = connected_idx,
            .traversed_parent_idx = dep_traversed_idx
        });
        return true;
    }
}

bool Resolver_Resolve(DependencyGraph* graph, NodeIdx_t root, NodeIdx_t currentNode, size_t* traversedNodeCount, NodeIdx_t** traversedNodes, KPMIO* kpmIO)
{
    struct ResolverState state = {
        .graph = graph,
        .traversed_nodes = TraversedNodeArray_Create()
    };

    TraversedNodeArray_Add(state.traversed_nodes, (struct TraversedNode) {
        .node_idx = currentNode,
        .traversed_parent_idx = -1,
        .connected_idx = -1
    });

    for (;;)
    {
        size_t current_node_traversed_idx = TraversedNodeArray_Size(state.traversed_nodes) - 1;
        const struct TraversedNode current_traversed_node = TraversedNodeArray_Get(state.traversed_nodes, current_node_traversed_idx);
        GraphNode current_node = state.graph->nodes[current_traversed_node.node_idx]; // Current node
        if (current_node.gt.type == GT_DEPENDENCY)
        {
            // Check if an existing resolution to this dependency id exists
            NodeIdx_t picked_target_idx = NODE_NULL;
            TraversedNodeArray_ForEach(state.traversed_nodes, traversed)
            {
                GraphNode traversed_node = state.graph->nodes[traversed.node_idx];
                if (traversed_node.gt.type == GT_DEPENDENCY)
                    continue;

                if (!strcmp(GetGraphTargetId(traversed_node.gt), GetGraphTargetId(current_node.gt)))
                {
                    picked_target_idx = traversed.node_idx;
                    break;
                }
            }

            if (picked_target_idx != NODE_NULL)
            {
                // Check if previously picked can resolve this dep
                NodeIdx_t connected_idx = NodeIdxArray_FindFirst(current_node.connected, picked_target_idx);
                if (connected_idx != -1)
                {
                    Resolver_Next(&state); // Move to next node
                    continue;
                }

                // Backtrack
                if (!Resolver_BacktrackDependencyGraph(&state, current_traversed_node.node_idx))
                    return false; // Could not backtrack, resolution failure
                continue;
            }
            
            // Pick first target
            TraversedNodeArray_Add(state.traversed_nodes, (struct TraversedNode) {
                .node_idx = NodeIdxArray_Get(current_node.connected, 0),
                .traversed_parent_idx = current_node_traversed_idx,
                .connected_idx = 0
            });
            continue;
        }

        // Target node
        if (NodeIdxArray_Size(current_node.connected) > 0)
        {
            TraversedNodeArray_Add(state.traversed_nodes, (struct TraversedNode) {
                .node_idx = NodeIdxArray_Get(current_node.connected, 0),
                .traversed_parent_idx = current_node_traversed_idx,
                .connected_idx = 0
            });
            continue;
        }

        // Empty target, move to next
        Resolver_Next(&state);
    }
}
