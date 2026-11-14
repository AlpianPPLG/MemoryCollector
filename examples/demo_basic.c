#include <stdio.h>
#include <string.h>
#include "../include/mem_collector.h"

typedef struct Node {
    int value;
    struct Node* next;
} Node;

// Trace function for Node
void trace_node(void* payload, gc_field_visitor_fn visitor, void* ctx) {
    Node* n = (Node*)payload;
    if (n->next) {
        visitor((void**)&n->next, ctx);
    }
}

int main(void) {
    printf("=== Demo 1: Basic Allocation & GC Root Tracking ===\n");

    // Initialize GC with 64KB Nursery and 256KB Tenured
    if (!gc_init(64 * 1024, 256 * 1024)) {
        return 1;
    }

    Node* root_node = NULL;
    gc_root_register((void**)&root_node);

    // Build a linked list of 100 items rooted at root_node
    printf("[1] Allocating 100-node linked list referenced from root...\n");
    for (int i = 0; i < 100; i++) {
        Node* n = (Node*)gc_alloc(sizeof(Node), trace_node);
        n->value = i;
        n->next = root_node;
        root_node = n;
    }

    printf("[2] Allocating 500 unreferenced 'garbage' nodes...\n");
    for (int i = 0; i < 500; i++) {
        Node* garbage = (Node*)gc_alloc(sizeof(Node), trace_node);
        garbage->value = 9999;
        garbage->next = NULL;
        (void)garbage;
    }

    printf("[3] Triggering Garbage Collection while root is active...\n");
    gc_collect();
    gc_dump_stats();

    printf("[4] Dropping the root reference (root_node = NULL)...\n");
    root_node = NULL;

    printf("[5] Triggering Garbage Collection again (everything should be collected)...\n");
    gc_collect();
    gc_dump_stats();

    gc_root_unregister((void**)&root_node);
    gc_shutdown();
    printf("=== Demo 1 Completed Successfully! ===\n");
    return 0;
}
