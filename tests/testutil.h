#ifndef TESTUTIL_H
#define TESTUTIL_H

#include "cydui/cydui.h"
#include "cydui_mem.h"

#define TU_W 320
#define TU_H 240

typedef void (*tu_scene)(cydui_ctx *ctx);

/** Framebuffer of the last tu_render() call. */
extern cydui_color tu_fb[TU_W * TU_H];

/** CYDUI_ERR_* flags raised during the last tu_render() call. */
extern uint32_t tu_errors;

/**
 * Renders one frame of `scene` on a 320x240 screen using strips of `strip_rows` rows (1..240)
 * and returns the framebuffer hash. The framebuffer stays in tu_fb.
 */
uint64_t tu_render(tu_scene scene, int strip_rows, cydui_color clear);

static inline cydui_color tu_px(int x, int y)
{
    return tu_fb[y * TU_W + x];
}

/* Shorthand for building rectangles. */
static inline cydui_rect tu_rect(int x, int y, int w, int h)
{
    cydui_rect r;
    r.x = (int16_t)x;
    r.y = (int16_t)y;
    r.w = (int16_t)w;
    r.h = (int16_t)h;
    return r;
}

#endif /* TESTUTIL_H */
