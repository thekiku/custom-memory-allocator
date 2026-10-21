#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
//decribes the structure of a block of memory
typedef struct block{
    size_t size;
    int free;

    struct block *next;
    struct block *prev;
}block_t;
//global variable to keep track of the free list
static block_t *free_list = NULL;
//provides memory allocation
static void *request_space(size_t size){
    void *p = sbrk(size);
    if(p==(void *)-1)
        return NULL;
    
    return p;
}

static block_t *find_free_block(size_t size){
    block_t *current = free_list;
    while(current != NULL){
        if(current->free && current->size >= size)
            return current;
        current = current->next;
    }
    return NULL;
}
