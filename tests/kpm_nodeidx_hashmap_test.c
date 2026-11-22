#include "dependencies.h"
#include "kpm_test_common.h"

int main(int argc, char* argv[])
{
    NodeIdxHashmap *hashmap = NodeIdxHashmap_Create(4);
    NodeIdxHashmap_Remove(hashmap, "210");
    NodeIdxArray *array;
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "hello", NULL), false);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "hello", &array), false);
    NodeIdxHashmap_Free(&hashmap);

    hashmap = NodeIdxHashmap_Create(4);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "hello", NULL), false);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "hello", &array), false);

    NodeIdxHashmap_AddNode(hashmap, "1", 1);
    NodeIdxHashmap_AddNode(hashmap, "2", 2);
    NodeIdxHashmap_AddNode(hashmap, "3", 3);
    NodeIdxHashmap_AddNode(hashmap, "4", 4);

    NodeIdxHashmap_Remove(hashmap, "210");

    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", &array), true);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 1);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "2", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "2", &array), true);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 2);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "3", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "3", &array), true);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 3);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "4", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "4", &array), true);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 4);

    NodeIdxHashmap_AddNode(hashmap, "1", 2);
    NodeIdxHashmap_AddNode(hashmap, "1", 3);
    NodeIdxHashmap_AddNode(hashmap, "1", 4);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 1);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 2);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 3);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 4);

    NodeIdxHashmap_Remove(hashmap, "1");
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", NULL), false);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", &array), false);
    NodeIdxHashmap_Remove(hashmap, "2");
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "2", NULL), false);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "2", &array), false);
    NodeIdxHashmap_Remove(hashmap, "3");
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "3", NULL), false);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "3", &array), false);
    NodeIdxHashmap_Remove(hashmap, "4");
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "4", NULL), false);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "4", &array), false);
    
    NodeIdxHashmap_Remove(hashmap, "210");

    NodeIdxHashmap_AddNode(hashmap, "1", 1);
    NodeIdxHashmap_AddNode(hashmap, "1", 2);
    NodeIdxHashmap_AddNode(hashmap, "1", 3);
    NodeIdxHashmap_AddNode(hashmap, "1", 4);
    NodeIdxHashmap_AddNode(hashmap, "2", 5);
    NodeIdxHashmap_AddNode(hashmap, "2", 6);
    NodeIdxHashmap_AddNode(hashmap, "2", 7);
    NodeIdxHashmap_AddNode(hashmap, "2", 8);
    NodeIdxHashmap_AddNode(hashmap, "3", 9);
    NodeIdxHashmap_AddNode(hashmap, "3", 10);
    NodeIdxHashmap_AddNode(hashmap, "3", 11);
    NodeIdxHashmap_AddNode(hashmap, "3", 12);
    NodeIdxHashmap_AddNode(hashmap, "4", 13);
    NodeIdxHashmap_AddNode(hashmap, "4", 14);
    NodeIdxHashmap_AddNode(hashmap, "4", 15);
    NodeIdxHashmap_AddNode(hashmap, "4", 16);
    NodeIdxHashmap_AddNode(hashmap, "5", 17);
    NodeIdxHashmap_AddNode(hashmap, "5", 18);
    NodeIdxHashmap_AddNode(hashmap, "5", 19);
    NodeIdxHashmap_AddNode(hashmap, "5", 20);
    NodeIdxHashmap_AddNode(hashmap, "6", 21);
    NodeIdxHashmap_AddNode(hashmap, "6", 22);
    NodeIdxHashmap_AddNode(hashmap, "6", 23);
    NodeIdxHashmap_AddNode(hashmap, "6", 24);
    NodeIdxHashmap_AddNode(hashmap, "7", 25);
    NodeIdxHashmap_AddNode(hashmap, "7", 26);
    NodeIdxHashmap_AddNode(hashmap, "7", 27);
    NodeIdxHashmap_AddNode(hashmap, "7", 29);
    NodeIdxHashmap_AddNode(hashmap, "8", 30);
    NodeIdxHashmap_AddNode(hashmap, "8", 31);
    NodeIdxHashmap_AddNode(hashmap, "8", 32);
    NodeIdxHashmap_AddNode(hashmap, "8", 33);


    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 1);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 2);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 3);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 4);

    
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "1", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 1);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 2);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 3);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 4);
    
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "2", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "2", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 5);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 6);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 7);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 8);
    
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "3", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "3", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 9);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 10);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 11);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 12);
    
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "4", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "4", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 13);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 14);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 15);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 16);
    
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "5", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "5", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 17);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 18);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 19);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 20);
    
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "6", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "6", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 21);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 22);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 23);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 24);
    
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "7", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "7", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 25);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 26);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 27);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 29);
    
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "8", NULL), true);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "8", &array), true);
    ASSERT_HD(NodeIdxArray_Size(array), 4);
    ASSERT_HD(NodeIdxArray_Get(array, 0), 30);
    ASSERT_HD(NodeIdxArray_Get(array, 1), 31);
    ASSERT_HD(NodeIdxArray_Get(array, 2), 32);
    ASSERT_HD(NodeIdxArray_Get(array, 3), 33);

    NodeIdxHashmap_AddNode(hashmap, "8", 1);
    NodeIdxHashmap_AddNode(hashmap, "9", 1);
    NodeIdxHashmap_AddNode(hashmap, "10", 1);
    NodeIdxHashmap_AddNode(hashmap, "11", 1);
    NodeIdxHashmap_AddNode(hashmap, "12", 1);
    NodeIdxHashmap_AddNode(hashmap, "13", 1);
    NodeIdxHashmap_AddNode(hashmap, "14", 1);
    NodeIdxHashmap_AddNode(hashmap, "15", 1);
    NodeIdxHashmap_AddNode(hashmap, "16", 1);
    NodeIdxHashmap_AddNode(hashmap, "17", 1);
    NodeIdxHashmap_AddNode(hashmap, "18", 1);
    NodeIdxHashmap_AddNode(hashmap, "19", 1);
    NodeIdxHashmap_AddNode(hashmap, "20", 1);
    NodeIdxHashmap_AddNode(hashmap, "21", 1);

    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "99", NULL), false);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "99", &array), false);
    NodeIdxHashmap_Remove(hashmap, "8");
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "8", NULL), false);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "8", &array), false);

    NodeIdxHashmap_Remove(hashmap, "21");
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "21", NULL), false);
    ASSERT_HD(NodeIdxHashmap_Get(hashmap, "21", &array), false);

    NodeIdxHashmap_Remove(hashmap, "210");


    NodeIdxHashmap_Free(&hashmap);

    return 0;
}