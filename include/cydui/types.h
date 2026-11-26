#ifndef CYDUI_TYPES_H
#define CYDUI_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** RGB565 pixel in native byte order. Byte swapping for a panel is the backend's job. */
typedef uint16_t cydui_color;

/** Packs 8-bit r, g, b into RGB565. */
#define CYDUI_RGB(r, g, b)                                                     \
    ((cydui_color)((((unsigned)(r) & 0xF8u) << 8) | (((unsigned)(g) & 0xFCu) << 3) | \
                   (((unsigned)(b) & 0xFFu) >> 3)))

/** Axis-aligned rectangle in pixels. A rectangle with w <= 0 or h <= 0 is empty. */
typedef struct {
    int16_t x, y, w, h;
} cydui_rect;

/** Error flags accumulated in a context; see cydui_errors(). */
enum {
    CYDUI_ERR_CMD_OVERFLOW = 1u << 0, /**< command list full, command dropped */
    CYDUI_ERR_BAD_ARG = 1u << 1,      /**< invalid argument, command dropped */
    CYDUI_ERR_NO_MEMORY = 1u << 2,    /**< caller-provided memory too small */
    CYDUI_ERR_BAD_BITMAP = 1u << 3,   /**< bitmap data shorter than its dimensions, dropped */
    CYDUI_ERR_TEXT_TRUNCATED = 1u << 4, /**< text longer than CYDUI_MAX_STR was cut; informative */
    CYDUI_ERR_ID_COLLISION = 1u << 5,   /**< two widgets got the same id in one frame (debug builds) */
    CYDUI_ERR_LAYOUT = 1u << 6,         /**< container/id nesting error: too deep, unbalanced, ... */
    CYDUI_ERR_TOO_MANY_WIDGETS = 1u << 7, /**< more than CYDUI_MAX_WIDGETS in a frame; extras ignored */
    CYDUI_ERR_THREAD = 1u << 8,         /**< UI called from a task other than the one that created it */
    CYDUI_ERR_FRAME_BUDGET = 1u << 9,   /**< the frame took longer than its time budget; the rest of its widgets were skipped */
    CYDUI_ERR_TWEEN_LIMIT = 1u << 10,   /**< more than CYDUI_MAX_TWEENS tweens running at once; the extra ones jumped to their target */
    CYDUI_ERR_STATE_FULL = 1u << 11     /**< the state table was full and an entry that was still in use (looked up in this or the previous frame) had to be replaced, or none could be made; a table of no entries is a choice and a tween that finds no room just jumps to its target, neither raises it; see cydui_ui_stats */
};

/** Intersection of two rectangles; an empty result has w == h == 0. */
cydui_rect cydui_rect_intersect(cydui_rect a, cydui_rect b);

/** True if the rectangle covers no pixels. */
bool cydui_rect_empty(cydui_rect r);

/**
 * Blends src over dst with alpha 0..255 (255 = src only).
 * Integer only; granularity is 1/32 per channel.
 */
cydui_color cydui_blend(cydui_color dst, cydui_color src, uint8_t alpha);

/** Expands an RGB565 pixel to 8-bit channels (low bits replicated). */
void cydui_color_unpack(cydui_color c, uint8_t *r, uint8_t *g, uint8_t *b);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_TYPES_H */
