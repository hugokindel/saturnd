#ifndef ARRAY_H
#define ARRAY_H

#include <sy5/types.h>

// An array is a pointer of elements (like a C array) that is prefixed by its size (a `uint64_t`) in memory.
//
// Example:
// ```
// char **array = NULL
// char *element1 = "Hello, ";
// array_push(array, element1);
// char *element2 = "World!";
// array_push(array, element2);
// ```
// Here's how it would look inside the memory:
// |2|"hello, "|"world!"|
//  ↑ ↑         ↑
//  │ │         └────────────────────────────────────────────────────┐
//  │ └────────────────────┐                                         │
//  The size of the array. │                                         │
//                         The first element (where `array` points). │
//                                                                   The second element.
//
// This array concept was inspired by stb's stretchy buffer (https://github.com/nothings/stb/blob/master/deprecated/stretchy_buffer.txt).

// Returns the number of element in an array (or 0 if the array is `NULL`).
#define array_size(array) ((array) ? *(uint64_t *)((uint8_t *)(array) - sizeof(uint64_t)) : 0)

// Returns the first element in an array (assumes that the array has at least 1 element).
#define array_first(array) ((array)[0])

// Returns the last element in an array (assumes that the array has at least 1 element).
#define array_last(array) ((array)[array_size(array) - 1])

// Returns `true` if the array is empty or `NULL`.
#define array_empty(array) (array_size(array) == 0)

// Pushes an element in the array (which can be `NULL` if empty).
#define array_push(array, item) array_push_internal((void **)&(array), &(item), sizeof(item))

// Pops the last element of the array (assumes that the array has at least 1 element).
#define array_pop(array) array_pop_internal((void **)&(array), sizeof((array)[0]))

// Removes the element at a given index in the array (assumes that the array has at least 1 element).
#define array_remove(array, index) array_remove_internal((void **)&(array), (index), sizeof((array)[0]))

// Frees the array (it can be `NULL`).
#define array_free(array) array_free_internal((void **)&(array))

// Internal method to push an element in the array.
static inline int array_push_internal(void **array, void *item, uint32_t item_size) {
    size_t size = sizeof(uint64_t) + item_size * array_size(*array);
    void *tmp = realloc(*array ? *array - sizeof(uint64_t) : 0, size + item_size);
    assert(tmp);
    
    if (*array == NULL) {
        *(uint64_t *)tmp = 0;
    }
    
    void *end_ptr = tmp + size;
    assert(memcpy(end_ptr, item, item_size) != NULL);
    *(uint64_t *)tmp += 1;
    *array = tmp + sizeof(uint64_t);
    
    return 0;
}

// Internal method to pop an element in the array.
static inline int array_pop_internal(void **array, uint32_t item_size) {
    size_t size = sizeof(uint64_t) + item_size * array_size(*array);
    void *tmp = realloc(*array - sizeof(uint64_t), size - item_size);
    assert(tmp);
    
    *(uint64_t *)tmp -= 1;
    *array = tmp + sizeof(uint64_t);
    
    return 0;
}

// Internal method to remove an element from the array.
static inline int array_remove_internal(void **array, uint64_t index, uint32_t item_size) {
    uint64_t count = array_size(*array);
    assert(memmove(*array + item_size * index, *array + item_size * index + item_size, (count - 1 - index) * item_size) != NULL);
    return array_pop_internal(array, item_size);
}

// Internal method to free the array.
static inline void array_free_internal(void **array) {
    if (*array == NULL) {
        return;
    }
    
    free(*array - sizeof(uint64_t));
    *array = NULL;
}

#endif /* ARRAY_H. */