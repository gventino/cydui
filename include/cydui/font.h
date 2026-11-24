#ifndef CYDUI_FONT_H
#define CYDUI_FONT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * One glyph. The bitmap is 4-bit coverage (A4): w x h pixels, high nibble first, each row padded
 * to a whole byte, starting at `offset` in the font's bitmap data. `left` is the distance from the
 * pen position to the bitmap's left edge, `top` the distance from the baseline up to its top edge.
 */
typedef struct {
    uint32_t cp;      /**< Unicode code point */
    uint32_t offset;  /**< start of the bitmap in cydui_font.bitmap */
    uint8_t w, h;     /**< bitmap size in pixels (0 for blank glyphs such as the space) */
    int8_t left, top;
    uint8_t advance;  /**< pen movement in pixels */
} cydui_glyph;

/**
 * A bitmap font: constant data produced by tools/fontgen.py. The glyph table is sorted by `cp`
 * without duplicates. `fallback` is the index of the glyph drawn for code points the font lacks
 * (the generator stores a box there, under U+FFFD).
 */
typedef struct {
    const cydui_glyph *glyphs;
    uint16_t glyph_count;
    const uint8_t *bitmap;
    uint32_t bitmap_len;
    uint8_t line_height; /**< distance between baselines */
    uint8_t ascent;      /**< baseline offset from the top of a line */
    uint16_t fallback;
} cydui_font;

typedef enum {
    CYDUI_FONT_OK = 0,
    CYDUI_FONT_ERR_NULL,      /**< font or a required pointer is NULL */
    CYDUI_FONT_ERR_EMPTY,     /**< no glyphs */
    CYDUI_FONT_ERR_ORDER,     /**< glyph table not strictly ascending by code point */
    CYDUI_FONT_ERR_BITMAP,    /**< a glyph's bitmap lies outside the bitmap data */
    CYDUI_FONT_ERR_FALLBACK,  /**< fallback index out of range */
    CYDUI_FONT_ERR_METRICS    /**< zero line height */
} cydui_font_status;

/** Checks a font completely. Cost is linear in the glyph count; use it at startup and in tests. */
cydui_font_status cydui_font_validate(const cydui_font *f);

/**
 * Finds the glyph for `cp` by binary search; if the font lacks it, returns the fallback glyph.
 * Returns NULL only if the font is unusable (NULL, no glyphs, or fallback out of range).
 * Safe on any font, valid or not.
 */
const cydui_glyph *cydui_font_lookup(const cydui_font *f, uint32_t cp);

/** Bytes of bitmap data a glyph needs: ((w + 1) / 2) * h. */
size_t cydui_glyph_bytes(const cydui_glyph *g);

/**
 * Returns the glyph's bitmap, or NULL if the glyph is blank or its data would fall outside the
 * font's bitmap array. Every read of glyph data goes through here, so a corrupt font cannot make
 * the rasterizer read out of bounds.
 */
const uint8_t *cydui_font_glyph_data(const cydui_font *f, const cydui_glyph *g);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_FONT_H */
