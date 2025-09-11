#include "kpm/kpm.h"
#include "kpm/semver.h"

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
    size_t* connected; /**< Dependencies of this node */
    size_t connectedCount; /**< Number of dependencies of this node */
    char* repository; /**< Package repo */
    char* id; /**< Package id */
    struct SemVer* min_version;
    struct SemVer* max_version;
};

void CreateDependencyGraph(struct DependencyGraph* graph, int count);
void ExtendDependencyGraph(struct DependencyGraph* graph, int allocate);
void FreeNode(struct DependencyNode* node);
void FreeDependencyGraph(struct DependencyGraph* graph);
size_t AddNode(struct DependencyGraph* graph, struct DependencyNode node);
void AddEdge(struct DependencyGraph* graph, size_t firstNodeIndex, size_t nextNodeIndex);