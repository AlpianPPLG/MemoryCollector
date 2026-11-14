#pragma once

#include "mem_pal.h"
#include "mem_object.h"
#include "mem_alloc.h"
#include "mem_roots.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mem_collector.h
 * @brief Core Garbage Collection Engine with Generational Promotion, LISP-2 Compactor, and Tri-Color Tracing.
 */

typedef struct {
    uint64_t total_allocations;
    uint64_t total_bytes_allocated;
    uint64_t minor_gc_count;
    uint64_t major_gc_count;
    uint64_t total_bytes_reclaimed;
    uint64_t total_objects_reclaimed;
    uint64_t total_objects_promoted;
    uint64_t total_bytes_promoted;
    uint64_t compaction_count;
    size_t   current_heap_usage;
} gc_stats_t;

typedef struct {
    // Virtual memory arenas
    void* nursery_raw;
    size_t nursery_size;
    bump_alloc_t nursery_alloc;

    void* tenured_raw;
    size_t tenured_size;
    freelist_alloc_t tenured_alloc;

    // Tracking list of all managed objects
    gc_header_t* all_objects;

    // Roots
    roots_registry_t roots;

    // Metrics & heuristics
    gc_stats_t stats;
    uint8_t tenuring_threshold; // Number of Minor GCs an object survives before promotion
    bool initialized;
} gc_engine_t;

// Engine lifecycle
bool gc_init(size_t nursery_bytes, size_t tenured_bytes);
void gc_shutdown(void);
void gc_set_tenuring_threshold(uint8_t threshold);

/**
 * @brief Allocate a managed object of the requested payload size.
 * @param size Byte size of user payload.
 * @param trace Optional pointer to function describing outgoing pointers (or NULL).
 * @return void* Pointer to the user payload, or NULL if out of memory.
 */
void* gc_alloc(size_t size, gc_trace_fn trace);

/**
 * @brief Register a variable slot as a GC root.
 */
bool gc_root_register(void** slot);

/**
 * @brief Unregister a variable slot as a GC root.
 */
bool gc_root_unregister(void** slot);

/**
 * @brief Run a Minor Collection (scavenges nursery, promotes survivors, and resets nursery).
 */
void gc_collect_minor(void);

/**
 * @brief Run a Major Collection (full tri-color tracing across all generations).
 */
void gc_collect_major(void);

/**
 * @brief Convenience trigger: decides between minor or major collection based on pressure.
 */
void gc_collect(void);

/**
 * @brief Defragment Tenured heap using LISP-2 Compaction algorithm.
 * Moves all live objects in Tenured space to a contiguous block, eliminating fragmentation.
 */
void gc_compact(void);

/**
 * @brief Retrieve current metrics.
 */
gc_stats_t gc_get_stats(void);

/**
 * @brief Pretty-print GC metrics and memory status to stdout.
 */
void gc_dump_stats(void);

#ifdef __cplusplus
}
#endif
