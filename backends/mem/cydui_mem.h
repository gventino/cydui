#ifndef CYDUI_MEM_H
#define CYDUI_MEM_H

/*
 * In-memory framebuffer backend. No dependencies: used by tests and by headless runs to hash or
 * dump what the core renders. The framebuffer is provided by the caller.
 */

#include "cydui/backend.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    cydui_backend backend; /**< pass &mem.backend to cydui_init */
    cydui_color *fb;       /**< width * height pixels */
    uint32_t now;          /**< value returned by now_ms; tests advance it by hand */
    uint32_t flushes;      /**< number of strips received */
} cydui_mem;

void cydui_mem_init(cydui_mem *m, cydui_color *fb, int16_t width, int16_t height);

/** FNV-1a 64-bit over the pixels as little-endian bytes (independent of host endianness). */
uint64_t cydui_mem_hash(const cydui_mem *m);

/** Writes the framebuffer as an RGB PNG. Returns 0 on success. */
int cydui_mem_write_png(const cydui_mem *m, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_MEM_H */
