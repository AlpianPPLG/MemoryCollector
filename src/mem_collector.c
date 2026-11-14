#include "../include/mem_collector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static gc_engine_t g_engine = {0};

#define DEFAULT_TENURING_THRESHOLD 1
#define INITIAL_GREY_CAPACITY 512

// Dynamically expandable worklist for Tri-Color Marking
typedef struct {
    gc_header_t** items;
    size_t count;
    size_t capacity;
} grey_worklist_t;

static grey_worklist_t g_grey_worklist = { .items = NULL, .count = 0, .capacity = 0 };

static bool grey_worklist_push(gc_header_t* hdr) {
    if (!hdr) return false;

    if (g_grey_worklist.count >= g_grey_worklist.capacity) {
        size_t new_cap = g_grey_worklist.capacity == 0 ? INITIAL_GREY_CAPACITY : g_grey_worklist.capacity * 2;
        gc_header_t** new_items = (gc_header_t**)realloc(g_grey_worklist.items, new_cap * sizeof(gc_header_t*));
        if (!new_items) {
            fprintf(stderr, "[GC Error] Out of memory expanding GC grey worklist!\n");
            return false;
        }
        g_grey_worklist.items = new_items;
        g_grey_worklist.capacity = new_cap;
    }

    g_grey_worklist.items[g_grey_worklist.count++] = hdr;
    return true;
}

static gc_header_t* grey_worklist_pop(void) {
    if (g_grey_worklist.count == 0) return NULL;
    return g_grey_worklist.items[--g_grey_worklist.count];
}

static void grey_worklist_cleanup(void) {
    if (g_grey_worklist.items) {
        free(g_grey_worklist.items);
        g_grey_worklist.items = NULL;
    }
    g_grey_worklist.count = 0;
    g_grey_worklist.capacity = 0;
}

// Safety check: validates if a pointer falls within the managed heap arenas
static bool is_managed_payload(void* ptr) {
    if (!ptr) return false;
    uintptr_t p = (uintptr_t)ptr;

    // Check Nursery bounds
    uintptr_t n_start = (uintptr_t)g_engine.nursery_raw;
    uintptr_t n_end   = n_start + g_engine.nursery_size;
    if (p >= n_start + GC_HEADER_SIZE && p < n_end) return true;

    // Check Tenured bounds
    uintptr_t t_start = (uintptr_t)g_engine.tenured_raw;
    uintptr_t t_end   = t_start + g_engine.tenured_size;
    if (p >= t_start + GC_HEADER_SIZE && p < t_end) return true;

    return false;
}

void gc_set_tenuring_threshold(uint8_t threshold) {
    g_engine.tenuring_threshold = threshold;
}

bool gc_init(size_t nursery_bytes, size_t tenured_bytes) {
    if (g_engine.initialized) return true;

    // Align sizes to OS page boundaries
    g_engine.nursery_size = mem_pal_align_page(nursery_bytes);
    g_engine.tenured_size = mem_pal_align_page(tenured_bytes);

    // Allocate virtual memory from OS PAL
    g_engine.nursery_raw = mem_pal_alloc_pages(g_engine.nursery_size, MEM_PROT_READWRITE);
    if (!g_engine.nursery_raw) {
        fprintf(stderr, "[GC] Failed to allocate virtual memory for nursery!\n");
        return false;
    }

    g_engine.tenured_raw = mem_pal_alloc_pages(g_engine.tenured_size, MEM_PROT_READWRITE);
    if (!g_engine.tenured_raw) {
        fprintf(stderr, "[GC] Failed to allocate virtual memory for tenured heap!\n");
        mem_pal_free_pages(g_engine.nursery_raw, g_engine.nursery_size);
        return false;
    }

    // Initialize allocators
    bump_alloc_init(&g_engine.nursery_alloc, g_engine.nursery_raw, g_engine.nursery_size);
    freelist_alloc_init(&g_engine.tenured_alloc, g_engine.tenured_raw, g_engine.tenured_size);

    roots_registry_init(&g_engine.roots);
    g_engine.all_objects = NULL;
    g_engine.tenuring_threshold = DEFAULT_TENURING_THRESHOLD;
    memset(&g_engine.stats, 0, sizeof(gc_stats_t));
    g_engine.initialized = true;

    return true;
}

void gc_shutdown(void) {
    if (!g_engine.initialized) return;

    roots_registry_clear(&g_engine.roots);
    grey_worklist_cleanup();

    if (g_engine.nursery_raw) {
        mem_pal_free_pages(g_engine.nursery_raw, g_engine.nursery_size);
        g_engine.nursery_raw = NULL;
    }

    if (g_engine.tenured_raw) {
        mem_pal_free_pages(g_engine.tenured_raw, g_engine.tenured_size);
        g_engine.tenured_raw = NULL;
    }

    g_engine.all_objects = NULL;
    g_engine.initialized = false;
}

bool gc_root_register(void** slot) {
    if (!g_engine.initialized) return false;
    return roots_registry_add(&g_engine.roots, slot);
}

bool gc_root_unregister(void** slot) {
    if (!g_engine.initialized) return false;
    return roots_registry_remove(&g_engine.roots, slot);
}

/* ========================================================================= */
/* Allocation Subsystem                                                      */
/* ========================================================================= */

void* gc_alloc(size_t size, gc_trace_fn trace) {
    if (!g_engine.initialized) return NULL;

    // Total block size = header + payload aligned to 8 bytes
    size_t aligned_payload = (size + 7) & ~7;
    size_t total_size = GC_HEADER_SIZE + aligned_payload;
    size_t actual_allocated = total_size;

    // Attempt allocation in Nursery (Young Generation)
    void* mem = bump_alloc(&g_engine.nursery_alloc, total_size, 8);
    uint8_t gen = GC_GEN_NURSERY;

    if (!mem) {
        // Nursery full -> Trigger Minor Collection (promotes survivors and resets nursery)
        gc_collect_minor();
        mem = bump_alloc(&g_engine.nursery_alloc, total_size, 8);
    }

    if (!mem) {
        // Fall back to Tenured Space
        mem = freelist_alloc(&g_engine.tenured_alloc, total_size, 8, &actual_allocated);
        gen = GC_GEN_TENURED;
        if (!mem) {
            // Tenured full -> Trigger Major Full GC
            gc_collect_major();
            mem = freelist_alloc(&g_engine.tenured_alloc, total_size, 8, &actual_allocated);
        }
        if (!mem) {
            // If still full, run compaction to defragment
            gc_compact();
            mem = freelist_alloc(&g_engine.tenured_alloc, total_size, 8, &actual_allocated);
        }
    }

    if (!mem) {
        fprintf(stderr, "[GC] FATAL: Out of managed memory! (Requested %zu bytes)\n", size);
        return NULL;
    }

    // Initialize Header
    gc_header_t* hdr = (gc_header_t*)mem;
    hdr->payload_size = (uint32_t)size;
    hdr->allocated_size = (uint32_t)actual_allocated;
    hdr->color = GC_COLOR_WHITE;
    hdr->generation = gen;
    hdr->age = 0;
    hdr->flags = 0;
    hdr->trace = trace;
    hdr->forwarding_address = NULL;

    // Prepend to active objects list
    hdr->next = g_engine.all_objects;
    g_engine.all_objects = hdr;

    // Clear user payload memory
    void* payload = gc_header_to_payload(hdr);
    memset(payload, 0, size);

    g_engine.stats.total_allocations++;
    g_engine.stats.total_bytes_allocated += actual_allocated;
    g_engine.stats.current_heap_usage += actual_allocated;

    return payload;
}

/* ========================================================================= */
/* Tri-Color Tracing Implementation                                          */
/* ========================================================================= */

static void field_visitor_callback(void** field_ptr, void* ctx) {
    (void)ctx;
    if (!field_ptr || !*field_ptr) return;

    if (!is_managed_payload(*field_ptr)) return;

    gc_header_t* child_hdr = gc_payload_to_header(*field_ptr);
    if (!child_hdr) return;

    // If White (unreached), mark Grey and push to worklist
    if (child_hdr->color == GC_COLOR_WHITE) {
        child_hdr->color = GC_COLOR_GREY;
        grey_worklist_push(child_hdr);
    }
}

static void root_scanner_callback(void** slot, void* ctx) {
    (void)ctx;
    if (!slot || !*slot) return;

    if (!is_managed_payload(*slot)) return;

    gc_header_t* root_hdr = gc_payload_to_header(*slot);
    if (!root_hdr) return;

    if (root_hdr->color == GC_COLOR_WHITE) {
        root_hdr->color = GC_COLOR_GREY;
        grey_worklist_push(root_hdr);
    }
}

static void gc_mark_phase(void) {
    g_grey_worklist.count = 0;

    // Reset all objects to WHITE and clear forwarding addresses
    gc_header_t* curr = g_engine.all_objects;
    while (curr) {
        curr->color = GC_COLOR_WHITE;
        curr->forwarding_address = NULL;
        curr = curr->next;
    }

    // Scan all registered roots
    roots_registry_iterate(&g_engine.roots, root_scanner_callback, NULL);

    // Process Grey worklist until empty
    gc_header_t* grey_obj = NULL;
    while ((grey_obj = grey_worklist_pop()) != NULL) {
        grey_obj->color = GC_COLOR_BLACK;

        if (grey_obj->trace) {
            void* payload = gc_header_to_payload(grey_obj);
            grey_obj->trace(payload, field_visitor_callback, NULL);
        }
    }
}

/* ========================================================================= */
/* Reference Updating Helper (For Promotion & Compaction)                   */
/* ========================================================================= */

static void update_pointer_if_forwarded(void** ptr_slot) {
    if (!ptr_slot || !*ptr_slot) return;
    if (!is_managed_payload(*ptr_slot)) return;

    gc_header_t* old_hdr = gc_payload_to_header(*ptr_slot);
    if (old_hdr && old_hdr->forwarding_address) {
        *ptr_slot = old_hdr->forwarding_address;
    }
}

static void root_updater_callback(void** slot, void* ctx) {
    (void)ctx;
    update_pointer_if_forwarded(slot);
}

static void field_updater_callback(void** field_ptr, void* ctx) {
    (void)ctx;
    update_pointer_if_forwarded(field_ptr);
}

static void gc_update_all_references(void) {
    // 1. Update roots
    roots_registry_iterate(&g_engine.roots, root_updater_callback, NULL);

    // 2. Update fields in all live objects
    gc_header_t* curr = g_engine.all_objects;
    while (curr) {
        if (curr->trace) {
            void* payload = gc_header_to_payload(curr);
            curr->trace(payload, field_updater_callback, NULL);
        }
        curr = curr->next;
    }
}

/* ========================================================================= */
/* Feature 1: Object Promotion & Generational Tenuring                       */
/* ========================================================================= */

static void gc_promote_nursery_survivors(void) {
    gc_header_t* curr = g_engine.all_objects;

    // First pass: Copy surviving nursery objects to Tenured Space and record forwarding addresses
    while (curr) {
        if (curr->generation == GC_GEN_NURSERY && curr->color == GC_COLOR_BLACK) {
            curr->age++;

            // Check if survival age exceeds threshold (or promote all nursery survivors to allow 100% reset)
            if (curr->age >= g_engine.tenuring_threshold) {
                size_t actual_allocated = 0;
                void* new_block = freelist_alloc(&g_engine.tenured_alloc, curr->allocated_size, 8, &actual_allocated);

                if (!new_block) {
                    // Tenured full -> defragment first
                    gc_compact();
                    new_block = freelist_alloc(&g_engine.tenured_alloc, curr->allocated_size, 8, &actual_allocated);
                }

                if (new_block) {
                    // Copy object header and payload to Tenured space
                    memcpy(new_block, curr, curr->allocated_size);

                    gc_header_t* new_hdr = (gc_header_t*)new_block;
                    new_hdr->generation = GC_GEN_TENURED;
                    new_hdr->allocated_size = (uint32_t)actual_allocated;
                    new_hdr->forwarding_address = NULL;

                    // Set forwarding address on old nursery object
                    curr->forwarding_address = gc_header_to_payload(new_hdr);

                    g_engine.stats.total_objects_promoted++;
                    g_engine.stats.total_bytes_promoted += curr->allocated_size;
                }
            }
        }
        curr = curr->next;
    }

    // Second pass: Update all references to point to new tenured locations
    gc_update_all_references();

    // Third pass: Replace forwarded nursery headers in all_objects list with new tenured headers
    gc_header_t** curr_ptr = &g_engine.all_objects;
    while (*curr_ptr) {
        gc_header_t* hdr = *curr_ptr;
        if (hdr->generation == GC_GEN_NURSERY) {
            if (hdr->forwarding_address) {
                // Relink to the promoted tenured object
                gc_header_t* promoted_hdr = gc_payload_to_header(hdr->forwarding_address);
                promoted_hdr->next = hdr->next;
                *curr_ptr = promoted_hdr;
                curr_ptr = &promoted_hdr->next;
            } else if (hdr->color == GC_COLOR_WHITE) {
                // Dead nursery object
                *curr_ptr = hdr->next;
                g_engine.stats.total_bytes_reclaimed += hdr->allocated_size;
                g_engine.stats.total_objects_reclaimed++;
                if (g_engine.stats.current_heap_usage >= hdr->allocated_size) {
                    g_engine.stats.current_heap_usage -= hdr->allocated_size;
                }
            } else {
                curr_ptr = &hdr->next;
            }
        } else {
            curr_ptr = &hdr->next;
        }
    }

    // Now all promoted objects are in Tenured space, and dead ones are discarded:
    // Nursery can be reset 100%!
    bump_alloc_reset(&g_engine.nursery_alloc);
}

void gc_collect_minor(void) {
    g_engine.stats.minor_gc_count++;
    gc_mark_phase();
    gc_promote_nursery_survivors();
}

/* ========================================================================= */
/* Sweeping Subsystem                                                        */
/* ========================================================================= */

static void gc_sweep_phase(void) {
    gc_header_t** curr_ptr = &g_engine.all_objects;

    while (*curr_ptr) {
        gc_header_t* hdr = *curr_ptr;
        size_t block_size = hdr->allocated_size;

        if (hdr->color == GC_COLOR_WHITE) {
            *curr_ptr = hdr->next;

            if (hdr->generation == GC_GEN_TENURED) {
                freelist_free(&g_engine.tenured_alloc, hdr, block_size);
            }

            g_engine.stats.total_bytes_reclaimed += block_size;
            g_engine.stats.total_objects_reclaimed++;
            if (g_engine.stats.current_heap_usage >= block_size) {
                g_engine.stats.current_heap_usage -= block_size;
            }
        } else {
            hdr->age++;
            hdr->color = GC_COLOR_WHITE;
            curr_ptr = &hdr->next;
        }
    }
}

void gc_collect_major(void) {
    g_engine.stats.major_gc_count++;
    gc_mark_phase();
    gc_sweep_phase();

    // Check if nursery is clear
    bool nursery_has_survivors = false;
    gc_header_t* curr = g_engine.all_objects;
    while (curr) {
        if (curr->generation == GC_GEN_NURSERY) {
            nursery_has_survivors = true;
            break;
        }
        curr = curr->next;
    }
    if (!nursery_has_survivors) {
        bump_alloc_reset(&g_engine.nursery_alloc);
    }
}

void gc_collect(void) {
    gc_collect_minor();
}

/* ========================================================================= */
/* Feature 2: LISP-2 Heap Compactor & Defragmenter                           */
/* ========================================================================= */

// Comparison helper to sort headers by memory address
static int compare_headers_by_address(const void* a, const void* b) {
    const gc_header_t* h1 = *(const gc_header_t**)a;
    const gc_header_t* h2 = *(const gc_header_t**)b;
    if ((uintptr_t)h1 < (uintptr_t)h2) return -1;
    if ((uintptr_t)h1 > (uintptr_t)h2) return 1;
    return 0;
}

void gc_compact(void) {
    g_engine.stats.compaction_count++;

    // Step 1: Run mark phase to identify all live objects
    gc_mark_phase();

    // Step 2: Collect all live Tenured objects
    size_t live_count = 0;
    gc_header_t* curr = g_engine.all_objects;
    while (curr) {
        if (curr->generation == GC_GEN_TENURED && curr->color == GC_COLOR_BLACK) {
            live_count++;
        }
        curr = curr->next;
    }

    if (live_count == 0) {
        // No live objects in Tenured space: reset free list to full capacity
        freelist_alloc_init(&g_engine.tenured_alloc, g_engine.tenured_raw, g_engine.tenured_size);
        return;
    }

    gc_header_t** live_objects = (gc_header_t**)malloc(live_count * sizeof(gc_header_t*));
    if (!live_objects) {
        fprintf(stderr, "[GC Error] Insufficient memory to perform LISP-2 compaction!\n");
        return;
    }

    size_t idx = 0;
    curr = g_engine.all_objects;
    while (curr) {
        if (curr->generation == GC_GEN_TENURED && curr->color == GC_COLOR_BLACK) {
            live_objects[idx++] = curr;
        }
        curr = curr->next;
    }

    // Sort live objects in ascending order of memory address
    qsort(live_objects, live_count, sizeof(gc_header_t*), compare_headers_by_address);

    // Step 3 (Pass 1): Calculate forwarding addresses contiguously from base of Tenured heap
    uint8_t* dest_ptr = (uint8_t*)g_engine.tenured_raw;
    for (size_t i = 0; i < live_count; i++) {
        gc_header_t* hdr = live_objects[i];
        hdr->forwarding_address = gc_header_to_payload((gc_header_t*)dest_ptr);
        dest_ptr += hdr->allocated_size;
    }

    // Step 4 (Pass 2): Update all references across roots and object fields
    gc_update_all_references();

    // Step 5 (Pass 3): Physically relocate objects to their compact destinations
    g_engine.all_objects = NULL;
    for (size_t i = 0; i < live_count; i++) {
        gc_header_t* old_hdr = live_objects[i];
        gc_header_t* new_hdr = gc_payload_to_header(old_hdr->forwarding_address);
        size_t block_sz = old_hdr->allocated_size;

        if (new_hdr != old_hdr) {
            memmove(new_hdr, old_hdr, block_sz);
        }

        new_hdr->forwarding_address = NULL;
        new_hdr->color = GC_COLOR_WHITE;

        // Re-link into all_objects list
        new_hdr->next = g_engine.all_objects;
        g_engine.all_objects = new_hdr;
    }

    free(live_objects);

    // Step 6: Reconstruct Free-List with the single contiguous remaining block!
    size_t bytes_occupied = (size_t)(dest_ptr - (uint8_t*)g_engine.tenured_raw);
    size_t remaining_free = g_engine.tenured_size > bytes_occupied ? (g_engine.tenured_size - bytes_occupied) : 0;

    if (remaining_free >= sizeof(fl_node_t)) {
        freelist_alloc_init(&g_engine.tenured_alloc, dest_ptr, remaining_free);
    } else {
        // Entire tenured space is packed
        g_engine.tenured_alloc.head = NULL;
        g_engine.tenured_alloc.bytes_allocated = bytes_occupied;
    }
}

gc_stats_t gc_get_stats(void) {
    return g_engine.stats;
}

void gc_dump_stats(void) {
    printf("\n================ [GC Engine Telemetry] ================\n");
    printf(" Total Allocations      : %llu\n", (unsigned long long)g_engine.stats.total_allocations);
    printf(" Total Allocated Memory : %llu bytes\n", (unsigned long long)g_engine.stats.total_bytes_allocated);
    printf(" Minor Collections      : %llu\n", (unsigned long long)g_engine.stats.minor_gc_count);
    printf(" Major Collections      : %llu\n", (unsigned long long)g_engine.stats.major_gc_count);
    printf(" Objects Promoted       : %llu (%llu bytes)\n", 
           (unsigned long long)g_engine.stats.total_objects_promoted,
           (unsigned long long)g_engine.stats.total_bytes_promoted);
    printf(" Compactions Executed   : %llu (LISP-2 Defrag)\n", (unsigned long long)g_engine.stats.compaction_count);
    printf(" Objects Reclaimed      : %llu\n", (unsigned long long)g_engine.stats.total_objects_reclaimed);
    printf(" Bytes Reclaimed        : %llu bytes\n", (unsigned long long)g_engine.stats.total_bytes_reclaimed);
    printf(" Active Heap Usage      : %zu bytes\n", g_engine.stats.current_heap_usage);
    printf(" Active Registered Roots: %zu\n", g_engine.roots.count);
    printf(" Nursery Remaining      : %zu / %zu bytes\n", 
           bump_alloc_remaining(&g_engine.nursery_alloc), g_engine.nursery_size);
    printf("========================================================\n\n");
}
