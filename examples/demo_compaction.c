#include <stdio.h>
#include "../include/mem_collector.h"

typedef struct Block {
    int id;
    int payload[8];
} Block;

int main(void) {
    printf("=== Demo 4: Heap Fragmentation & LISP-2 Compaction ===\n");

    // Initialize with 4KB Nursery and 64KB Tenured
    if (!gc_init(4 * 1024, 64 * 1024)) {
        return 1;
    }

    #define NUM_BLOCKS 50
    Block* roots[NUM_BLOCKS] = {0};

    // Register all root slots
    for (int i = 0; i < NUM_BLOCKS; i++) {
        gc_root_register((void**)&roots[i]);
    }

    printf("[1] Allocating %d objects directly into Tenured Space...\n", NUM_BLOCKS);
    for (int i = 0; i < NUM_BLOCKS; i++) {
        // Allocate directly into Tenured by exhausting nursery or promoting
        roots[i] = (Block*)gc_alloc(sizeof(Block), NULL);
        roots[i]->id = i;
    }
    // Promote them all into Tenured space
    gc_collect_minor();

    printf("[2] Intentionally creating heavy 'checkerboard' fragmentation...\n");
    printf("    Freeing every odd-numbered block (roots[1], roots[3], roots[5], ...)\n");
    for (int i = 1; i < NUM_BLOCKS; i += 2) {
        roots[i] = NULL; // Sever root
    }

    printf("[3] Running sweep to free the 25 odd blocks (leaving 25 holes in memory):\n");
    gc_collect_major();
    gc_dump_stats();

    printf("[4] Executing LISP-2 Heap Compaction (Defragmentation)...\n");
    gc_compact();
    gc_dump_stats();

    printf("[5] Verifying integrity of remaining even blocks after physical memory relocation:\n");
    bool all_valid = true;
    for (int i = 0; i < NUM_BLOCKS; i += 2) {
        if (!roots[i] || roots[i]->id != i) {
            all_valid = false;
            printf("    [Error] Block %d data corrupted after compaction!\n", i);
        }
    }
    if (all_valid) {
        printf("    -> All 25 surviving blocks verified intact at their new contiguous addresses!\n");
    }

    // Clean up
    for (int i = 0; i < NUM_BLOCKS; i++) {
        gc_root_unregister((void**)&roots[i]);
    }
    gc_shutdown();
    printf("=== Demo 4 (LISP-2 Compaction) Completed Successfully! ===\n");
    return 0;
}
