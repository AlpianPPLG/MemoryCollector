#include "../include/mem_roots.h"
#include <stdlib.h>

void roots_registry_init(roots_registry_t* reg) {
    if (!reg) return;
    reg->head = NULL;
    reg->count = 0;
}

bool roots_registry_add(roots_registry_t* reg, void** slot) {
    if (!reg || !slot) return false;

    // Check if already registered
    root_entry_t* curr = reg->head;
    while (curr) {
        if (curr->slot == slot) return true; // Already exists
        curr = curr->next;
    }

    root_entry_t* entry = (root_entry_t*)malloc(sizeof(root_entry_t));
    if (!entry) return false;

    entry->slot = slot;
    entry->next = reg->head;
    reg->head = entry;
    reg->count++;
    return true;
}

bool roots_registry_remove(roots_registry_t* reg, void** slot) {
    if (!reg || !slot) return false;

    root_entry_t* prev = NULL;
    root_entry_t* curr = reg->head;

    while (curr) {
        if (curr->slot == slot) {
            if (prev) {
                prev->next = curr->next;
            } else {
                reg->head = curr->next;
            }
            free(curr);
            reg->count--;
            return true;
        }
        prev = curr;
        curr = curr->next;
    }

    return false;
}

void roots_registry_clear(roots_registry_t* reg) {
    if (!reg) return;

    root_entry_t* curr = reg->head;
    while (curr) {
        root_entry_t* next = curr->next;
        free(curr);
        curr = next;
    }

    reg->head = NULL;
    reg->count = 0;
}

void roots_registry_iterate(roots_registry_t* reg, root_callback_fn callback, void* ctx) {
    if (!reg || !callback) return;

    root_entry_t* curr = reg->head;
    while (curr) {
        callback(curr->slot, ctx);
        curr = curr->next;
    }
}
