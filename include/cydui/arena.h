#ifndef CYDUI_ARENA_H
#define CYDUI_ARENA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Bump allocator over caller-provided memory. The library never calls malloc. */
typedef struct {
    uint8_t *base;
    size_t cap;
    size_t used;
} cydui_arena;

/** Initializes an arena over `mem` (may be NULL if cap is 0). */
void cydui_arena_init(cydui_arena *a, void *mem, size_t cap);

/**
 * Allocates `size` bytes aligned to `align` (a power of two). Returns NULL, leaving the
 * arena untouched, if the request does not fit or the arguments are invalid.
 */
void *cydui_arena_alloc(cydui_arena *a, size_t size, size_t align);

/** Frees everything at once. */
void cydui_arena_reset(cydui_arena *a);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_ARENA_H */
