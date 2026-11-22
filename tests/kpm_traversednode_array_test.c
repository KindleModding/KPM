#include "dependencies.h"
#include "kpm_test_common.h"

int main(int argc, char* argv[])
{
    TraversedNodeArray *array = TraversedNodeArray_Create();
    ASSERT_HD(TraversedNodeArray_Size(array), 0);
    ASSERT_HD(TraversedNodeArray_Get(array, -1).node_idx, NODE_NULL);
    ASSERT_HD(TraversedNodeArray_Get(array, 0).node_idx, NODE_NULL);
    ASSERT_HD(TraversedNodeArray_Get(array, 1).node_idx, NODE_NULL);

    ASSERT_HD(TraversedNodeArray_FindFirstByNodeIdx(array, 0), -1);
    TraversedNodeArray_Add(array, (TraversedNode) { .node_idx = 0 });
    TraversedNodeArray_Add(array, (TraversedNode) { .node_idx = 1 });
    TraversedNodeArray_Add(array, (TraversedNode) { .node_idx = 2 });
    TraversedNodeArray_Add(array, (TraversedNode) { .node_idx = 3 });
    ASSERT_HD(TraversedNodeArray_Size(array), 4);
    ASSERT_HD(TraversedNodeArray_Get(array, 5).node_idx, NODE_NULL);
    ASSERT_HD(TraversedNodeArray_Get(array, 0).node_idx, 0);
    ASSERT_HD(TraversedNodeArray_Get(array, 1).node_idx, 1);
    ASSERT_HD(TraversedNodeArray_Get(array, 2).node_idx, 2);
    ASSERT_HD(TraversedNodeArray_Get(array, 3).node_idx, 3);
    TraversedNodeArray_Add(array, (TraversedNode) { .node_idx = 2 });
    TraversedNodeArray_Add(array, (TraversedNode) { .node_idx = 2 });
    TraversedNodeArray_Add(array, (TraversedNode) { .node_idx = 2 });
    ASSERT_HD(TraversedNodeArray_FindFirstByNodeIdx(array, 2), 2);
    ASSERT_HD(TraversedNodeArray_FindFirstByNodeIdx(array, 4), -1);

    TraversedNodeArray_Free(&array);
    return 0;
}