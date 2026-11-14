#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mem_object.h
 * @brief Managed object header, layout, color flags, and field tracing callback types.
 */

// Tri-color marking status
typedef enum {
    GC_COLOR_WHITE = 0, // Unvisited / garbage candidate
    GC_COLOR_GREY  = 1, // Visited, pending exploration of outgoing references
    GC_COLOR_BLACK = 2  // Fully visited and live
} gc_color_t;

// Generation tagging
typedef enum {
    GC_GEN_NURSERY = 0, // Young generation (short-lived)
    GC_GEN_TENURED = 1  // Old generation (long-lived / promoted)
} gc_gen_t;

/**
 * @brief Visitor function called by an object's trace function for each nested pointer.
 * @param field_ptr Pointer to the field storing the reference (void**).
 * @param ctx Arbitrary context pointer passed down from tracer.
 */
typedef void (*gc_field_visitor_fn)(void** field_ptr, void* ctx);

/**
 * @brief User-defined trace function to discover nested references inside an object.
 * @param payload Pointer to the user object payload.
 * @param visitor Visitor function to call for each contained reference.
 * @param ctx Opaque context passed to the visitor.
 */
typedef void (*gc_trace_fn)(void* payload, gc_field_visitor_fn visitor, void* ctx);

/**
 * @brief Object Header placed directly before every managed memory block.
 */
typedef struct gc_header {
    uint32_t payload_size;       // Byte size of user payload
    uint32_t allocated_size;     // Exact byte size of block allocated in heap (including header)
    uint8_t  color;              // gc_color_t (Tri-color mark)
    uint8_t  generation;         // gc_gen_t (NURSERY or TENURED)
    uint8_t  age;                // Collection survival count
    uint8_t  flags;              // Extra bitflags
    gc_trace_fn trace;           // Tracing function pointer (or NULL for raw data)
    struct gc_header* next;      // Intrusive linked list pointer for heap tracking
    void* forwarding_address;    // Pointer to relocated object during Promotion or Compaction
} gc_header_t;

#define GC_HEADER_SIZE (sizeof(gc_header_t))

// Helper conversions between payload and header
static inline gc_header_t* gc_payload_to_header(void* payload) {
    if (!payload) return NULL;
    return (gc_header_t*)((uint8_t*)payload - GC_HEADER_SIZE);
}

static inline void* gc_header_to_payload(gc_header_t* header) {
    if (!header) return NULL;
    return (void*)((uint8_t*)header + GC_HEADER_SIZE);
}

#ifdef __cplusplus
}
#endif
