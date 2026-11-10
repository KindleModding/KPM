#ifndef ARRAY_H
#define ARRAY_H

#include <stddef.h>
#include <stdlib.h>

#define create_array_h(type)                                                \
    typedef struct                                                          \
    {                                                                       \
        type *items;                                                        \
        size_t count;                                                       \
        size_t alloc;                                                       \
    } type##Array;                                                          \
                                                                            \
    type##Array *type##Array_Create();                                      \
    type type##Array_Get(type##Array *array, size_t index);                 \
    type type##Array_Pop(type##Array *array);                               \
    void type##Array_Truncate(type##Array *array, size_t size);             \
    void type##Array_Add(type##Array *array, type item);                    \
    void type##Array_Insert(type##Array *array, size_t index, type item);   \
    size_t type##Array_FindFirst(type##Array *array, type item);            \
    size_t type##Array_Size(type##Array *array);                            \
    void type##Array_Free(type##Array *array);                              \


#define create_array_c(type, type_null, eq)                                                                 \
    type##Array *type##Array_Create()                                                                       \
    {                                                                                                       \
        type##Array *array = malloc(sizeof(type##Array));                                                   \
        array->items = NULL;                                                                                \
        array->count = 0;                                                                                   \
        array->alloc = 0;                                                                                   \
        return array;                                                                                       \
    }                                                                                                       \
                                                                                                            \
    type type##Array_Get(type##Array *array, size_t index)                                                  \
    {                                                                                                       \
        if (index > array->count)                                                                           \
            return type_null;                                                                               \
        return array->items[index];                                                                         \
    }                                                                                                       \
                                                                                                            \
    type type##Array_Pop(type##Array *array)                                                                \
    {                                                                                                       \
        if (array->count == 0)                                                                              \
            return type_null;                                                                               \
        return array->items[--array->count];                                                                \
    }                                                                                                       \
                                                                                                            \
    void type##Array_Truncate(type##Array *array, size_t size)                                              \
    {                                                                                                       \
        array->alloc = size;                                                                                \
        array->items = realloc(array->items, sizeof(type) * array->alloc);                                  \
    }                                                                                                       \
                                                                                                            \
    void type##Array_Add(type##Array *array, type item)                                                     \
    {                                                                                                       \
        if (++array->count > array->alloc)                                                                  \
            array->items = realloc(array->items, sizeof(type) * (array->alloc = array->alloc * 1.5 + 1));   \
        array->items[array->count - 1] = item;                                                              \
    }                                                                                                       \
                                                                                                            \
    void type##Array_Insert(type##Array *array, size_t index, type item)                                    \
    {                                                                                                       \
        if (++array->count > array->alloc)                                                                  \
            array->alloc = array->alloc * 1.5 + 1;                                                          \
        array->items = realloc(array->items, array->alloc);                                                 \
        memcpy(array->items + index + 1, array->items + index, array->count - index);                       \
    }                                                                                                       \
                                                                                                            \
                                                                                                            \
    size_t type##Array_FindFirst(type##Array *array, type item)                                             \
    {                                                                                                       \
        for (int i=0; i < array->count; i++)                                                                \
            if (eq(array->items[i], item))                                                                  \
                return i;                                                                                   \
                                                                                                            \
        return -1;                                                                                          \
    }                                                                                                       \
                                                                                                            \
    size_t type##Array_Size(type##Array *array)                                                             \
    {                                                                                                       \
        return array->count;                                                                                \
    }                                                                                                       \
                                                                                                            \
    void type##Array_Free(type##Array *array)                                                               \
    {                                                                                                       \
        free(array->items);                                                                                 \
        array->count = 0;                                                                                   \
        array->alloc = 0;                                                                                   \
    }                                                                                                       \


#endif