#pragma once

#include "kpm/kpm.h"
#include <assert.h>

typedef int NodeIndex_t;
#define NODE_NULL (NodeIndex_t)(-1)

struct HashmapBucket {
    char* key;
    NodeIndex_t node_count;
    NodeIndex_t* nodes;
    struct HashmapBucket* pnext;
};

typedef struct {
    int bucket_count;
    struct HashmapBucket* buckets;
} Hashmap;

Hashmap* Hashmap_Create(int size);
int Hashmap_Hash(Hashmap* hashmap, char* key);
NodeIndex_t Hashmap_Get(Hashmap* hashmap, NodeIndex_t** nodes, char* key);
void Hashmap_Remove(Hashmap* hashmap, char* key);
void Hashmap_RemoveNode(Hashmap* hashmap, char* key, NodeIndex_t node);
void Hashmap_AddNode(Hashmap* hashmap, char* key, NodeIndex_t node);

typedef enum
{
    GT_DEPENDENCY, /**< Node that declares a dependency between two versions */
    GT_INSTALL_TARGET, /**< Node that declares a specific install target */
    GT_INSTALLED, /**< Node that declares an already installed target */
} GraphTargetType;

typedef struct
{
    GraphTargetType type;
    union {
        Dependency dependency;
        InstallTarget target;
        InstalledPackage installed;
    };
} GraphTarget;

typedef struct
{
    NodeIndex_t connectedCount; /**< Number of dependencies of this node */
    NodeIndex_t* connected; /**< Dependencies of this node */
    GraphTarget gt;
} GraphNode;

typedef struct
{
    GraphNode* nodes; /**< Nodes in this graph */
    NodeIndex_t allocated; /**< The maximum storable nodes of this graph */
    NodeIndex_t nodeCount; /**< The number of nodes currently in this graph */
    Hashmap* targetmap; /**< Map IDs to a list of target or installed nodes */
    Hashmap* depmap; /**< Map IDs to a list of dependency nodes */
} DependencyGraph;

void CreateDependencyGraph(DependencyGraph* graph, int count);
void ExtendDependencyGraph(DependencyGraph* graph, int allocate);
void FreeNode(GraphNode* node);
void FreeDependencyGraph(DependencyGraph* graph);
NodeIndex_t AddNode(DependencyGraph* graph, GraphNode node);
NodeIndex_t CreateTargetNode(DependencyGraph* graph, InstallTarget target);
NodeIndex_t CreateInstalledNode(DependencyGraph* graph, InstalledPackage installed);
NodeIndex_t CreateDependencyNode(DependencyGraph* graph, Dependency dependency);
void AddArtifactDep(DependencyGraph* graph, NodeIndex_t artNodeIdx, NodeIndex_t depNodeIdx);
void AddDepTarget(DependencyGraph* graph, NodeIndex_t depNodeIdx, NodeIndex_t artNodeIdx);
void AddFirstEdge(DependencyGraph* graph, NodeIndex_t firstNodeIndex, NodeIndex_t nextNodeIndex);
void RenderGraph(DependencyGraph* graph, char** output);
void Internal_ArrayAddNode(size_t* traversedNodeCount, NodeIndex_t** traversedNodes, NodeIndex_t node);


bool Internal_ResolveDependencyGraph(DependencyGraph* graph, NodeIndex_t root, NodeIndex_t currentNode,size_t* traversedNodeCount, NodeIndex_t** traversedNodes, KPMIO* statusCallback);