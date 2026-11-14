#pragma once

#include "mem_collector.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mem_vm.h
 * @brief Stack-based Bytecode Virtual Machine with full Garbage Collector integration.
 */

typedef enum {
    VAL_NIL,
    VAL_INT,
    VAL_OBJ
} vm_val_type_t;

typedef enum {
    OBJ_STRING,
    OBJ_ARRAY,
    OBJ_PAIR
} vm_obj_type_t;

typedef struct {
    vm_obj_type_t type;
} vm_obj_header_t;

struct vm_val;

typedef struct vm_val {
    vm_val_type_t type;
    union {
        int64_t integer;
        void*   obj; // Pointer to managed GC object payload
    } as;
} vm_val_t;

typedef struct {
    vm_obj_header_t base;
    size_t length;
    char chars[];
} vm_string_t;

typedef struct {
    vm_obj_header_t base;
    size_t count;
    vm_val_t elements[];
} vm_array_t;

typedef struct {
    vm_obj_header_t base;
    vm_val_t head;
    vm_val_t tail;
} vm_pair_t;

// VM Value constructors
static inline vm_val_t vm_val_nil(void) {
    return (vm_val_t){ .type = VAL_NIL, .as.integer = 0 };
}

static inline vm_val_t vm_val_int(int64_t val) {
    return (vm_val_t){ .type = VAL_INT, .as.integer = val };
}

static inline vm_val_t vm_val_obj(void* ptr) {
    return (vm_val_t){ .type = VAL_OBJ, .as.obj = ptr };
}

// Bytecode Opcodes
typedef enum {
    OP_HALT = 0,
    OP_PUSH_INT,       // Operand: int64_t
    OP_PUSH_NIL,
    OP_NEW_STRING,     // Operands: uint32_t len, followed by characters
    OP_NEW_ARRAY,      // Operand: uint32_t count (pops count items from stack)
    OP_NEW_PAIR,       // Pops head and tail, pushes Pair
    OP_ARRAY_GET,      // Stack: array, index -> val
    OP_ARRAY_SET,      // Stack: array, index, val -> none
    OP_DUP,
    OP_POP,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_PRINT,          // Pops and prints value to stdout
    OP_GC              // Explicitly invoke GC
} vm_opcode_t;

#define VM_STACK_MAX 1024

typedef struct {
    vm_val_t stack[VM_STACK_MAX];
    size_t sp;               // Stack pointer
    const uint8_t* bytecode;
    size_t code_size;
    size_t ip;               // Instruction pointer
    bool running;
} vm_t;

// VM lifecycle & execution
bool vm_init(vm_t* vm, const uint8_t* bytecode, size_t code_size);
void vm_shutdown(vm_t* vm);
bool vm_run(vm_t* vm);

// VM Object Creators (allocated on GC heap)
vm_string_t* vm_create_string(const char* text, size_t len);
vm_array_t*  vm_create_array(size_t count);
vm_pair_t*   vm_create_pair(vm_val_t head, vm_val_t tail);

#ifdef __cplusplus
}
#endif
