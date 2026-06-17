#pragma once

#include "kpm/kpm.h"
#include "kpm/semver.h"

typedef size_t NodeIndex_t;

struct DependencyGraph
{
    struct DependencyNode* nodes; /**< Nodes in this graph */
    size_t allocated; /**< The maximum storable nodes of this graph */
    size_t nodeCount; /**< The number of nodes currently in this graph */
};

enum NodeType
{
    NODE_DEPENDENCY, /**< Node that declares a dependency between two versions */
    NODE_ARTIFACT /**< Node that declares a specific artifact at a version */
};

struct DependencyNode
{
    enum NodeType type; /**< The type of node this is */
    NodeIndex_t* connected; /**< Dependencies of this node */
    size_t connectedCount; /**< Number of dependencies of this node */
    char* repository; /**< Package repo, may be NULL */
    char* id; /**< Package id */
    struct SemVer min_version;
    struct SemVer max_version;
};

void CreateDependencyGraph(struct DependencyGraph* graph, int count);
void ExtendDependencyGraph(struct DependencyGraph* graph, int allocate);
void FreeNode(struct DependencyNode* node);
void FreeDependencyGraph(struct DependencyGraph* graph);
NodeIndex_t AddNode(struct DependencyGraph* graph, struct DependencyNode node);
void AddEdge(struct DependencyGraph* graph, NodeIndex_t firstNodeIndex, NodeIndex_t nextNodeIndex);
void AddFirstEdge(struct DependencyGraph* graph, NodeIndex_t firstNodeIndex, NodeIndex_t nextNodeIndex);
bool FindArtifactNode(struct DependencyGraph* graph, char* repository, char* id, struct SemVer version, NodeIndex_t* index);
void RenderGraph(struct DependencyGraph* graph, char** output);
void Internal_ArrayAddNode(size_t* traversedNodeCount, NodeIndex_t** traversedNodes, NodeIndex_t node);

enum KPMResult Internal_GetArtifactDependencies(struct KPM* kpm, struct IndexedArtifact* target, size_t* targetDependencyCount, struct ArtifactDependency** targetDependencies);
bool Internal_NarrowDependency(struct ArtifactDependency* currentDependency, struct ArtifactDependency* targetDependency);
int Internal_ConstructGraphFromArtifact(struct KPM* kpm, struct DependencyGraph* graph, struct IndexedArtifact* artifact);
bool Internal_ResolveDependencyGraph(struct DependencyGraph* graph, NodeIndex_t root, size_t* traversedNodeCount, NodeIndex_t** traversedNodes, struct KPMIO* statusCallback);