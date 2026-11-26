#ifndef CYDUI_CMD_H
#define CYDUI_CMD_H

#include "cydui/font.h"
#include "cydui/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CYDUI_CMD_RECT,
    CYDUI_CMD_RRECT,
    CYDUI_CMD_LINE,
    CYDUI_CMD_BITMAP,
    CYDUI_CMD_GRADIENT,
    CYDUI_CMD_TEXT
} cydui_cmd_type;

typedef enum {
    CYDUI_BITMAP_RGB565, /**< opaque pixels, 2 bytes each, native order, row-major */
    CYDUI_BITMAP_A4,     /**< 4-bit coverage mask tinted with a color; rows padded to a byte */
    CYDUI_BITMAP_A1      /**< 1 bit per pixel, tinted with a color: 1 draws the tint, 0 draws nothing; the leftmost pixel is the
                              most significant bit of a byte, and rows are padded to a byte. With cydui_draw_bitmap_a1 every
                              bit can be scaled up by a whole number of pixels, so a QR code needs no mask of its own size */
} cydui_bitmap_format;

/**
 * One draw command. `rect` is the geometry (for lines, the stroke's bounding box); `clip` is
 * the clip rectangle in effect when the command was emitted. Commands are rasterized in order.
 */
typedef struct {
    uint8_t type;
    uint8_t alpha;
    cydui_rect rect;
    cydui_rect clip;
    union {
        struct {
            cydui_color color;
        } rect;
        struct {
            cydui_color color;
            uint8_t radius;    /**< pixels, clamped to half the smaller side */
            uint8_t thickness; /**< 0 = filled, otherwise outline thickness inside the rect */
        } rrect;
        struct {
            int16_t x0, y0, x1, y1;
            cydui_color color;
            uint8_t thickness; /**< pixels, >= 1 */
        } line;
        struct {
            const void *data;
            cydui_color tint; /**< A4 and A1 only */
            uint8_t format;
            uint8_t scale;    /**< A1 only: each bit is scale x scale pixels (1 for the others); rect.w and rect.h are the scaled size */
        } bitmap;
        struct {
            cydui_color c0, c1;
            uint8_t vertical; /**< 1: c0 at top to c1 at bottom; 0: left to right */
        } gradient;
        struct {
            const cydui_font *font;
            const char *str;   /**< a copy owned by the frame's text arena */
            uint16_t len;      /**< bytes, at most CYDUI_MAX_STR */
            int16_t wrap;      /**< wrap width in pixels, 0 = no wrapping */
            cydui_color color;
            uint8_t align;     /**< cydui_align */
        } text;
    } u;
} cydui_cmd;

/** Bytes a bitmap of w x h pixels needs in the given format; 0 for an invalid format. */
size_t cydui_bitmap_size(cydui_bitmap_format fmt, int16_t w, int16_t h);

/**
 * Rasterizes `n` commands into one strip: `buf` holds `h` rows of `width` pixels covering screen
 * rows y0 .. y0+h-1. The strip is first filled with `clear`. The result for any pixel does not
 * depend on how the screen is split into strips.
 */
void cydui_raster_strip(const cydui_cmd *cmds, uint32_t n, cydui_color *buf, int16_t width,
                        int16_t y0, int16_t h, cydui_color clear);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_CMD_H */
