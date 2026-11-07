#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <stddef.h>

//allocate size bytes, or return null when the request cannot be made 
void *my_malloc(size_t size);

//release a block returned by my_malloc or my_realloc 
void my_free(void *ptr);

//allocate count zeroed elements 
void *my_calloc(size_t count, size_t size);

//resize a block while preserving its existing contents 
void *my_realloc(void *ptr, size_t size);

#endif
