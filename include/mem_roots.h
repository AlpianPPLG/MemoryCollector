#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mem_roots.h
 * @brief Dynamic registry for precise root scanning (stack & global variables).
 */

typedef struct root_entry {
    void** slot;               // Pointer to the variable holding the managed object pointer
    struct root_entry* next;
} root_entry_t;

typedef struct {
    root_entry_t* head;
    size_t count;
} roots_registry_t;

void roots_registry_init(roots_registry_t* reg);
bool roots_registry_add(roots_registry_t* reg, void** slot);
bool roots_registry_remove(roots_registry_t* reg, void** slot);
void roots_registry_clear(roots_registry_t* reg);

typedef void (*root_callback_fn)(void** slot, void* ctx);
void roots_registry_iterate(roots_registry_t* reg, root_callback_fn callback, void* ctx);

#ifdef __cplusplus
}
#endif
