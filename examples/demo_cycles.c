#include <stdio.h>
#include "../include/mem_collector.h"

typedef struct CircularNode {
    const char* name;
    struct CircularNode* partner;
} CircularNode;

void trace_circular(void* payload, gc_field_visitor_fn visitor, void* ctx) {
    CircularNode* n = (CircularNode*)payload;
    if (n->partner) {
        visitor((void**)&n->partner, ctx);
    }
}

int main(void) {
    printf("=== Demo 2: Cyclic Graph & Circular Reference Resolution ===\n");

    if (!gc_init(64 * 1024, 256 * 1024)) {
        return 1;
    }

    CircularNode* root = NULL;
    gc_root_register((void**)&root);

    printf("[1] Creating circular graph: Node A -> Node B -> Node C -> Node A\n");
    CircularNode* a = (CircularNode*)gc_alloc(sizeof(CircularNode), trace_circular);
    CircularNode* b = (CircularNode*)gc_alloc(sizeof(CircularNode), trace_circular);
    CircularNode* c = (CircularNode*)gc_alloc(sizeof(CircularNode), trace_circular);

    a->name = "Node A";
    b->name = "Node B";
    c->name = "Node C";

    a->partner = b;
    b->partner = c;
    c->partner = a; // Completes the cycle!

    root = a;

    printf("[2] Triggering GC with root pointing to Node A...\n");
    gc_collect();
    gc_dump_stats();

    printf("[3] Severing root pointer (root = NULL)...\n");
    printf("    Note: Reference Counting would fail and LEAK here because ref_count of each node is >= 1.\n");
    printf("    Our Tri-Color Tracing GC identifies the entire cyclic island as unreachable!\n");
    root = NULL;

    printf("[4] Triggering GC to collect cyclic island...\n");
    gc_collect();
    gc_dump_stats();

    gc_root_unregister((void**)&root);
    gc_shutdown();
    printf("=== Demo 2 Completed Successfully! ===\n");
    return 0;
}
