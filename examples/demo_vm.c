#include <stdio.h>
#include <string.h>
#include "../include/mem_vm.h"

int main(void) {
    printf("=== Demo 5: Bytecode Virtual Machine with GC Integration ===\n");

    // Initialize GC Engine
    if (!gc_init(32 * 1024, 128 * 1024)) {
        return 1;
    }

    /*
     * Build Bytecode Program:
     * 1. Push 40
     * 2. Push 2
     * 3. Add -> 42
     * 4. Print -> [VM Output] 42
     * 5. Push string "Hello, Systems GC!"
     * 6. Print -> [VM Output] "Hello, Systems GC!"
     * 7. Push 100, 200, 300
     * 8. Create Array of 3 elements [100, 200, 300]
     * 9. Trigger OP_GC (Traces array on VM stack as live root!)
     * 10. Duplicate array, get index 1 (value 200)
     * 11. Print -> [VM Output] 200
     * 12. Print array -> [VM Output] <array of 3 items>
     * 13. Halt
     */

    uint8_t bytecode[256];
    size_t ip = 0;

    // 1. Push 40
    bytecode[ip++] = OP_PUSH_INT;
    int64_t val1 = 40;
    memcpy(&bytecode[ip], &val1, sizeof(int64_t));
    ip += sizeof(int64_t);

    // 2. Push 2
    bytecode[ip++] = OP_PUSH_INT;
    int64_t val2 = 2;
    memcpy(&bytecode[ip], &val2, sizeof(int64_t));
    ip += sizeof(int64_t);

    // 3. Add
    bytecode[ip++] = OP_ADD;

    // 4. Print
    bytecode[ip++] = OP_PRINT;

    // 5. Push string "Hello, Systems GC!"
    bytecode[ip++] = OP_NEW_STRING;
    const char* msg = "Hello, Systems GC!";
    uint32_t msg_len = (uint32_t)strlen(msg);
    memcpy(&bytecode[ip], &msg_len, sizeof(uint32_t));
    ip += sizeof(uint32_t);
    memcpy(&bytecode[ip], msg, msg_len);
    ip += msg_len;

    // 6. Print string
    bytecode[ip++] = OP_PRINT;

    // 7. Push elements 100, 200, 300
    for (int64_t v = 100; v <= 300; v += 100) {
        bytecode[ip++] = OP_PUSH_INT;
        memcpy(&bytecode[ip], &v, sizeof(int64_t));
        ip += sizeof(int64_t);
    }

    // 8. Create array of 3 elements
    bytecode[ip++] = OP_NEW_ARRAY;
    uint32_t arr_size = 3;
    memcpy(&bytecode[ip], &arr_size, sizeof(uint32_t));
    ip += sizeof(uint32_t);

    // 9. Trigger GC while array is on top of VM stack!
    bytecode[ip++] = OP_GC;

    // 10. Dup array, push index 1, array_get
    bytecode[ip++] = OP_DUP;
    bytecode[ip++] = OP_PUSH_INT;
    int64_t idx = 1;
    memcpy(&bytecode[ip], &idx, sizeof(int64_t));
    ip += sizeof(int64_t);
    bytecode[ip++] = OP_ARRAY_GET;

    // 11. Print array element at index 1
    bytecode[ip++] = OP_PRINT;

    // 12. Print array itself
    bytecode[ip++] = OP_PRINT;

    // 13. Halt
    bytecode[ip++] = OP_HALT;

    printf("[1] Initializing VM with compiled bytecode (%zu bytes)...\n", ip);
    vm_t vm;
    if (!vm_init(&vm, bytecode, ip)) {
        return 1;
    }

    printf("[2] Executing VM program...\n");
    if (!vm_run(&vm)) {
        printf("[VM Error] Execution failed!\n");
    }

    printf("\n[3] Final GC Engine Telemetry after VM execution:\n");
    gc_dump_stats();

    vm_shutdown(&vm);
    gc_shutdown();

    printf("=== Demo 5 (Bytecode VM) Completed Successfully! ===\n");
    return 0;
}
