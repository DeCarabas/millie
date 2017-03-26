#ifndef PLATFORM_INCLUDED
#include "platform.h"
#endif

/*
 * Assertion and failure
 */
void Fail(char *message)
{
    assert(0 && message);
    abort();
}

/*
 * Memory Allocation
 */
const size_t ARENA_SIZE = 2 * 1024 * 1024; // 2MB?
struct ArenaBlock
{
    char *start;
    char arena[ARENA_SIZE];
    struct ArenaBlock *next;
};

struct Arena
{
    struct ArenaBlock *current;
};

struct Arena *
MakeFreshArena()
{
    return calloc(1, sizeof(struct Arena));
}

void
FreeArena(struct Arena **arena)
{
    if (*arena) {
        struct ArenaBlock *block = (*arena)->current;
        while(block) {
            struct ArenaBlock *next = block->next;
            free(block);
            block = next;
        }
        free(*arena);
    }
    *arena = NULL;
}

void *
ArenaAllocate(struct Arena *arena, size_t size)
{
    if (size > ARENA_SIZE) {
        Fail("Allocation too big.");
    }
    struct ArenaBlock *current_block = arena->current;
    ptrdiff_t space_remaining = current_block->start - current_block->arena;
    if ((space_remaining < 0) || (size > (size_t)space_remaining)) {
        struct ArenaBlock *new_block = calloc(1, sizeof(struct ArenaBlock));
        new_block->next = current_block;
        new_block->start = new_block->arena;
        arena->current = new_block;
        current_block = new_block;
    }

    void *result = current_block->start;
    uintptr_t new_start = (uintptr_t)(current_block->start + size);
    if (new_start & 7) { new_start += 8 - (new_start & 7); }
    current_block->start = (char *)new_start;
    return result;
}

struct ArrayList *
CreateArrayList(unsigned int item_size, unsigned int capacity)
{
    if (capacity == 0) { capacity = 4; }
    struct ArrayList *result = calloc(1, sizeof(struct ArrayList));
    result->item_size = item_size;
    result->capacity = capacity;
    result->buffer = malloc(item_size * capacity);

    return result;
}

void
FreeArrayList(struct ArrayList **array_ptr)
{
    struct ArrayList *array = *array_ptr;
    *array_ptr = NULL;

    if (!array) { return; }
    free(array->buffer);
    free(array);
}


void *
ArrayListIndex(struct ArrayList *array, unsigned int index)
{
    if (index >= array->item_count) {
        Fail("Attempt to access outside array bounds");
    }
    return ((char *)array->buffer) + (index * array->item_size);
}

int
ArrayListAdd(struct ArrayList *array, void *item)
{
    if (array->item_count == array->capacity) {
        array->capacity *= 2;
        size_t new_size = array->capacity * array->item_size;
        array->buffer = realloc(array->buffer, new_size);
    }

    int new_index = array->item_count;
    array->item_count += 1;
    void *item_ptr = ArrayListIndex(array, new_index);
    memcpy(item_ptr, item, array->item_size);
    return new_index;
}
