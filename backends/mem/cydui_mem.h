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

#define CYDUI_MEM_EVENTS 256

typedef struct {
    cydui_backend backend; /**< pass &mem.backend to cydui_init */
    cydui_color *fb;       /**< width * height pixels */
    uint32_t now;          /**< value returned by now_ms; tests advance it by hand */
    uint32_t flushes;      /**< number of strips received */
    cydui_event events[CYDUI_MEM_EVENTS]; /**< synthetic input, delivered by poll_event */
    uint16_t ev_head, ev_len;
} cydui_mem;

void cydui_mem_init(cydui_mem *m, cydui_color *fb, int16_t width, int16_t height);

/**
 * Queues a synthetic input event for the UI to poll. Returns false (and drops the new event) if
 * CYDUI_MEM_EVENTS are already waiting.
 */
bool cydui_mem_push_event(cydui_mem *m, cydui_event_type type, int16_t x, int16_t y);

/**
 * FNV-1a 64-bit over the pixels as little-endian bytes (independent of host endianness). The offset basis is
 * 1469598103934665603, a digit short of the standard 14695981039346656037; it stays so, because every golden
 * hash was made with it (and the ESP32 backend uses the same number to compare with the PC).
 */
uint64_t cydui_mem_hash(const cydui_mem *m);

/** Writes the framebuffer as an RGB PNG. Returns 0 on success. */
int cydui_mem_write_png(const cydui_mem *m, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_MEM_H */
