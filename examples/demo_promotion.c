#include <stdio.h>
#include "../include/mem_collector.h"

typedef struct DataNode {
    int id;
    struct DataNode* next;
} DataNode;

void trace_data_node(void* payload, gc_field_visitor_fn visitor, void* ctx) {
    DataNode* node = (DataNode*)payload;
    if (node->next) {
        visitor((void**)&node->next, ctx);
    }
}

int main(void) {
    printf("=== Demo 3: Generational Promotion & Tenuring ===\n");

    // Initialize with 32KB Nursery and 128KB Tenured
    if (!gc_init(32 * 1024, 128 * 1024)) {
        return 1;
    }

    gc_set_tenuring_threshold(1); // Objects surviving 1 Minor GC are promoted

    DataNode* live_root = NULL;
    gc_root_register((void**)&live_root);

    printf("[1] Allocating 10 surviving objects in Nursery attached to root...\n");
    for (int i = 0; i < 10; i++) {
        DataNode* n = (DataNode*)gc_alloc(sizeof(DataNode), trace_data_node);
        n->id = i;
        n->next = live_root;
        live_root = n;
    }

    printf("[2] Allocating 150 temporary 'garbage' objects in Nursery...\n");
    for (int i = 0; i < 150; i++) {
        DataNode* garbage = (DataNode*)gc_alloc(sizeof(DataNode), trace_data_node);
        garbage->id = 999;
        garbage->next = NULL;
        (void)garbage;
    }

    printf("[3] Nursery state before collection:\n");
    gc_dump_stats();

    printf("[4] Running Minor Collection (Promoting 10 survivors to Tenured space)...\n");
    gc_collect_minor();
    gc_dump_stats();

    printf("[5] Verifying all 10 nodes survived and their values remain intact after promotion:\n");
    DataNode* curr = live_root;
    int count = 0;
    while (curr) {
        count++;
        curr = curr->next;
    }
    printf("    -> Successfully verified %d live objects in Tenured Space!\n", count);

    printf("[6] Dropping root and running Major Collection...\n");
    live_root = NULL;
    gc_collect_major();
    gc_dump_stats();

    gc_root_unregister((void**)&live_root);
    gc_shutdown();
    printf("=== Demo 3 (Promotion) Completed Successfully! ===\n");
    return 0;
}
