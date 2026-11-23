#ifndef DEPENDENCIES_H
#define DEPENDENCIES_H

#include "array.h"
#include "kpm/kpm.h"
#include <assert.h>

typedef int NodeIdx;
#define NODE_NULL (NodeIdx)(-1)

create_array_h(NodeIdx)

#define NodeIdxArray_ForEachIndex(array, i, current) NodeIdx current; for (int i = 0; i < array->count && (current = array->items[i], 1); i++)

struct NodeIdxHashmapBucket {
    char *key;
    NodeIdxArray *payload;
    struct NodeIdxHashmapBucket *pnext;
};

typedef struct {
    size_t bucket_count;
    struct NodeIdxHashmapBucket *buckets;
} NodeIdxHashmap;

NodeIdxHashmap *NodeIdxHashmap_Create(size_t size);
size_t NodeIdxHashmap_Hash(NodeIdxHashmap *NodeIdxHashmap, char *key);
bool NodeIdxHashmap_Get(NodeIdxHashmap *NodeIdxHashmap, char *key, NodeIdxArray **array);
void NodeIdxHashmap_Remove(NodeIdxHashmap *NodeIdxHashmap, char *key);
void NodeIdxHashmap_AddNode(NodeIdxHashmap *NodeIdxHashmap, char *key, NodeIdx node);
void NodeIdxHashmap_FreeBuckets(struct NodeIdxHashmapBucket *bucket);
void NodeIdxHashmap_Free(NodeIdxHashmap **hashmap);

typedef enum
{
    GT_DEPENDENCY, /**< Node that declares a dependency between two versions */
    GT_INSTALL_TARGET, /**< Node that declares a specific install target */
    GT_INSTALLED, /**< Node that declares a target representing an already-installed package */
} GraphTargetType;

typedef enum
{
    GT_PRIORITY_EXPLICIT=2,
    GT_PRIORITY_INSTALLED=1,
    GT_PRIORITY_NONE=0,
} GraphTargetPriority;

typedef struct
{
    GraphTargetPriority priority; /**< Explicit user-selected target */
    GraphTargetType type;
    union {
        KPM_Dependency dependency;
        KPM_InstallTarget target;
        KPM_InstalledPackage installed;
    };
} DepGraph_Target;

typedef struct
{
    NodeIdxArray *connected;
    DepGraph_Target gt;
} DepGraph_Node;

typedef struct
{
    DepGraph_Node *nodes; /**< Nodes in this graph */
    NodeIdx allocated; /**< The maximum storable nodes of this graph */
    NodeIdx nodeCount; /**< The number of nodes currently in this graph */
    NodeIdxHashmap *targetmap; /**< Map IDs to a list of target or installed nodes */
    NodeIdxHashmap *depmap; /**< Map IDs to a list of dependency nodes */
} DependencyGraph;

DependencyGraph *DepGraph_Create(int count);
void DepGraph_Extend(DependencyGraph *graph, int allocate);
void DepGraph_FreeNode(DepGraph_Node *node);
void DepGraph_FreeGraph(DependencyGraph *graph);
NodeIdx DepGraph_AddNode(DependencyGraph *graph, DepGraph_Node node);
SemVer DepGraph_GetTargetVersion(DepGraph_Target target);
char *DepGraph_GetTargetId(DepGraph_Target target);
NodeIdx DepGraph_CreateTargetNode(DependencyGraph *graph, KPM_InstallTarget target, GraphTargetPriority priority);
NodeIdx DepGraph_CreateInstalledNode(DependencyGraph *graph, KPM_InstalledPackage installed);
NodeIdx DepGraph_CreateDependencyNode(DependencyGraph *graph, KPM_Dependency dependency);
void DepGraph_AddTargetDep(DependencyGraph *graph, NodeIdx art_node_idx, NodeIdx dep_node_idx);
void DepGraph_AddDepTarget(DependencyGraph *graph, NodeIdx dep_node_idx, NodeIdx art_node_idx);
KPM_Result DepGraph_GetTargetDependencies(KPM *kpm, DepGraph_Target target, size_t *targetDependencyCount, KPM_Dependency **targetDependencies, KPMIO *kpmio);
void DepGraph_Render(DependencyGraph *graph, char **output);

NodeIdx DepGraph_FindDependency(DependencyGraph *graph, KPM_Dependency dep);
NodeIdx DepGraph_FindTargetInGraph(DependencyGraph *graph, DepGraph_Target gt);
NodeIdx DepGraph_FindArtifactInGraph(DependencyGraph *graph, KPM_Artifact artifact);
void DepGraph_AddExistingTargetsToDependency(DependencyGraph *graph, NodeIdx dep_idx);
void DepGraph_AddTargetToExistingDependencies(DependencyGraph *graph, NodeIdx target_idx);

bool DepGraph_FillGraphFromTarget(KPM *kpm, DependencyGraph *graph, NodeIdx target_node_id, KPMIO *kpmio);

/**
 * Dependency Resolver
 * 
 */
typedef struct {
    NodeIdx node_idx;         /**< The index of the node in the graph */
    size_t parent_traversed_idx;    /**< The index of the parent in the list of traversed nodes */
    size_t connected_idx;       /**< The index of this node in the parent's connected node list */
} TraversedNode;

create_array_h(TraversedNode);
#define TraversedNodeArray_ForEachIndex(array, i, current) for (int i = 0; i < array->count && (current = array->items[i], 1); i++)
int TraversedNodeArray_FindFirstByNodeIdx(TraversedNodeArray *array, size_t node_idx);

typedef struct {
    DependencyGraph *graph;                 /**< Store a reference to the graph */
    TraversedNodeArray *traversed_nodes;    /**< List of dep and target nodes traversed */
} Resolver_State;

void Resolver_FreeState(Resolver_State *state);

bool Resolver_Next(Resolver_State *state, KPMIO *kpmio);
bool Resolver_Backtrack(Resolver_State *state, NodeIdx dependency_idx, KPMIO *kpmio);
Resolver_State *Resolver_Resolve(DependencyGraph *graph, NodeIdx root, KPMIO *kpmio);

#endif