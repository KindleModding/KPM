#pragma once

#include "kpm/kpm.h"
#include "kpm/semver.h"

typedef size_t NodeIndex_t;

typedef enum
{
    NODE_DEPENDENCY, /**< Node that declares a dependency between two versions */
    NODE_ARTIFACT /**< Node that declares a specific artifact at a version */
} NodeType;

struct DependencyNode
{
    NodeType type; /**< The type of node this is */
    NodeIndex_t* connected; /**< Dependencies of this node */
    size_t connectedCount; /**< Number of dependencies of this node */
    char* repository; /**< Package repo, may be NULL */
    char* id; /**< Package id */
    char* url; /**< Artifact url */
    SemVer min_version;
    SemVer max_version;
};
typedef struct DependencyNode DependencyNode;

struct DependencyGraph
{
    DependencyNode* nodes; /**< Nodes in this graph */
    size_t allocated; /**< The maximum storable nodes of this graph */
    size_t nodeCount; /**< The number of nodes currently in this graph */
};
typedef struct DependencyGraph DependencyGraph;

void CreateDependencyGraph(DependencyGraph* graph, int count);
void ExtendDependencyGraph(DependencyGraph* graph, int allocate);
void FreeNode(DependencyNode* node);
void FreeDependencyGraph(DependencyGraph* graph);
NodeIndex_t AddNode(DependencyGraph* graph, DependencyNode node);
void AddEdge(DependencyGraph* graph, NodeIndex_t firstNodeIndex, NodeIndex_t nextNodeIndex);
void AddFirstEdge(DependencyGraph* graph, NodeIndex_t firstNodeIndex, NodeIndex_t nextNodeIndex);
bool FindArtifactNode(DependencyGraph* graph, char* id, SemVer version, NodeIndex_t* index);
void RenderGraph(DependencyGraph* graph, char** output);
void Internal_ArrayAddNode(size_t* traversedNodeCount, NodeIndex_t** traversedNodes, NodeIndex_t node);

KPMResult Internal_GetArtifactDependencies(KPM* kpm, IndexedArtifact* target, size_t* targetDependencyCount, ArtifactDependency** targetDependencies, KPMIO* statusCallback);
bool Internal_NarrowDependency(ArtifactDependency* currentDependency, ArtifactDependency* targetDependency);
int Internal_ConstructGraphFromArtifact(KPM* kpm, DependencyGraph* graph, IndexedArtifact* artifact, KPMIO* statusCallback);
bool Internal_ResolveDependencyGraph(DependencyGraph* graph, NodeIndex_t root, NodeIndex_t currentNode,size_t* traversedNodeCount, NodeIndex_t** traversedNodes, KPMIO* statusCallback);