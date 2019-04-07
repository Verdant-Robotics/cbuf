#pragma once
#include "mytypes.h"
#include <stddef.h> // for size_t

class PoolAllocator
{
    struct block {
        size_t free_size;
        struct block *next;
        u8 *start_address;
        u8 *free_address;
    };
    block root_block;
    size_t total_size;
    size_t block_size;

    void allocateBlock(block *b);
    void * allocateFromBlock(block *b, size_t size);
public:
    PoolAllocator(size_t start_size = 0);
    ~PoolAllocator();
    void * alloc(size_t size);
    bool isAddressInRange(void *p);
};

void * operator new ( size_t size, PoolAllocator *p);
