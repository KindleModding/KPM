#include "dependencies.h"
#include "kpm_test_common.h"

int main(int argc, char* argv[])
{
    NodeIdxArray *array = NodeIdxArray_Create();
    ASSERT_HD(NodeIdxArray_Size(array), 0);
    ASSERT_HD(NodeIdxArray_Get(array, -1), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Get(array, 0), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Get(array, 1), NODE_NULL);

    ASSERT_HD(NodeIdxArray_FindFirst(array, 0), -1);
    NodeIdxArray_Add(array, 0);
    NodeIdxArray_Add(array, 1);
    NodeIdxArray_Add(array, 2);
    NodeIdxArray_Add(array, 3);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 5), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 0);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 1);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 2);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 3);

    ASSERT_HD(NodeIdxArray_Pop(array), 3);
    ASSERT_HD(NodeIdxArray_Pop(array), 2);
    ASSERT_HD(NodeIdxArray_Pop(array), 1);
    ASSERT_HD(NodeIdxArray_Pop(array), 0);
    ASSERT_HD(NodeIdxArray_Pop(array), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Pop(array), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Pop(array), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Size(array), 0);

    NodeIdxArray_Insert(array, 0, 3);
    NodeIdxArray_Insert(array, 0, 2);
    NodeIdxArray_Insert(array, 0, 1);
    NodeIdxArray_Insert(array, 0, 0);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 0);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 1);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 2);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 3);
    ASSERT_HD(NodeIdxArray_Get(array, 4), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Pop(array), 3);
    ASSERT_HD(NodeIdxArray_Pop(array), 2);
    ASSERT_HD(NodeIdxArray_Pop(array), 1);
    ASSERT_HD(NodeIdxArray_Pop(array), 0);
    ASSERT_HD(NodeIdxArray_Pop(array), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Size(array), 0);

    NodeIdxArray_Add(array, 0);
    NodeIdxArray_Add(array, 1);
    NodeIdxArray_Add(array, 2);
    NodeIdxArray_Add(array, 3);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 0), 0);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 1), 1);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 2), 2);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 3), 3);
    NodeIdxArray_Add(array, 0);
    NodeIdxArray_Add(array, 1);
    NodeIdxArray_Add(array, 2);
    NodeIdxArray_Add(array, 3);
    ASSERT_HD(NodeIdxArray_Size(array), 8);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 0), 0);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 1), 1);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 2), 2);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 3), 3);

    NodeIdxArray_Truncate(array, 1);
    ASSERT_HD(NodeIdxArray_Size(array), 1);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 0), 0);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 1), -1);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 2), -1);
    ASSERT_HD(NodeIdxArray_FindFirst(array, 3), -1);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 0);
    ASSERT_HD(NodeIdxArray_Get(array, 1), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Get(array, 2), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Get(array, 3), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Pop(array), 0);
    ASSERT_HD(NodeIdxArray_Pop(array), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Size(array), 0);

    NodeIdxArray_Add(array, 0);
    NodeIdxArray_Add(array, 1);
    NodeIdxArray_Add(array, 2);
    NodeIdxArray_Add(array, 3);

    NodeIdxArray_Free(&array);

    array = NodeIdxArray_Create();
    ASSERT_HD(NodeIdxArray_FindFirst(array, 0), -1);
    NodeIdxArray_Insert(array, 0, 0);
    NodeIdxArray_Insert(array, 0, 1);
    NodeIdxArray_Insert(array, 0, 2);
    NodeIdxArray_Insert(array, 0, 3);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 5), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 0);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 1);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 2);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 3);

    ASSERT_HD(NodeIdxArray_Pop(array), 0);
    ASSERT_HD(NodeIdxArray_Pop(array), 1);
    ASSERT_HD(NodeIdxArray_Pop(array), 2);
    ASSERT_HD(NodeIdxArray_Pop(array), 3);
    ASSERT_HD(NodeIdxArray_Pop(array), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Pop(array), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Pop(array), NODE_NULL);
    ASSERT_HD(NodeIdxArray_Size(array), 0);
    NodeIdxArray_Free(&array);

    return 0;
}