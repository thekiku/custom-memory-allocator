#include "allocator.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef _WIN32
#error "this allocator is only supported on windows"
#endif

#include <windows.h>

/*
 * every allocation starts with one of these headers. the caller receives
 * the bytes immediately after the header, so the header can be found again
 * when my_free is called.
 */
typedef struct block {
    size_t size;
    int free;
    struct block *next;
    struct block *prev;
    max_align_t alignment;
} block_t;

/* all blocks are kept in address order so neighbouring blocks can be joined */
static block_t *first_block = NULL;
static block_t *last_block = NULL;

/* keep sizes aligned for every type, not only for char arrays */
static size_t align_size(size_t size)
{
    const size_t alignment = sizeof(max_align_t);
    const size_t remainder = size % alignment;

    if (remainder == 0) {
        return size;
    }

    return size + alignment - remainder;
}

/* reject sizes that would wrap around while adding allocator metadata */
static int prepare_size(size_t requested, size_t *size)
{
    if (requested == 0 || requested > SIZE_MAX - (sizeof(max_align_t) - 1)) {
        return 0;
    }

    *size = align_size(requested);
    return *size <= SIZE_MAX - sizeof(block_t);
}

/* ask windows for a private region of readable and writable memory */
static void *request_space(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

/* find a reusable block with enough space */
static block_t *find_free_block(size_t size)
{
    block_t *current = first_block;

    while (current != NULL) {
        if (current->free && current->size >= size) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

/* add a newly obtained block to the end of the address-ordered list */
static void add_block(block_t *block)
{
    block->prev = last_block;
    block->next = NULL;
    block->free = 0;

    if (last_block != NULL) {
        last_block->next = block;
    } else {
        first_block = block;
    }

    last_block = block;
}

/* split a large free block when the remainder can hold another block */
static void split_block(block_t *block, size_t size)
{
    block_t *new_block;
    size_t remaining = block->size - size;

    if (remaining < sizeof(block_t) + sizeof(max_align_t)) {
        return;
    }

    new_block = (block_t *)((char *)(block + 1) + size);
    new_block->size = remaining - sizeof(block_t);
    new_block->free = 1;
    new_block->prev = block;
    new_block->next = block->next;

    if (new_block->next != NULL) {
        new_block->next->prev = new_block;
    } else {
        last_block = new_block;
    }

    block->next = new_block;
    block->size = size;
}

/* two blocks are neighbours when the second starts after the first payload */
static int are_adjacent(block_t *first, block_t *second)
{
    return (char *)(first + 1) + first->size == (char *)second;
}

/* merge a free block with the free block directly after it */
static void merge_with_next(block_t *block)
{
    block_t *next = block->next;

    if (next == NULL || !next->free || !are_adjacent(block, next)) {
        return;
    }

    block->size += sizeof(block_t) + next->size;
    block->next = next->next;

    if (block->next != NULL) {
        block->next->prev = block;
    } else {
        last_block = block;
    }
}

/* join free neighbours around a block after it is released */
static void coalesce(block_t *block)
{
    if (block->next != NULL && block->next->free) {
        merge_with_next(block);
    }

    if (block->prev != NULL && block->prev->free) {
        merge_with_next(block->prev);
    }
}

/* return the header only when ptr is one of our allocation payloads */
static block_t *block_from_pointer(void *ptr)
{
    block_t *current = first_block;

    while (current != NULL) {
        if ((void *)(current + 1) == ptr) {
            return current;
        }
        current = current->next;
    }

    return NULL;
}

void *my_malloc(size_t size)
{
    block_t *block;
    size_t aligned_size;

    if (!prepare_size(size, &aligned_size)) {
        return NULL;
    }

    block = find_free_block(aligned_size);
    if (block != NULL) {
        split_block(block, aligned_size);
        block->free = 0;
        return (void *)(block + 1);
    }

    block = (block_t *)request_space(sizeof(block_t) + aligned_size);
    if (block == NULL) {
        return NULL;
    }

    block->size = aligned_size;
    add_block(block);
    return (void *)(block + 1);
}

void my_free(void *ptr)
{
    block_t *block;

    if (ptr == NULL) {
        return;
    }

    block = block_from_pointer(ptr);
    if (block == NULL || block->free) {
        return;
    }

    block->free = 1;
    coalesce(block);
}

void *my_calloc(size_t count, size_t size)
{
    void *memory;

    if (count != 0 && size > SIZE_MAX / count) {
        return NULL;
    }

    memory = my_malloc(count * size);
    if (memory != NULL) {
        memset(memory, 0, count * size);
    }

    return memory;
}

void *my_realloc(void *ptr, size_t size)
{
    block_t *block;
    void *new_memory;
    size_t copy_size;

    if (ptr == NULL) {
        return my_malloc(size);
    }
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    block = block_from_pointer(ptr);
    if (block == NULL || block->free) {
        return NULL;
    }

    if (block->size >= size) {
        size_t aligned_size;

        if (prepare_size(size, &aligned_size)) {
            split_block(block, aligned_size);
        }
        return ptr;
    }

    new_memory = my_malloc(size);
    if (new_memory == NULL) {
        return NULL;
    }

    copy_size = block->size < size ? block->size : size;
    memcpy(new_memory, ptr, copy_size);
    my_free(ptr);
    return new_memory;
}
