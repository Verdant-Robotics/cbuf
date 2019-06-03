#pragma once
#include "mytypes.h"
#include <stddef.h> // for size_t

class Allocator
{
public:
        Allocator(size_t start_size = 0) {}
        virtual ~Allocator() {}
        virtual void* alloc(size_t size) = 0;
        virtual void free(void* p) = 0;
};

class PoolAllocator : public Allocator
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
    void * alloc(size_t size) override;
        void free(void* p) override;
    bool isAddressInRange(void *p);
};

void * operator new (size_t size, Allocator *p);

class MallocAllocator : public Allocator
{
public:
        MallocAllocator() {}
        ~MallocAllocator() {}

        void* alloc(size_t size) override;
        void free(void* p) override;
};

MallocAllocator* getMallocAllocator();

