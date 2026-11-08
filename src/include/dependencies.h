#pragma once

#include "kpm/kpm.h"
#include <assert.h>

typedef int NodeIdx_t;
#define NODE_NULL (NodeIdx_t)(-1)

typedef struct {
    size_t count;
    NodeIdx_t* items;
} NodeIdxArray;

#define NodeIdxArray_ForEach(array, current) NodeIdx_t current; for (int i = 0; current = array->items[i], i < array->count; i++)
#define NodeIdxArray_ForEachIndex(array, i, current) NodeIdx_t current; for (int i = 0; current = array->items[i], i < array->count; i++)

NodeIdxArray* NodeIdxArray_Create();
void NodeIdxArray_Free(NodeIdxArray* array);
void NodeIdxArray_Add(NodeIdxArray* array, NodeIdx_t item);
size_t NodeIdxArray_FindFirst(NodeIdxArray* array, NodeIdx_t item);
void NodeIdxArray_Insert(NodeIdxArray* array, size_t index, NodeIdx_t node_idx);
int NodeIdxArray_Size(NodeIdxArray* array);
NodeIdx_t NodeIdxArray_Get(NodeIdxArray* array, size_t index);

struct HashmapBucket {
    char* key;
    NodeIdxArray* payload;
    struct HashmapBucket* pnext;
};

typedef struct {
    int bucket_count;
    struct HashmapBucket* buckets;
} Hashmap;

Hashmap* Hashmap_Create(int size);
int Hashmap_Hash(Hashmap* hashmap, char* key);
bool Hashmap_Get(Hashmap* hashmap, char* key, NodeIdxArray** array);
void Hashmap_Remove(Hashmap* hashmap, char* key);
void Hashmap_AddNode(Hashmap* hashmap, char* key, NodeIdx_t node);

typedef enum
{
    GT_DEPENDENCY, /**< Node that declares a dependency between two versions */
    GT_INSTALL_TARGET, /**< Node that declares a specific install target */
    GT_INSTALLED, /**< Node that declares an already installed target */
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
        Dependency dependency;
        InstallTarget target;
        InstalledPackage installed;
    };
} GraphTarget;

typedef struct
{
    NodeIdxArray* connected;
    GraphTarget gt;
} GraphNode;

typedef struct
{
    GraphNode* nodes; /**< Nodes in this graph */
    NodeIdx_t allocated; /**< The maximum storable nodes of this graph */
    NodeIdx_t nodeCount; /**< The number of nodes currently in this graph */
    Hashmap* targetmap; /**< Map IDs to a list of target or installed nodes */
    Hashmap* depmap; /**< Map IDs to a list of dependency nodes */
} DependencyGraph;

void CreateDependencyGraph(DependencyGraph* graph, int count);
void ExtendDependencyGraph(DependencyGraph* graph, int allocate);
void FreeNode(GraphNode* node);
void FreeDependencyGraph(DependencyGraph* graph);
NodeIdx_t AddNode(DependencyGraph* graph, GraphNode node);
NodeIdx_t CreateTargetNode(DependencyGraph* graph, InstallTarget target, GraphTargetPriority priority);
NodeIdx_t CreateInstalledNode(DependencyGraph* graph, InstalledPackage installed);
NodeIdx_t CreateDependencyNode(DependencyGraph* graph, Dependency dependency);
void AddArtifactDep(DependencyGraph* graph, NodeIdx_t artNodeIdx, NodeIdx_t depNodeIdx);
void AddDepTarget(DependencyGraph* graph, NodeIdx_t depNodeIdx, NodeIdx_t artNodeIdx);
void AddFirstEdge(DependencyGraph* graph, NodeIdx_t firstNodeIndex, NodeIdx_t nextNodeIndex);
void RenderGraph(DependencyGraph* graph, char** output);
void Internal_ArrayAddNode(size_t* traversedNodeCount, NodeIdx_t** traversedNodes, NodeIdx_t node);

/**
 * Dependency Resolver
 * 
 */
struct TraversedNode {
    NodeIdx_t node_idx;         /**< The index of the node in the graph */
    size_t traversed_parent_idx;    /**< The index of the parent in the list of traversed nodes */
    size_t connected_idx;       /**< The index of this node in the parent's connected node list */
};

typedef struct {
    size_t count;
    struct TraversedNode* items;
} TraversedNodeArray;

#define TraversedNodeArray_ForEach(array, current) struct TraversedNode current; for (int i = 0; current = array->items[i], i < array->count; i++)
#define TraversedNodeArray_ForEachIndex(array, i, current) struct TraversedNode current; for (int i = 0; current = array->items[i], i < array->count; i++)

TraversedNodeArray* TraversedNodeArray_Create();
void TraversedNodeArray_Free(TraversedNodeArray* array);
void TraversedNodeArray_Truncate(TraversedNodeArray* array, size_t size);
void TraversedNodeArray_Add(TraversedNodeArray* array, struct TraversedNode item);
int TraversedNodeArray_FindFirstByNodeIdx(TraversedNodeArray* array, size_t node_idx);
int TraversedNodeArray_Size(TraversedNodeArray* array);
const struct TraversedNode TraversedNodeArray_Get(TraversedNodeArray* array, size_t idx);


struct ResolverState {
    DependencyGraph* graph;                     /**< Store a reference to the graph */
    TraversedNodeArray* traversed_nodes;              /**< List of dep and target nodes traversed */
};

bool Resolver_Resolve(DependencyGraph* graph, NodeIdx_t root, NodeIdx_t currentNode,size_t* traversedNodeCount, NodeIdx_t** traversedNodes, KPMIO* statusCallback);