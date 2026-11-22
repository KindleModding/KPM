#ifndef OPERATION_H
#define OPERATION_H

#include "dependencies.h"
#include "array.h"

typedef enum
{
    OP_INSTALL,            /**< Full download */
    OP_REINSTALL,
    OP_UPGRADE,
    OP_DOWNGRADE,
    OP_UNINSTALL
} Operation_Type;

typedef struct
{
    DepGraph_Target target;
    SemVer current_version;
    Operation_Type type;
} Operation;

create_array_h(Operation);
#define OperationArray_ForEachIndex(array, i, current) for(int i=0; i < OperationArray_Size(array) && (current = array->items[i], 1); i++)

#endif