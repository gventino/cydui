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
    CYDUI_ERR_TEXT_TRUNCATED = 1u << 4 /**< text longer than CYDUI_MAX_STR was cut; informative */
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
