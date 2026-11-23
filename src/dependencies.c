#include "dependencies.h"

#include "kpm/kpkg.h"
#include "kpm/kpm.h"
#include "kpm/kpmio.h"
#include "kpm/semver.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal_utils.h"
#include "array.h"

bool NodeIdxEq(NodeIdx a, NodeIdx b) { return a == b; }
create_array_c(NodeIdx, NODE_NULL, NodeIdxEq);

bool TraversedNodeEq(TraversedNode a, TraversedNode b) { return a.node_idx == b.node_idx; }
create_array_c(TraversedNode, (TraversedNode){.node_idx=NODE_NULL}, TraversedNodeEq);

int TraversedNodeArray_FindFirstByNodeIdx(TraversedNodeArray *array, size_t node_idx)
{
    if (array->count == 0)
        return -1;

    TraversedNode current;
    TraversedNodeArray_ForEachIndex(array, i, current)
        if (current.node_idx == node_idx)
            return i;
    return -1;
}

NodeIdxHashmap *NodeIdxHashmap_Create(size_t size)
{
    assert((size & (size-1)) == 0); // Ensure power of 2
    NodeIdxHashmap *h = malloc(sizeof(NodeIdxHashmap));
    *h = (NodeIdxHashmap) {
        .bucket_count = size,
        .buckets = malloc(size * sizeof(struct NodeIdxHashmapBucket))
    };
    memset(h->buckets, 0, sizeof(struct NodeIdxHashmapBucket) * size);
    return h;
}

size_t NodeIdxHashmap_Hash(NodeIdxHashmap *NodeIdxHashmap, char *key)
{
    size_t hash = 0;
    size_t len = strlen(key);
    for (size_t i=0; i < len; i++)
        hash += key[i]; // @TODO: More robust hashing algorithm lol
    return (NodeIdxHashmap->bucket_count - 1) & hash;
}

bool NodeIdxHashmap_Get(NodeIdxHashmap *NodeIdxHashmap, char *key, NodeIdxArray **array)
{
    if (array)
        *array = NULL;
    
    size_t hash = NodeIdxHashmap_Hash(NodeIdxHashmap, key);
    
    for (
        struct NodeIdxHashmapBucket *bucket = NodeIdxHashmap->buckets + hash;
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

void NodeIdxHashmap_Remove(NodeIdxHashmap *NodeIdxHashmap, char *key)
{
    size_t hash = NodeIdxHashmap_Hash(NodeIdxHashmap, key);

    for (
        struct NodeIdxHashmapBucket *bucket = NodeIdxHashmap->buckets + hash;
        bucket != NULL && bucket->key != NULL;
        bucket = bucket->pnext
    )
    {
        if (!strcmp(bucket->key, key))
        {
            free(bucket->key);
            bucket->key = NULL;
            NodeIdxArray_Free(&bucket->payload);
            if (bucket->pnext)
            {
                struct NodeIdxHashmapBucket *to_free = bucket->pnext;
                memcpy(bucket, to_free, sizeof(struct NodeIdxHashmapBucket));
                free(to_free);
            }
            return;
        }
    }
}

void NodeIdxHashmap_AddNode(NodeIdxHashmap *NodeIdxHashmap, char *key, NodeIdx idx)
{
    size_t hash = NodeIdxHashmap_Hash(NodeIdxHashmap, key);

    struct NodeIdxHashmapBucket *bucket;
    for (
        bucket = NodeIdxHashmap->buckets + hash;
        bucket->key != NULL && strcmp(bucket->key, key);
        bucket = bucket->pnext
    )
    {
        if (!bucket->pnext)
        {
            bucket->pnext = malloc(sizeof(struct NodeIdxHashmapBucket));
            bucket = bucket->pnext;
            bucket->key = NULL;
            break;
        }
    }
    
    if (!bucket->key)
    {
        bucket->key = strdup(key);
        bucket->payload = NodeIdxArray_Create();
        bucket->pnext = NULL;
    }

    NodeIdxArray_Add(bucket->payload, idx);
}

void NodeIdxHashmap_FreeBuckets(struct NodeIdxHashmapBucket *bucket)
{
    if (!bucket)
        return;
    NodeIdxHashmap_FreeBuckets(bucket->pnext);
    free(bucket->key);
    NodeIdxArray_Free(&bucket->payload);
    free(bucket);
}

void NodeIdxHashmap_Free(NodeIdxHashmap **hashmap)
{
    for (size_t i=0; i < (*hashmap)->bucket_count; i++)
    {
        NodeIdxHashmap_FreeBuckets((*hashmap)->buckets[i].pnext);
        free((*hashmap)->buckets[i].key);
        NodeIdxArray_Free(&(*hashmap)->buckets[i].payload);
    }

    free((*hashmap)->buckets);
    free(*hashmap);
    *hashmap = NULL;
}

void KPM_FreeDependency(KPM_Dependency *dependency)
{
    free(dependency->id);
    dependency->id = NULL;
}

void KPM_FreeDependencyList(size_t dependency_count, KPM_Dependency *dependencies)
{
    for (size_t i=0; i < dependency_count; i++)
        KPM_FreeDependency(&dependencies[i]);
    
    free(dependencies);
}

/**
 * @brief Create a Dependency Graph object
 * 
 * @param graph Pointer to an empty graph struct to initialise
 * @param count Number of nodes to allocate in the graph object initially
 */
DependencyGraph *DepGraph_Create(int count)
{
    DependencyGraph *graph = malloc(sizeof(DependencyGraph));
    graph->nodeCount = 0;
    graph->nodes = NULL;
    graph->allocated = 0;
    graph->depmap = NodeIdxHashmap_Create(32);
    graph->targetmap = NodeIdxHashmap_Create(32);

    if (count != 0)
    {
        graph->nodes = malloc(sizeof(DepGraph_Node) * count);
        graph->allocated = count;
    }

    return graph;
}

/**
 * @brief Free a node object
 * 
 * @param node The node to free
 */
void DepGraph_FreeNode(DepGraph_Node *node)
{
    switch (node->gt.type)
    {
    case GT_DEPENDENCY:
        KPM_FreeDependency(&node->gt.dependency);
        break;
    case GT_INSTALL_TARGET:
        KPM_FreeInstallTarget(&node->gt.target);
        break;
    case GT_INSTALLED:
        KPM_FreeInstalledPackage(&node->gt.installed);
        break;
    }
    
    NodeIdxArray_Free(&node->connected);
}

/**
 * @brief Free a dependency graph and all the nodes in it
 * 
 * @param graph The graph to free
 */
void DepGraph_FreeGraph(DependencyGraph *graph)
{
    for (NodeIdx i=0; i < graph->nodeCount; i++)
    {
        DepGraph_FreeNode(&graph->nodes[i]);
    }

    NodeIdxHashmap_Free(&graph->depmap);
    NodeIdxHashmap_Free(&graph->targetmap);
    free(graph->nodes);
    free(graph);
}

/**
 * @brief Allocate more space for a dependency grpah
 * 
 * @param graph The dependency graph object
 * @param allocate How many nodes to expand it by
 */
void DepGraph_Extend(DependencyGraph *graph, int allocate)
{
    graph->allocated += allocate;
    graph->nodes = realloc(graph->nodes, sizeof(DepGraph_Node) * graph->allocated);
}

/**
 * @brief Add a node to the dependency graph
 * 
 * @param graph 
 * @param node 
 * @return NodeIndex_t 
 */
NodeIdx DepGraph_AddNode(DependencyGraph *graph, DepGraph_Node node)
{
    if (graph->nodeCount == graph->allocated)
        DepGraph_Extend(graph, graph->allocated/2 + 1); // ~1.5x growth factor

    graph->nodes[graph->nodeCount++] = node;

    if (node.gt.type == GT_DEPENDENCY)
        NodeIdxHashmap_AddNode(graph->depmap, DepGraph_GetTargetId(node.gt), graph->nodeCount-1);
    else
        NodeIdxHashmap_AddNode(graph->targetmap, DepGraph_GetTargetId(node.gt), graph->nodeCount-1);

    return graph->nodeCount-1;
}

SemVer DepGraph_GetTargetVersion(DepGraph_Target target)
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

char *DepGraph_GetTargetId(DepGraph_Target target)
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
void DepGraph_AddTargetDep(DependencyGraph *graph, NodeIdx art_node_idx, NodeIdx dep_node_idx)
{
    NodeIdxArray_Add(graph->nodes[art_node_idx].connected, dep_node_idx);
}


void DepGraph_AddDepTarget(DependencyGraph *graph, NodeIdx dep_node_idx, NodeIdx target_node_idx)
{
    DepGraph_Node target_node = graph->nodes[target_node_idx];
    assert(graph->nodes[dep_node_idx].gt.type == GT_DEPENDENCY);
    assert(target_node.gt.type == GT_INSTALL_TARGET || target_node.gt.type == GT_INSTALLED);
    
    NodeIdx insertionIndex = graph->nodes[dep_node_idx].connected->count - 1;
    for (; insertionIndex > 0; insertionIndex--)
    {
        DepGraph_Node prev_node = graph->nodes[graph->nodes[dep_node_idx].connected->items[insertionIndex - 1]];
        if (prev_node.gt.priority > target_node.gt.priority)
            break; // Insert here

        if (SemVerCmp(
            DepGraph_GetTargetVersion(prev_node.gt),
            DepGraph_GetTargetVersion(target_node.gt)
        ) > 0)
            break; // Insert here
    }
    
    NodeIdxArray_Insert(graph->nodes[dep_node_idx].connected, insertionIndex, target_node_idx);
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
KPM_Result DepGraph_GetTargetDependencies(KPM *kpm, DepGraph_Target target, size_t *targetDependencyCount, KPM_Dependency **targetDependencies, KPMIO *kpmio)
{
    switch (target.type)
    {
    case GT_INSTALL_TARGET:
        if (target.target.type == INSTALL_ARTIFACT)
            return KPM_ListArtifactDependencies(kpm, target.target.artifact.repository, target.target.artifact.id, target.target.artifact.url, targetDependencyCount, targetDependencies);

        *targetDependencyCount = KPKG_GetDependencies(target.target.file, (KPKG_DependencyEntry**) targetDependencies);
        return KPM_OK;
    case GT_INSTALLED:
        return KPM_ListInstalledPackageDependencies(kpm, target.installed.id, targetDependencyCount, targetDependencies);
    case GT_DEPENDENCY:
        assert(false);
    };
}

void strcat_hd(char *dest, char *src, size_t *length, bool dry)
{
    *length += strlen(src);
    if (!dry)
        strcat(dest, src);
}

void node2str(DependencyGraph *graph, NodeIdx index, char **output, bool declaration)
{
    if (!declaration)
    {
        *output = asprintf_hd("%i", index);
        return;
    }
    
    DepGraph_Node node = graph->nodes[index];
    switch (node.gt.type) {
    case GT_DEPENDENCY:
        *output = asprintf_hd(
            "%i([\"(%i)\n%s\n%u.%u.%u to %u.%u.%u\"])",
            index,
            index,
            node.gt.dependency.id,
            node.gt.dependency.min_version.major,
            node.gt.dependency.min_version.minor,
            node.gt.dependency.min_version.patch,
            node.gt.dependency.max_version.major,
            node.gt.dependency.max_version.minor,
            node.gt.dependency.max_version.patch
        );
        break;
    case GT_INSTALL_TARGET:
        if (node.gt.target.type == INSTALL_ARTIFACT)
        {
            *output = asprintf_hd(
                "%i[\"(%i)\nART\n%s\n%s\n%u.%u.%u\n(P:%i)\"]",
                index,
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
                "%i[\"(%i)\nFILE\n(NO REPO)\n%s\n%u.%u.%u\n(P:%i)\"]",
                index,
                index,
                KPKG_GetId(node.gt.target.file),
                KPKG_GetVersion(node.gt.target.file).major,
                KPKG_GetVersion(node.gt.target.file).minor,
                KPKG_GetVersion(node.gt.target.file).patch,
                node.gt.priority
            );
        }
        break;
    case GT_INSTALLED:
        *output = asprintf_hd(
            "%i[\"(%i)\nINST\n%s\n%s\n%u.%u.%u\n(P:%i)\n(D:%i)\"]",
            index,
            index,
            node.gt.installed.repository,
            node.gt.installed.id,
            node.gt.installed.version.major,
            node.gt.installed.version.minor,
            node.gt.installed.version.patch,
            node.gt.priority,
            node.gt.installed.installed_as_dependency
        );
        break;
    }
}

int Internal_RenderGraph(DependencyGraph *graph, char *output, bool dry)
{
    size_t outputLength=0;
    strcat_hd(output, "flowchart TD\n", &outputLength, dry);

    for (NodeIdx i=0; i < graph->nodeCount; i++)
    {
        char *nodeName = NULL;
        node2str(graph, i, &nodeName, true);
        strcat_hd(output, "    ", &outputLength, dry);
        strcat_hd(output, nodeName, &outputLength, dry);
        strcat_hd(output, "\n", &outputLength, dry);
        free(nodeName);

        NodeIdxArray_ForEachIndex(graph->nodes[i].connected, j, connected_idx)
        {
            char *connectedName = NULL;
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

void DepGraph_Render(DependencyGraph *graph, char **output)
{
    size_t targetSize = 1 + Internal_RenderGraph(graph, NULL, true);
    *output = malloc(targetSize);
    //memset(*output, 0, targetSize);
    *output[0] = '\0';
    Internal_RenderGraph(graph, *output, false);
}

NodeIdx DepGraph_CreateTargetNode(DependencyGraph *graph, KPM_InstallTarget target, GraphTargetPriority priority)
{
    return DepGraph_AddNode(graph, (DepGraph_Node) {
        .connected = NodeIdxArray_Create(),
        .gt = {
            .priority = priority,
            .type = GT_INSTALL_TARGET,
            .target = target
        }
    });
}

NodeIdx DepGraph_CreateInstalledNode(DependencyGraph *graph, KPM_InstalledPackage installed)
{
    return DepGraph_AddNode(graph, (DepGraph_Node) {
        .connected = NodeIdxArray_Create(),
        .gt = {
            .priority = GT_PRIORITY_INSTALLED,
            .type = GT_INSTALLED,
            .installed = installed
        }
    });
}

NodeIdx DepGraph_CreateDependencyNode(DependencyGraph *graph, KPM_Dependency dependency)
{
    return DepGraph_AddNode(graph, (DepGraph_Node) {
        .connected = NodeIdxArray_Create(),
        .gt = {
            .priority = GT_PRIORITY_NONE,
            .type = GT_DEPENDENCY,
            .dependency = dependency
        }
    });
}

NodeIdx DepGraph_FindDependency(DependencyGraph *graph, KPM_Dependency dep)
{
    NodeIdxArray *deps_found_array;
    NodeIdxHashmap_Get(graph->depmap, dep.id, &deps_found_array);
    NodeIdxArray_ForEachIndex(deps_found_array, i, idx)
    {
        DepGraph_Node node = graph->nodes[idx];
        assert(node.gt.type == GT_DEPENDENCY);

        if (SemVerCmp(node.gt.dependency.min_version, dep.min_version))
            continue;

        if (SemVerCmp(node.gt.dependency.max_version, dep.max_version))
            continue;

        return idx;
    }
    return NODE_NULL;
}

NodeIdx DepGraph_FindTargetInGraph(DependencyGraph *graph, DepGraph_Target gt)
{
    NodeIdxArray *targets_found_array;
    NodeIdxHashmap_Get(graph->targetmap, DepGraph_GetTargetId(gt), &targets_found_array);
    NodeIdxArray_ForEachIndex(targets_found_array, i, idx)
    {
        DepGraph_Node node = graph->nodes[idx];
        assert(node.gt.type != GT_DEPENDENCY);

        if (SemVerCmp(DepGraph_GetTargetVersion(gt), DepGraph_GetTargetVersion(node.gt)))
            continue;

        return idx;
    }

    return NODE_NULL;
}

NodeIdx DepGraph_FindArtifactInGraph(DependencyGraph *graph, KPM_Artifact artifact)
{
    NodeIdxArray *targets_found_array;
    NodeIdxHashmap_Get(graph->targetmap, artifact.id, &targets_found_array);
    NodeIdxArray_ForEachIndex(targets_found_array, i, idx)
    {
        DepGraph_Node node = graph->nodes[idx];
        assert(node.gt.type != GT_DEPENDENCY);

        if (SemVerCmp(artifact.version, DepGraph_GetTargetVersion(node.gt)))
            continue;

        return idx;
    }

    return NODE_NULL;
}

void DepGraph_AddExistingTargetsToDependency(DependencyGraph *graph, NodeIdx dep_idx)
{
    assert(graph->nodes[dep_idx].gt.type == GT_DEPENDENCY);
    KPM_Dependency dep = graph->nodes[dep_idx].gt.dependency;

    NodeIdxArray *targets_found_array;
    NodeIdxHashmap_Get(graph->targetmap, dep.id, &targets_found_array);
    NodeIdxArray_ForEachIndex(targets_found_array, i, target_idx)
    {
        DepGraph_Node node = graph->nodes[target_idx];
        assert(node.gt.type != GT_DEPENDENCY);

        if (
            SemVerCmp(
                DepGraph_GetTargetVersion(node.gt),
                dep.min_version
            ) >= 0 &&
            SemVerCmp(
                DepGraph_GetTargetVersion(node.gt),
                dep.max_version
            ) < 0
        )
        {
            DepGraph_AddDepTarget(graph, dep_idx, target_idx);
        }
    }
}

void DepGraph_AddTargetToExistingDependencies(DependencyGraph *graph, NodeIdx target_idx)
{
    DepGraph_Node target_node = graph->nodes[target_idx];
    assert(target_node.gt.type != GT_DEPENDENCY);

    NodeIdxArray *dep_node_ids;
    NodeIdxHashmap_Get(graph->depmap, DepGraph_GetTargetId(target_node.gt), &dep_node_ids);
    NodeIdxArray_ForEachIndex(dep_node_ids, i, dep_idx)
    {
        DepGraph_Node dep_node = graph->nodes[dep_idx];

        if (
            SemVerCmp(
                DepGraph_GetTargetVersion(target_node.gt),
                dep_node.gt.dependency.min_version
            ) >= 0 &&
            SemVerCmp(
                DepGraph_GetTargetVersion(target_node.gt),
                dep_node.gt.dependency.max_version
            ) < 0
        )
        {
            DepGraph_AddDepTarget(graph, dep_idx, target_idx);
        }
    }
}

bool DepGraph_FillGraphFromTarget(KPM *kpm, DependencyGraph *graph, NodeIdx target_node_id, KPMIO *kpmio)
{
    DepGraph_Target graph_target = graph->nodes[target_node_id].gt;
    assert(graph_target.type == GT_INSTALL_TARGET);
    assert(graph_target.type == GT_INSTALLED);

    DepGraph_AddTargetToExistingDependencies(graph, target_node_id);

    // Go through this target's own dependencies
    size_t dep_count;
    KPM_Dependency *dependencies;
    if (DepGraph_GetTargetDependencies(kpm, graph_target, &dep_count, &dependencies, kpmio) != KPM_OK)
        return false;

    for (int i=0; i < dep_count; i++)
    {
        KPM_Dependency dep = dependencies[i];

        // Check if this dependency already exists in the graph
        NodeIdx dep_found = DepGraph_FindDependency(graph, dep);
        if (dep_found != NODE_NULL)
        {
            DepGraph_AddTargetDep(graph, target_node_id, dep_found);
            continue;
        }

        // Gotta make AND POPULATE the dep node ourselves!
        NodeIdx dep_idx = DepGraph_CreateDependencyNode(graph, dep);

        // For each target in the graph that fits, add it to our dependency
        DepGraph_AddExistingTargetsToDependency(graph, dep_idx);

        // Find all artifacts NOT in the graph that satisfy this dependency and recursively add them
        size_t artifact_count;
        KPM_Artifact *artifacts;
        if (!KPM_ListPackageArtifacts(kpm, NULL, dep.id,  &artifact_count, &artifacts))
        {
            KPM_FreeArtifactList(artifact_count, artifacts);
            KPM_FreeDependencyList(dep_count, dependencies);
            return false;
        }

        for (int j = 0; j < artifact_count; j++)
        {            
            NodeIdx existing_artifact = DepGraph_FindArtifactInGraph(graph, artifacts[j]);
            if (existing_artifact != NODE_NULL)
                continue;

            // This artifact is new, we should add it to the graph...
            NodeIdx idx = DepGraph_CreateTargetNode(
                graph,
                (KPM_InstallTarget) {
                    .type = INSTALL_ARTIFACT,
                    .artifact = artifacts[j]
                },
                GT_PRIORITY_NONE
            );

            if (idx == NODE_NULL)
            {
                KPM_FreeArtifactList(artifact_count, artifacts);
                KPM_FreeDependencyList(dep_count, dependencies);
                return false;
            }

            DepGraph_AddDepTarget(graph, dep_idx, idx);

            if (!DepGraph_FillGraphFromTarget(
                kpm,
                graph,
                idx,
                kpmio
            ))
            {
                KPM_FreeArtifactList(artifact_count, artifacts);
                KPM_FreeDependencyList(dep_count, dependencies);
                return false;
            }
        }
        KPM_FreeArtifactList(artifact_count, artifacts);
    }

    KPM_FreeDependencyList(dep_count, dependencies);
    return true;
}

void Resolver_FreeState(Resolver_State *state)
{
    TraversedNodeArray_Free(&state->traversed_nodes);
    DepGraph_FreeGraph(state->graph);
    free(state);
}

bool Resolver_Next(Resolver_State *state, KPMIO *kpmio)
{
    size_t current_traversed_node_idx = TraversedNodeArray_Size(state->traversed_nodes) - 1;
    size_t parent_traversed_node_idx = TraversedNodeArray_Get(state->traversed_nodes, current_traversed_node_idx).parent_traversed_idx;

    TraversedNode current_traversed_node;
    TraversedNode parent_traversed_node;
    for (;;)
    {
        current_traversed_node = TraversedNodeArray_Get(state->traversed_nodes, current_traversed_node_idx);
        parent_traversed_node = TraversedNodeArray_Get(state->traversed_nodes, parent_traversed_node_idx);

        DepGraph_Node parent_node = state->graph->nodes[parent_traversed_node.node_idx];
        if (
            parent_node.gt.type != GT_DEPENDENCY ||
            current_traversed_node.connected_idx == NodeIdxArray_Size(parent_node.connected) - 1
        )
        {
            current_traversed_node_idx = parent_traversed_node_idx;
            parent_traversed_node_idx = parent_traversed_node.parent_traversed_idx;
            continue;
        }

        // Add node
        TraversedNodeArray_Add(state->traversed_nodes, (TraversedNode) {
            .connected_idx = current_traversed_node.connected_idx + 1,
            .node_idx = NodeIdxArray_Get(parent_node.connected, current_traversed_node.connected_idx + 1),
            .parent_traversed_idx = parent_traversed_node_idx
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
bool Resolver_Backtrack(Resolver_State *state, NodeIdx dependency_idx, KPMIO *kpmio)
{
    NodeIdx dep_traversed_idx = dependency_idx;
    for (;;)
    {
        // Get first instance of the dependency and the picked target
        dep_traversed_idx = TraversedNodeArray_FindFirstByNodeIdx(state->traversed_nodes, dep_traversed_idx);
        TraversedNode dep_traversed = TraversedNodeArray_Get(state->traversed_nodes, dep_traversed_idx);
        TraversedNode picked_target_traversed = TraversedNodeArray_Get(state->traversed_nodes, dep_traversed_idx + 1);
        assert(picked_target_traversed.parent_traversed_idx == dep_traversed_idx);

        DepGraph_Node dep_node = state->graph->nodes[dep_traversed.node_idx];
        kpmio->log(KPM_VERBOSITY_DEBUG, "Backtracking: (%i) %s", dep_traversed.node_idx, DepGraph_GetTargetId(dep_node.gt));
        if (picked_target_traversed.connected_idx == NodeIdxArray_Size(dep_node.connected) - 1)
        {
            // Backtrack to the parent dependency!
            do
            {
                if (dep_traversed.parent_traversed_idx == -1)
                    return false; // It's dependover, there are no more parents

                dep_traversed_idx = TraversedNodeArray_Get(state->traversed_nodes, dep_traversed_idx).parent_traversed_idx;
            } while (state->graph->nodes[TraversedNodeArray_Get(state->traversed_nodes, dep_traversed_idx).node_idx].gt.type != GT_DEPENDENCY);
            continue;
        }

        // Oh goody, we have backtracked enough
        NodeIdx connected_idx = picked_target_traversed.connected_idx + 1;
        TraversedNodeArray_Truncate(state->traversed_nodes, dep_traversed_idx + 1);
        TraversedNodeArray_Add(state->traversed_nodes, (TraversedNode) {
            .node_idx = NodeIdxArray_Get(dep_node.connected, connected_idx),
            .connected_idx = connected_idx,
            .parent_traversed_idx = dep_traversed_idx
        });
        return true;
    }
}

Resolver_State *Resolver_Resolve(DependencyGraph *graph, NodeIdx root, KPMIO *kpmio)
{
    Resolver_State *state = malloc(sizeof(Resolver_State));
    *state = (Resolver_State) {
        .graph = graph,
        .traversed_nodes = TraversedNodeArray_Create()
    };

    TraversedNodeArray_Add(state->traversed_nodes, (TraversedNode) {
        .node_idx = root,
        .parent_traversed_idx = -1,
        .connected_idx = -1
    });

    for (;;)
    {
        const size_t current_node_traversed_idx = TraversedNodeArray_Size(state->traversed_nodes) - 1;
        const TraversedNode current_node_traversed = TraversedNodeArray_Get(state->traversed_nodes, current_node_traversed_idx);
        const DepGraph_Node current_node = state->graph->nodes[current_node_traversed.node_idx]; // Current node

        if (current_node.gt.type == GT_DEPENDENCY)
        {
            kpmio->log(KPM_VERBOSITY_DEBUG, "Resolving dependency (%i) %s %i.%i.%i - %i.%i.%i", current_node_traversed.node_idx, DepGraph_GetTargetId(current_node.gt), current_node.gt.dependency.min_version.major, current_node.gt.dependency.min_version.minor, current_node.gt.dependency.min_version.patch, current_node.gt.dependency.max_version.major, current_node.gt.dependency.max_version.minor, current_node.gt.dependency.max_version.patch);

            // Check if an existing resolution for this dependency id exists
            NodeIdx picked_target_idx = NODE_NULL;
            TraversedNode traversed;
            TraversedNodeArray_ForEachIndex(state->traversed_nodes, i, traversed)
            {
                DepGraph_Node traversed_node = state->graph->nodes[traversed.node_idx];
                if (traversed_node.gt.type == GT_DEPENDENCY)
                    continue;

                if (!strcmp(DepGraph_GetTargetId(traversed_node.gt), DepGraph_GetTargetId(current_node.gt)))
                {
                    kpmio->log(KPM_VERBOSITY_DEBUG, "Found existing resolution (%i) %s (%i.%i.%i)", traversed.node_idx, DepGraph_GetTargetId(traversed_node.gt), DepGraph_GetTargetVersion(traversed_node.gt).major, DepGraph_GetTargetVersion(traversed_node.gt).minor, DepGraph_GetTargetVersion(traversed_node.gt).patch);
                    picked_target_idx = traversed.node_idx;
                    break;
                }
            }

            if (picked_target_idx != NODE_NULL)
            {
                // Check if the previously picked can resolve this dep
                NodeIdx connected_idx = NodeIdxArray_FindFirst(current_node.connected, picked_target_idx);
                if (connected_idx != -1)
                {
                    if (!Resolver_Next(state, kpmio))
                        break;  // If there is no next after this empty target, we're done!
                    continue;
                }

                // Backtrack
                kpmio->log(KPM_VERBOSITY_DEBUG, "Conflict detected!");
                if (!Resolver_Backtrack(state, current_node_traversed.node_idx, kpmio))
                {
                    Resolver_FreeState(state);
                    return NULL; // Could not backtrack, resolution failure
                }
                continue;
            }
            
            // Pick first target
            TraversedNodeArray_Add(state->traversed_nodes, (TraversedNode) {
                .node_idx = NodeIdxArray_Get(current_node.connected, 0),
                .parent_traversed_idx = current_node_traversed_idx,
                .connected_idx = 0
            });
            continue;
        }

        // Target node
        kpmio->log(KPM_VERBOSITY_DEBUG, "Resolving target (%i) %s", current_node_traversed.node_idx, DepGraph_GetTargetId(current_node.gt));

        // Check if previously traversed
        if (TraversedNodeArray_FindFirstByNodeIdx(state->traversed_nodes, current_node_traversed.node_idx) < current_node_traversed_idx)
            if (!Resolver_Next(state, kpmio))
                break; // If there is no next after this empty target, we're done!

        if (NodeIdxArray_Size(current_node.connected) > 0)
        {
            kpmio->log(KPM_VERBOSITY_DEBUG, "Resolving target dependencies...");
            TraversedNodeArray_Add(state->traversed_nodes, (TraversedNode) {
                .node_idx = NodeIdxArray_Get(current_node.connected, 0),
                .parent_traversed_idx = current_node_traversed_idx,
                .connected_idx = 0
            });
            continue;
        }

        // Empty target, move to next
        if (!Resolver_Next(state, kpmio))
            break; // If there is no next after this empty target, we're done!
    }

    return state;
}
