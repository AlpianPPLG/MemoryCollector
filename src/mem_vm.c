#include "../include/mem_vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/* GC Tracing Callbacks for VM Objects                                       */
/* ========================================================================= */

static void trace_vm_array(void* payload, gc_field_visitor_fn visitor, void* ctx) {
    vm_array_t* arr = (vm_array_t*)payload;
    for (size_t i = 0; i < arr->count; i++) {
        if (arr->elements[i].type == VAL_OBJ && arr->elements[i].as.obj) {
            visitor(&arr->elements[i].as.obj, ctx);
        }
    }
}

static void trace_vm_pair(void* payload, gc_field_visitor_fn visitor, void* ctx) {
    vm_pair_t* pair = (vm_pair_t*)payload;
    if (pair->head.type == VAL_OBJ && pair->head.as.obj) {
        visitor(&pair->head.as.obj, ctx);
    }
    if (pair->tail.type == VAL_OBJ && pair->tail.as.obj) {
        visitor(&pair->tail.as.obj, ctx);
    }
}

/* ========================================================================= */
/* Managed Object Constructors                                               */
/* ========================================================================= */

vm_string_t* vm_create_string(const char* text, size_t len) {
    size_t total_size = sizeof(vm_string_t) + len + 1;
    vm_string_t* str = (vm_string_t*)gc_alloc(total_size, NULL);
    if (!str) return NULL;

    str->base.type = OBJ_STRING;
    str->length = len;
    if (text) {
        memcpy(str->chars, text, len);
    }
    str->chars[len] = '\0';
    return str;
}

vm_array_t* vm_create_array(size_t count) {
    size_t total_size = sizeof(vm_array_t) + (count * sizeof(vm_val_t));
    vm_array_t* arr = (vm_array_t*)gc_alloc(total_size, trace_vm_array);
    if (!arr) return NULL;

    arr->base.type = OBJ_ARRAY;
    arr->count = count;
    for (size_t i = 0; i < count; i++) {
        arr->elements[i] = vm_val_nil();
    }
    return arr;
}

vm_pair_t* vm_create_pair(vm_val_t head, vm_val_t tail) {
    vm_pair_t* pair = (vm_pair_t*)gc_alloc(sizeof(vm_pair_t), trace_vm_pair);
    if (!pair) return NULL;

    pair->base.type = OBJ_PAIR;
    pair->head = head;
    pair->tail = tail;
    return pair;
}

/* ========================================================================= */
/* VM Execution Engine                                                       */
/* ========================================================================= */

static void print_val(vm_val_t val) {
    switch (val.type) {
        case VAL_NIL:
            printf("nil\n");
            break;
        case VAL_INT:
            printf("%lld\n", (long long)val.as.integer);
            break;
        case VAL_OBJ: {
            if (!val.as.obj) {
                printf("<null obj>\n");
                return;
            }
            vm_obj_header_t* header = (vm_obj_header_t*)val.as.obj;
            if (header->type == OBJ_STRING) {
                vm_string_t* s = (vm_string_t*)header;
                printf("\"%s\"\n", s->chars);
            } else if (header->type == OBJ_ARRAY) {
                vm_array_t* a = (vm_array_t*)header;
                printf("<array of %zu items>\n", a->count);
            } else if (header->type == OBJ_PAIR) {
                printf("<pair>\n");
            } else {
                printf("<unknown obj>\n");
            }
            break;
        }
    }
}

bool vm_init(vm_t* vm, const uint8_t* bytecode, size_t code_size) {
    if (!vm) return false;
    vm->bytecode = bytecode;
    vm->code_size = code_size;
    vm->ip = 0;
    vm->sp = 0;
    vm->running = false;

    // Register all VM stack slots as GC roots so active stack operands are never swept!
    for (size_t i = 0; i < VM_STACK_MAX; i++) {
        vm->stack[i] = vm_val_nil();
        gc_root_register((void**)&vm->stack[i].as.obj);
    }

    return true;
}

void vm_shutdown(vm_t* vm) {
    if (!vm) return;
    for (size_t i = 0; i < VM_STACK_MAX; i++) {
        gc_root_unregister((void**)&vm->stack[i].as.obj);
        vm->stack[i] = vm_val_nil();
    }
}

static inline bool vm_push(vm_t* vm, vm_val_t val) {
    if (vm->sp >= VM_STACK_MAX) {
        fprintf(stderr, "[VM Error] Stack overflow!\n");
        return false;
    }
    vm->stack[vm->sp++] = val;
    return true;
}

static inline vm_val_t vm_pop(vm_t* vm) {
    if (vm->sp == 0) {
        fprintf(stderr, "[VM Error] Stack underflow!\n");
        return vm_val_nil();
    }
    vm_val_t val = vm->stack[--vm->sp];
    // Clear popped slot to prevent stale reference retention in GC
    vm->stack[vm->sp] = vm_val_nil();
    return val;
}

bool vm_run(vm_t* vm) {
    if (!vm || !vm->bytecode) return false;
    vm->running = true;

    while (vm->running && vm->ip < vm->code_size) {
        uint8_t op = vm->bytecode[vm->ip++];

        switch (op) {
            case OP_HALT:
                vm->running = false;
                break;

            case OP_PUSH_INT: {
                int64_t val = 0;
                memcpy(&val, &vm->bytecode[vm->ip], sizeof(int64_t));
                vm->ip += sizeof(int64_t);
                vm_push(vm, vm_val_int(val));
                break;
            }

            case OP_PUSH_NIL:
                vm_push(vm, vm_val_nil());
                break;

            case OP_NEW_STRING: {
                uint32_t len = 0;
                memcpy(&len, &vm->bytecode[vm->ip], sizeof(uint32_t));
                vm->ip += sizeof(uint32_t);

                const char* text = (const char*)&vm->bytecode[vm->ip];
                vm->ip += len;

                vm_string_t* str = vm_create_string(text, len);
                vm_push(vm, vm_val_obj(str));
                break;
            }

            case OP_NEW_ARRAY: {
                uint32_t count = 0;
                memcpy(&count, &vm->bytecode[vm->ip], sizeof(uint32_t));
                vm->ip += sizeof(uint32_t);

                vm_array_t* arr = vm_create_array(count);
                if (!arr) return false;

                // Pop elements in reverse order
                for (int i = (int)count - 1; i >= 0; i--) {
                    arr->elements[i] = vm_pop(vm);
                }
                vm_push(vm, vm_val_obj(arr));
                break;
            }

            case OP_NEW_PAIR: {
                vm_val_t tail = vm_pop(vm);
                vm_val_t head = vm_pop(vm);
                vm_pair_t* pair = vm_create_pair(head, tail);
                vm_push(vm, vm_val_obj(pair));
                break;
            }

            case OP_ARRAY_GET: {
                vm_val_t idx_val = vm_pop(vm);
                vm_val_t arr_val = vm_pop(vm);
                if (arr_val.type != VAL_OBJ || idx_val.type != VAL_INT) {
                    fprintf(stderr, "[VM Error] Type error in OP_ARRAY_GET\n");
                    return false;
                }
                vm_array_t* arr = (vm_array_t*)arr_val.as.obj;
                int64_t idx = idx_val.as.integer;
                if (idx < 0 || (size_t)idx >= arr->count) {
                    fprintf(stderr, "[VM Error] Array index %lld out of bounds (size %zu)\n", (long long)idx, arr->count);
                    return false;
                }
                vm_push(vm, arr->elements[idx]);
                break;
            }

            case OP_ARRAY_SET: {
                vm_val_t val = vm_pop(vm);
                vm_val_t idx_val = vm_pop(vm);
                vm_val_t arr_val = vm_pop(vm);
                if (arr_val.type != VAL_OBJ || idx_val.type != VAL_INT) {
                    fprintf(stderr, "[VM Error] Type error in OP_ARRAY_SET\n");
                    return false;
                }
                vm_array_t* arr = (vm_array_t*)arr_val.as.obj;
                int64_t idx = idx_val.as.integer;
                if (idx < 0 || (size_t)idx >= arr->count) {
                    fprintf(stderr, "[VM Error] Array index out of bounds in SET\n");
                    return false;
                }
                arr->elements[idx] = val;
                break;
            }

            case OP_DUP: {
                if (vm->sp == 0) return false;
                vm_val_t top = vm->stack[vm->sp - 1];
                vm_push(vm, top);
                break;
            }

            case OP_POP:
                vm_pop(vm);
                break;

            case OP_ADD: {
                vm_val_t b = vm_pop(vm);
                vm_val_t a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, vm_val_int(a.as.integer + b.as.integer));
                } else {
                    return false;
                }
                break;
            }

            case OP_SUB: {
                vm_val_t b = vm_pop(vm);
                vm_val_t a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, vm_val_int(a.as.integer - b.as.integer));
                } else {
                    return false;
                }
                break;
            }

            case OP_MUL: {
                vm_val_t b = vm_pop(vm);
                vm_val_t a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT) {
                    vm_push(vm, vm_val_int(a.as.integer * b.as.integer));
                } else {
                    return false;
                }
                break;
            }

            case OP_DIV: {
                vm_val_t b = vm_pop(vm);
                vm_val_t a = vm_pop(vm);
                if (a.type == VAL_INT && b.type == VAL_INT && b.as.integer != 0) {
                    vm_push(vm, vm_val_int(a.as.integer / b.as.integer));
                } else {
                    return false;
                }
                break;
            }

            case OP_PRINT: {
                vm_val_t top = vm_pop(vm);
                printf("[VM Output] ");
                print_val(top);
                break;
            }

            case OP_GC:
                printf("[VM] Triggering explicit Garbage Collection...\n");
                gc_collect();
                break;

            default:
                fprintf(stderr, "[VM Error] Unknown opcode: 0x%02X at ip=%zu\n", op, vm->ip - 1);
                return false;
        }
    }

    return true;
}
