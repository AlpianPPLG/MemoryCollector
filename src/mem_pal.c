#include "../include/mem_pal.h"
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

size_t mem_pal_page_size(void) {
    static size_t page_sz = 0;
    if (page_sz == 0) {
        SYSTEM_INFO sys_info;
        GetSystemInfo(&sys_info);
        page_sz = (size_t)sys_info.dwPageSize;
    }
    return page_sz;
}

size_t mem_pal_align_page(size_t size) {
    size_t ps = mem_pal_page_size();
    return (size + ps - 1) & ~(ps - 1);
}

void* mem_pal_alloc_pages(size_t size, mem_prot_t prot) {
    size_t aligned_size = mem_pal_align_page(size);
    DWORD win_prot = 0;

    if ((prot & MEM_PROT_READWRITE) == MEM_PROT_READWRITE) {
        win_prot = PAGE_READWRITE;
    } else if (prot & MEM_PROT_READ) {
        win_prot = PAGE_READONLY;
    } else if (prot & MEM_PROT_EXEC) {
        win_prot = PAGE_EXECUTE_READWRITE;
    } else {
        win_prot = PAGE_NOACCESS;
    }

    void* ptr = VirtualAlloc(NULL, aligned_size, MEM_RESERVE | MEM_COMMIT, win_prot);
    return ptr;
}

bool mem_pal_free_pages(void* base, size_t size) {
    (void)size; // VirtualFree with MEM_RELEASE requires dwSize to be 0
    if (!base) return true;
    return VirtualFree(base, 0, MEM_RELEASE) != 0;
}

bool mem_pal_protect_pages(void* addr, size_t size, mem_prot_t prot) {
    DWORD win_prot = 0;
    DWORD old_prot = 0;

    if ((prot & MEM_PROT_READWRITE) == MEM_PROT_READWRITE) {
        win_prot = PAGE_READWRITE;
    } else if (prot & MEM_PROT_READ) {
        win_prot = PAGE_READONLY;
    } else if (prot == MEM_PROT_NONE) {
        win_prot = PAGE_NOACCESS;
    } else {
        win_prot = PAGE_READWRITE;
    }

    return VirtualProtect(addr, size, win_prot, &old_prot) != 0;
}

#else
// POSIX Implementation (mmap/munmap/mprotect)
#include <unistd.h>
#include <sys/mman.h>

size_t mem_pal_page_size(void) {
    return (size_t)sysconf(_SC_PAGESIZE);
}

size_t mem_pal_align_page(size_t size) {
    size_t ps = mem_pal_page_size();
    return (size + ps - 1) & ~(ps - 1);
}

void* mem_pal_alloc_pages(size_t size, mem_prot_t prot) {
    size_t aligned_size = mem_pal_align_page(size);
    int pflags = PROT_NONE;
    if (prot & MEM_PROT_READ) pflags |= PROT_READ;
    if (prot & MEM_PROT_WRITE) pflags |= PROT_WRITE;
    if (prot & MEM_PROT_EXEC) pflags |= PROT_EXEC;

    void* ptr = mmap(NULL, aligned_size, pflags, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ptr == MAP_FAILED) return NULL;
    return ptr;
}

bool mem_pal_free_pages(void* base, size_t size) {
    if (!base) return true;
    size_t aligned_size = mem_pal_align_page(size);
    return munmap(base, aligned_size) == 0;
}

bool mem_pal_protect_pages(void* addr, size_t size, mem_prot_t prot) {
    int pflags = PROT_NONE;
    if (prot & MEM_PROT_READ) pflags |= PROT_READ;
    if (prot & MEM_PROT_WRITE) pflags |= PROT_WRITE;
    if (prot & MEM_PROT_EXEC) pflags |= PROT_EXEC;

    return mprotect(addr, size, pflags) == 0;
}

#endif
