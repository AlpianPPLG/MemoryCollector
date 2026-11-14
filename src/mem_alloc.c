#include "../include/mem_alloc.h"
#include <string.h>

/* ========================================================================= */
/* Bump Allocator Implementation                                             */
/* ========================================================================= */

bool bump_alloc_init(bump_alloc_t* alloc, void* raw_memory, size_t capacity) {
    if (!alloc || !raw_memory || capacity == 0) return false;
    alloc->memory = (uint8_t*)raw_memory;
    alloc->capacity = capacity;
    alloc->offset = 0;
    alloc->high_watermark = 0;
    return true;
}

void* bump_alloc(bump_alloc_t* alloc, size_t size, size_t alignment) {
    if (!alloc || size == 0) return NULL;
    if (alignment == 0) alignment = sizeof(void*);

    // Calculate current address and aligned position
    uintptr_t current = (uintptr_t)(alloc->memory + alloc->offset);
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t padding = (size_t)(aligned - current);

    if (alloc->offset + padding + size > alloc->capacity) {
        return NULL; // Out of memory in this bump chunk
    }

    alloc->offset += (padding + size);
    if (alloc->offset > alloc->high_watermark) {
        alloc->high_watermark = alloc->offset;
    }

    return (void*)aligned;
}

void bump_alloc_reset(bump_alloc_t* alloc) {
    if (alloc) {
        alloc->offset = 0;
    }
}

size_t bump_alloc_remaining(const bump_alloc_t* alloc) {
    if (!alloc) return 0;
    return (alloc->offset < alloc->capacity) ? (alloc->capacity - alloc->offset) : 0;
}

/* ========================================================================= */
/* Free-List Allocator Implementation                                        */
/* ========================================================================= */

bool freelist_alloc_init(freelist_alloc_t* fl, void* raw_memory, size_t capacity) {
    if (!fl || !raw_memory || capacity < sizeof(fl_node_t)) return false;

    fl->memory = (uint8_t*)raw_memory;
    fl->capacity = capacity;
    fl->bytes_allocated = 0;

    fl->head = (fl_node_t*)raw_memory;
    fl->head->size = capacity;
    fl->head->next = NULL;

    return true;
}

void* freelist_alloc(freelist_alloc_t* fl, size_t size, size_t alignment, size_t* out_allocated_size) {
    if (!fl || size == 0) return NULL;
    if (alignment == 0) alignment = sizeof(void*);

    // Minimal size must at least hold a free list node
    size_t actual_size = size < sizeof(fl_node_t) ? sizeof(fl_node_t) : size;
    actual_size = (actual_size + alignment - 1) & ~(alignment - 1);

    fl_node_t* prev = NULL;
    fl_node_t* curr = fl->head;

    while (curr) {
        if (curr->size >= actual_size) {
            size_t final_size = actual_size;

            // Check if we can split the chunk
            if (curr->size >= actual_size + sizeof(fl_node_t) + alignment) {
                fl_node_t* next_chunk = (fl_node_t*)((uint8_t*)curr + actual_size);
                next_chunk->size = curr->size - actual_size;
                next_chunk->next = curr->next;

                if (prev) {
                    prev->next = next_chunk;
                } else {
                    fl->head = next_chunk;
                }
            } else {
                // Entire chunk used to prevent tiny unusable slivers
                final_size = curr->size;
                if (prev) {
                    prev->next = curr->next;
                } else {
                    fl->head = curr->next;
                }
            }

            fl->bytes_allocated += final_size;
            if (out_allocated_size) {
                *out_allocated_size = final_size;
            }
            return (void*)curr;
        }

        prev = curr;
        curr = curr->next;
    }

    return NULL; // Out of memory in free list
}

void freelist_free(freelist_alloc_t* fl, void* ptr, size_t size) {
    if (!fl || !ptr || size == 0) return;

    size_t actual_size = size < sizeof(fl_node_t) ? sizeof(fl_node_t) : size;

    fl_node_t* released = (fl_node_t*)ptr;
    released->size = actual_size;

    // Insert back sorted by address to allow coalescing
    fl_node_t* prev = NULL;
    fl_node_t* curr = fl->head;

    while (curr && curr < released) {
        prev = curr;
        curr = curr->next;
    }

    // Double-free guard
    if (curr == released) {
        return;
    }

    if (prev) {
        prev->next = released;
    } else {
        fl->head = released;
    }
    released->next = curr;

    if (fl->bytes_allocated >= actual_size) {
        fl->bytes_allocated -= actual_size;
    } else {
        fl->bytes_allocated = 0;
    }

    // Coalesce with next chunk if adjacent
    if (curr && ((uint8_t*)released + released->size == (uint8_t*)curr)) {
        released->size += curr->size;
        released->next = curr->next;
    }

    // Coalesce with previous chunk if adjacent
    if (prev && ((uint8_t*)prev + prev->size == (uint8_t*)released)) {
        prev->size += released->size;
        prev->next = released->next;
    }
}
