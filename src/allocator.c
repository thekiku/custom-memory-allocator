#include <stdio.h>
#include <unistd.h>
#include <stddef.h>
//decribes the structure of a block of memory
//metadata part for each block of memory
typedef struct block{
    size_t size;
    int free;// 1 or 0 (space available or not)
    struct block *next;
    struct block *prev;
}block_t; //alias

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

static void add_to_free_list(block_t *block){
    block->free =1;

    block->next = free_list;
    block->prev = NULL;
    if(free_list != NULL)
        free_list->prev = block;
    
    free_list = block;
}

void *my_malloc(size_t size){
    if(size == 0){
        return NULL;
    }
    block_t *block = find_free_block(size);
    if(block != NULL){
        block->free = 0;
        return (void *)(block + 1);
    }

    block = request_space(sizeof(block_t) + size);
    if(block == NULL)
        return NULL;
    
    block -> size = size;
    block -> free = 0;
    block -> next = NULL;
    
}


