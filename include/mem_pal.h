#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file mem_pal.h
 * @brief Platform Abstraction Layer (PAL) for low-level virtual memory operations.
 */

// Memory protection flags
typedef enum {
    MEM_PROT_NONE      = 0,
    MEM_PROT_READ      = (1 << 0),
    MEM_PROT_WRITE     = (1 << 1),
    MEM_PROT_EXEC      = (1 << 2),
    MEM_PROT_READWRITE = (MEM_PROT_READ | MEM_PROT_WRITE)
} mem_prot_t;

/**
 * @brief Get the host operating system's page size (typically 4096 bytes).
 */
size_t mem_pal_page_size(void);

/**
 * @brief Round a size up to the nearest multiple of the OS page size.
 */
size_t mem_pal_align_page(size_t size);

/**
 * @brief Allocate pages directly from the OS virtual memory manager.
 * 
 * On Windows: Uses VirtualAlloc(MEM_RESERVE | MEM_COMMIT)
 * On POSIX: Uses mmap(MAP_ANONYMOUS | MAP_PRIVATE)
 * 
 * @param size Total bytes to allocate (will be aligned to page size).
 * @param prot Protection flags (e.g. MEM_PROT_READWRITE).
 * @return void* Pointer to the base of the allocated region, or NULL on failure.
 */
void* mem_pal_alloc_pages(size_t size, mem_prot_t prot);

/**
 * @brief Release allocated pages back to the operating system kernel.
 * 
 * @param base Base address returned by mem_pal_alloc_pages.
 * @param size Size allocated in bytes.
 * @return true on success, false on error.
 */
bool mem_pal_free_pages(void* base, size_t size);

/**
 * @brief Alter protection on an existing range of virtual memory pages (e.g., for Guard Pages).
 * 
 * @param addr Address to protect.
 * @param size Size in bytes.
 * @param prot Target protection flags.
 * @return true on success, false on error.
 */
bool mem_pal_protect_pages(void* addr, size_t size, mem_prot_t prot);

#ifdef __cplusplus
}
#endif
