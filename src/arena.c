#include "cydui/arena.h"

void cydui_arena_init(cydui_arena *a, void *mem, size_t cap)
{
    a->base = (uint8_t *)mem;
    a->cap = mem ? cap : 0;
    a->used = 0;
}

void *cydui_arena_alloc(cydui_arena *a, size_t size, size_t align)
{
    if (align == 0 || (align & (align - 1)) != 0 || size == 0) {
        return NULL;
    }

    /* Align the address, not the offset, so any base pointer works. */
    uintptr_t addr = (uintptr_t)a->base + a->used;
    size_t pad = (size_t)(((addr + align - 1) & ~(uintptr_t)(align - 1)) - addr);

    if (pad > a->cap - a->used || size > a->cap - a->used - pad) {
        return NULL;
    }
    void *p = a->base + a->used + pad;
    a->used += pad + size;
    return p;
}

void cydui_arena_reset(cydui_arena *a)
{
    a->used = 0;
}
