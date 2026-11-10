#ifndef DEPENDENCIES_H
#define DEPENDENCIES_H

#include "array.h"
#include "kpm/kpm.h"
#include <assert.h>

typedef int NodeIdx;
#define NODE_NULL (NodeIdx)(-1)

create_array_h(NodeIdx)

#define NodeIdxArray_ForEach(array, current) NodeIdx current; for (int i = 0; current = array->items[i], i < array->count; i++)
#define NodeIdxArray_ForEachIndex(array, i, current) NodeIdx current; for (int i = 0; current = array->items[i], i < array->count; i++)

struct HashmapBucket {
    char *key;
    NodeIdxArray *payload;
    struct HashmapBucket *pnext;
};

typedef struct {
    int bucket_count;
    struct HashmapBucket *buckets;
} Hashmap;

Hashmap *Hashmap_Create(int size);
int Hashmap_Hash(Hashmap *hashmap, char *key);
bool Hashmap_Get(Hashmap *hashmap, char *key, NodeIdxArray **array);
void Hashmap_Remove(Hashmap *hashmap, char *key);
void Hashmap_AddNode(Hashmap *hashmap, char *key, NodeIdx node);

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
    Hashmap *targetmap; /**< Map IDs to a list of target or installed nodes */
    Hashmap *depmap; /**< Map IDs to a list of dependency nodes */
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
void DepGraph_AddArtifactDep(DependencyGraph *graph, NodeIdx art_node_idx, NodeIdx dep_node_idx);
void DepGraph_AddDepTarget(DependencyGraph *graph, NodeIdx dep_node_idx, NodeIdx art_node_idx);
void DepGraph_Render(DependencyGraph *graph, char **output);

bool DepGraph_FillGraphFromTarget(KPM *kpm, DependencyGraph *graph, NodeIdx target_node_id, KPMIO *kpm_io);

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
#define TraversedNodeArray_ForEach(array, current) for (int i = 0; current = array->items[i], i < array->count; i++)
#define TraversedNodeArray_ForEachIndex(array, i, current) for (int i = 0; current = array->items[i], i < array->count; i++)
int TraversedNodeArray_FindFirstByNodeIdx(TraversedNodeArray *array, size_t node_idx);

typedef struct {
    DependencyGraph *graph;                 /**< Store a reference to the graph */
    TraversedNodeArray *traversed_nodes;    /**< List of dep and target nodes traversed */
} Resolver_State;

void Resolver_FreeState(Resolver_State *state);
Resolver_State *Resolver_Resolve(DependencyGraph *graph, NodeIdx root, KPMIO *kpm_io);

#endif