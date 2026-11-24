#ifndef CYDUI_TEXT_H
#define CYDUI_TEXT_H

/*
 * Text layout. Applications use cydui_text_size() and cydui_draw_text() from cydui.h; the iterator
 * below is what both the command recorder and the rasterizer share, so that measuring and drawing
 * always agree.
 */

#include <stdbool.h>

#include "cydui/font.h"
#include "cydui/types.h"
#include "cydui/utf8.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Longest text taken into account, in bytes (CT-02). Longer text is truncated. */
#ifndef CYDUI_MAX_STR
#define CYDUI_MAX_STR 256
#endif

/** Render bidirectional formatting characters as the fallback box (CT-04). 0 disables it. */
#ifndef CYDUI_FILTER_CONTROL
#define CYDUI_FILTER_CONTROL 1
#endif

/**
 * How far, in pixels, a glyph's ink can lie from the line box that contains it, for any font:
 * bearings are int8 and bitmaps at most 255 rows. Culling uses it so that skipping a command or a
 * line can never change the image, whatever the font contains.
 */
#define CYDUI_TEXT_REACH(font) ((int)(font)->ascent + 383)

/** Pass as `len` for a NUL-terminated string. The search for the NUL stops at CYDUI_MAX_STR. */
#define CYDUI_NTS ((size_t)-1)

typedef enum {
    CYDUI_ALIGN_LEFT,
    CYDUI_ALIGN_CENTER,
    CYDUI_ALIGN_RIGHT
} cydui_align;

/** Size of a piece of text. `lines` is 0 for empty text. */
typedef struct {
    int16_t w, h;
    uint16_t lines;
} cydui_text_extent;

/**
 * How many bytes of `s` will be used: at most CYDUI_MAX_STR, cut on a code point boundary, and for
 * CYDUI_NTS up to the NUL. Sets *truncated if text was dropped. Never reads past the returned
 * length (or past the first CYDUI_MAX_STR bytes for CYDUI_NTS).
 */
size_t cydui_text_clip_len(const char *s, size_t len, bool *truncated);

/**
 * The glyph to draw for `cp`, or NULL if it draws nothing (carriage return, or an unusable font).
 * Control characters and, with CYDUI_FILTER_CONTROL, bidi formatting characters give the fallback
 * box even if the font has a glyph for them.
 */
const cydui_glyph *cydui_text_resolve(const cydui_font *f, uint32_t cp);

typedef struct {
    const cydui_font *font;
    const char *s;
    size_t len;
    int wrap; /* wrap width in pixels, 0 = no wrapping */
    size_t pos;
} cydui_text_iter;

typedef struct {
    size_t start, end; /* the line's bytes, s[start..end); the newline and wrapped spaces excluded */
    int width;         /* pixels, up to the last visible glyph (trailing spaces do not count) */
} cydui_text_line;

/** `len` must already be limited with cydui_text_clip_len(). */
void cydui_text_iter_init(cydui_text_iter *it, const cydui_font *f, const char *s, size_t len,
                          int16_t wrap_w);

/**
 * Produces the next line, or returns false at the end. Lines break at '\n', at spaces when the
 * text would pass the wrap width, and inside a word that is wider than the wrap width. Every line
 * consumes at least one byte, so a loop over it always terminates.
 */
bool cydui_text_next_line(cydui_text_iter *it, cydui_text_line *line);

/** Size of `s` set in `f`; wrap_w <= 0 means no wrapping. Applies the CYDUI_MAX_STR limit. */
cydui_text_extent cydui_text_size(const cydui_font *f, const char *s, size_t len, int16_t wrap_w);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_TEXT_H */
