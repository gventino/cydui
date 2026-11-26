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
 * The version of the library, semantic versioning: a new MINOR only adds to the API, a new PATCH only fixes
 * (docs/11-api.md says how a change of the API is announced). The numbers are those of the release; the string
 * also carries the pre-release suffix of a release candidate ("1.0.0-rc.1"); a release has none ("1.0.0").
 */
#define CYDUI_VERSION_MAJOR 1
#define CYDUI_VERSION_MINOR 0
#define CYDUI_VERSION_PATCH 0
#define CYDUI_VERSION_STRING "1.0.0"

/** Returns the library version as a static string, "MAJOR.MINOR.PATCH" with an optional "-suffix" (CYDUI_VERSION_STRING). */
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
    /**
     * How many strip buffers the memory above holds: 0 or 1 (one strip, as always) or 2 (two equal
     * strips that the core uses in turn, so that the backend can send one while the next is drawn;
     * see flush_strip in backend.h). Anything else is CYDUI_ERR_BAD_ARG.
     */
    uint8_t strip_buffers;
    /**
     * Optional: memory for the dirty regions, one uint64_t per strip (ceil(height / strip height)
     * of them). With it, a strip whose commands are the same as in the previous frame is neither
     * rendered nor sent. Too small: CYDUI_ERR_NO_MEMORY. The bitmap data that a command points at
     * must not change in place (change the pointer instead): only the pointer is compared.
     */
    void *dirty_mem;
    size_t dirty_mem_size;
} cydui_config;

/**
 * A post-effect (FR-07): changes every strip after it is rasterized and before it is sent, for what a
 * command list cannot say (scanlines, a glitch, a tint). Both functions are called with the strip's rows
 * in screen coordinates, so the effect must depend on `y0 + row` and not on the strip it is in (the result
 * must not depend on how the screen is cut into strips, R-08), and be integer only.
 *
 *  - `key` says what the effect will do to the rows [y0, y0 + h): the same number means the same pixels.
 *    It is part of what decides if a strip is sent again (dirty regions), so an effect that moves with
 *    time or state returns a different number when it does, and one that does nothing to those rows (or
 *    never changes) returns a constant. NULL: the effect is constant.
 *  - `apply` changes the `h` rows of `width` pixels in `buf`.
 */
typedef struct {
    uint32_t (*key)(void *user, int16_t y0, int16_t h);
    void (*apply)(void *user, cydui_color *buf, int16_t width, int16_t y0, int16_t h);
    void *user;
} cydui_post_effect;

/** Library state. Fields are private; use the functions below. */
typedef struct {
    const cydui_backend *backend;
    cydui_cmd *cmds;
    uint32_t cmd_cap, cmd_len;
    cydui_color *strip;
    int16_t strip_h;
    size_t strip_pixels;       /**< pixels in one strip buffer */
    uint8_t strip_buffers, strip_next;
    uint64_t *dirty;           /**< NULL: every strip is sent every frame */
    uint32_t dirty_cap;
    bool dirty_primed;
    cydui_arena text_arena;
    cydui_rect clip;
    cydui_color clear;
    cydui_post_effect post; /**< `apply` NULL: none */
    uint32_t errors;
} cydui_ctx;

/** Returns 0 on success, or CYDUI_ERR_NO_MEMORY / CYDUI_ERR_BAD_ARG. Does not allocate. */
uint32_t cydui_init(cydui_ctx *ctx, const cydui_config *cfg);

/** Starts a frame: empties the command list, resets the clip and sets the background color. */
void cydui_begin_frame(cydui_ctx *ctx, cydui_color clear);

/**
 * Rasterizes the frame strip by strip and hands each strip to the backend (only the strips that
 * changed since the previous frame, when dirty regions are on).
 */
void cydui_end_frame(cydui_ctx *ctx);

/**
 * Sets the post-effect (a copy is kept), or removes it with NULL or an effect without `apply`. The next
 * frame sends every strip again.
 */
void cydui_set_post_effect(cydui_ctx *ctx, const cydui_post_effect *fx);

/** Makes the next frame send every strip again (after the panel was reset, say). Harmless otherwise. */
void cydui_invalidate(cydui_ctx *ctx);

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
 * stay valid until cydui_end_frame(). `tint` is used by CYDUI_BITMAP_A4 and CYDUI_BITMAP_A1 only
 * (an A1 bitmap drawn here has a scale of 1). A bitmap that is drawn again in the same place with other
 * pixels but the same address and size looks unchanged to the dirty regions (they compare the address): an
 * application that does that calls cydui_invalidate().
 */
void cydui_draw_bitmap(cydui_ctx *ctx, cydui_rect r, const void *data, size_t len,
                       cydui_bitmap_format fmt, cydui_color tint, uint8_t alpha);

/**
 * Draws a 1 bit per pixel mask of w x h bits (CYDUI_BITMAP_A1: rows padded to a byte, most significant bit
 * first, 1 = the tint) with its top-left at (x, y), every bit as a scale x scale square of pixels, so it
 * covers w * scale x h * scale pixels. `scale` is 1 to 8, w and h are positive and the scaled size fits an
 * int16, else the command is dropped with CYDUI_ERR_BAD_ARG; `len` must cover the whole mask, else
 * CYDUI_ERR_BAD_BITMAP. `data` must stay valid until cydui_end_frame(). A QR code of 33 modules at scale 4
 * is 140 bytes of mask instead of the 17 KB of an A4 mask of its drawn size.
 */
void cydui_draw_bitmap_a1(cydui_ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t scale, const void *data,
                          size_t len, cydui_color tint, uint8_t alpha);

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
