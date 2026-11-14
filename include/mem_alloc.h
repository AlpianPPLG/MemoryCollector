#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mem_alloc.h
 * @brief Subsystem allocators: Bump Allocator (Eden/Nursery) and Free-List Allocator (Tenured).
 */

/* ========================================================================= */
/* Bump Allocator (Linear O(1) Allocations for Fast Nursery)                */
/* ========================================================================= */

typedef struct {
    uint8_t* memory;
    size_t capacity;
    size_t offset;
    size_t high_watermark;
} bump_alloc_t;

bool bump_alloc_init(bump_alloc_t* alloc, void* raw_memory, size_t capacity);
void* bump_alloc(bump_alloc_t* alloc, size_t size, size_t alignment);
void bump_alloc_reset(bump_alloc_t* alloc);
size_t bump_alloc_remaining(const bump_alloc_t* alloc);

/* ========================================================================= */
/* Segregated Free-List Allocator (For Tenured / Long-Lived Objects)         */
/* ========================================================================= */

typedef struct fl_node {
    size_t size;               // Capacity of this free chunk
    struct fl_node* next;      // Next free chunk in linked list
} fl_node_t;

typedef struct {
    uint8_t* memory;
    size_t capacity;
    fl_node_t* head;           // Head of free blocks list
    size_t bytes_allocated;
} freelist_alloc_t;

bool freelist_alloc_init(freelist_alloc_t* fl, void* raw_memory, size_t capacity);
void* freelist_alloc(freelist_alloc_t* fl, size_t size, size_t alignment, size_t* out_allocated_size);
void freelist_free(freelist_alloc_t* fl, void* ptr, size_t size);

#ifdef __cplusplus
}
#endif
