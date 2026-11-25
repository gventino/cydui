#ifndef CYDUI_H
#define CYDUI_H

#include "cydui/arena.h"
#include "cydui/backend.h"
#include "cydui/cmd.h"
#include "cydui/font.h"
#include "cydui/text.h"
#include "cydui/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The version of the library. Until 1.0 a new MINOR may change the API (docs/11-api.md says how that
 * is announced); a new PATCH never does.
 */
#define CYDUI_VERSION_MAJOR 0
#define CYDUI_VERSION_MINOR 4
#define CYDUI_VERSION_PATCH 0
#define CYDUI_VERSION_STRING "0.4.0"

/** Returns the library version as a static "MAJOR.MINOR.PATCH" string (CYDUI_VERSION_STRING). */
const char *cydui_version(void);

/** Everything cydui_init needs; all memory is provided by the caller. */
typedef struct {
    const cydui_backend *backend;
    void *cmd_mem;      /**< memory for the command list */
    size_t cmd_mem_size;
    void *strip_mem;    /**< memory for the render strip, at least width * 2 bytes (>= 1 row) */
    size_t strip_mem_size;
    void *text_mem;     /**< optional: copies of this frame's strings; without it text is dropped */
    size_t text_mem_size;
} cydui_config;

/** Library state. Fields are private; use the functions below. */
typedef struct {
    const cydui_backend *backend;
    cydui_cmd *cmds;
    uint32_t cmd_cap, cmd_len;
    cydui_color *strip;
    int16_t strip_h;
    cydui_arena text_arena;
    cydui_rect clip;
    cydui_color clear;
    uint32_t errors;
} cydui_ctx;

/** Returns 0 on success, or CYDUI_ERR_NO_MEMORY / CYDUI_ERR_BAD_ARG. Does not allocate. */
uint32_t cydui_init(cydui_ctx *ctx, const cydui_config *cfg);

/** Starts a frame: empties the command list, resets the clip and sets the background color. */
void cydui_begin_frame(cydui_ctx *ctx, cydui_color clear);

/** Rasterizes the frame strip by strip and hands each strip to the backend. */
void cydui_end_frame(cydui_ctx *ctx);

/** Accumulated CYDUI_ERR_* flags since the last cydui_clear_errors(). */
uint32_t cydui_errors(const cydui_ctx *ctx);
void cydui_clear_errors(cydui_ctx *ctx);

/** Number of commands recorded in the current frame. */
uint32_t cydui_cmd_count(const cydui_ctx *ctx);

/** Height in rows of the render strip chosen at init. */
int16_t cydui_strip_height(const cydui_ctx *ctx);

/** Restricts following commands to `r` (intersected with the screen). */
void cydui_set_clip(cydui_ctx *ctx, cydui_rect r);
void cydui_reset_clip(cydui_ctx *ctx);

/* ---- drawing. alpha: 0..255, 255 = opaque. Commands wholly outside the clip are dropped. ---- */

void cydui_draw_rect(cydui_ctx *ctx, cydui_rect r, cydui_color color, uint8_t alpha);

/** Antialiased rounded rectangle; radius is clamped to half the smaller side (and to 255). */
void cydui_draw_rrect(cydui_ctx *ctx, cydui_rect r, uint8_t radius, cydui_color color,
                      uint8_t alpha);

/** Outline of a rounded rectangle, `thickness` pixels wide, drawn inside `r`. */
void cydui_draw_rrect_outline(cydui_ctx *ctx, cydui_rect r, uint8_t radius, uint8_t thickness,
                              cydui_color color, uint8_t alpha);

/** Antialiased line with round caps; thickness in pixels (>= 1). Endpoints are pixel centers. */
void cydui_draw_line(cydui_ctx *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     uint8_t thickness, cydui_color color, uint8_t alpha);

/**
 * Draws a bitmap at r.x, r.y with size r.w x r.h. `len` is the size of `data` in bytes and must
 * cover the whole bitmap, otherwise the command is dropped with CYDUI_ERR_BAD_BITMAP. `data` must
 * stay valid until cydui_end_frame(). `tint` is used by CYDUI_BITMAP_A4 only.
 */
void cydui_draw_bitmap(cydui_ctx *ctx, cydui_rect r, const void *data, size_t len,
                       cydui_bitmap_format fmt, cydui_color tint, uint8_t alpha);

/**
 * Draws UTF-8 text with its top-left at (x, y). `len` is the byte length or CYDUI_NTS for a
 * NUL-terminated string; at most CYDUI_MAX_STR bytes are used (CYDUI_ERR_TEXT_TRUNCATED otherwise).
 * The string is copied, so `s` may be a temporary buffer. With wrap_w > 0 the text wraps inside a
 * box of that width and `align` positions each line in it; with wrap_w <= 0 there is no wrapping
 * and `align` only matters between lines of text containing '\n'. Text needs `text_mem` in the
 * config: without it, or when it is full, the text is dropped with CYDUI_ERR_NO_MEMORY.
 */
void cydui_draw_text(cydui_ctx *ctx, const cydui_font *font, int16_t x, int16_t y, int16_t wrap_w,
                     cydui_align align, const char *s, size_t len, cydui_color color,
                     uint8_t alpha);

/** Two-color linear gradient; vertical: c0 at the top, c1 at the bottom, else left to right. */
void cydui_draw_gradient(cydui_ctx *ctx, cydui_rect r, cydui_color c0, cydui_color c1,
                         bool vertical, uint8_t alpha);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_H */
