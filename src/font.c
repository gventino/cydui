#include "cydui/font.h"

size_t cydui_glyph_bytes(const cydui_glyph *g)
{
    return (((size_t)g->w + 1u) / 2u) * (size_t)g->h;
}

/* True if the glyph's bitmap fits in the font's bitmap array. No overflow for any offset. */
static int bitmap_in_range(const cydui_font *f, const cydui_glyph *g)
{
    size_t need = cydui_glyph_bytes(g);
    if (need == 0) {
        return 1;
    }
    return f->bitmap != NULL && (size_t)g->offset <= (size_t)f->bitmap_len &&
           need <= (size_t)f->bitmap_len - (size_t)g->offset;
}

cydui_font_status cydui_font_validate(const cydui_font *f)
{
    uint32_t i;

    if (!f || !f->glyphs) {
        return CYDUI_FONT_ERR_NULL;
    }
    if (f->glyph_count == 0) {
        return CYDUI_FONT_ERR_EMPTY;
    }
    if (f->line_height == 0) {
        return CYDUI_FONT_ERR_METRICS;
    }
    if (f->fallback >= f->glyph_count) {
        return CYDUI_FONT_ERR_FALLBACK;
    }
    for (i = 0; i < f->glyph_count; i++) {
        if (i > 0 && f->glyphs[i].cp <= f->glyphs[i - 1].cp) {
            return CYDUI_FONT_ERR_ORDER;
        }
        if (!bitmap_in_range(f, &f->glyphs[i])) {
            return CYDUI_FONT_ERR_BITMAP;
        }
    }
    return CYDUI_FONT_OK;
}

const cydui_glyph *cydui_font_fallback(const cydui_font *f)
{
    if (!f || !f->glyphs || f->fallback >= f->glyph_count) {
        return NULL;
    }
    return &f->glyphs[f->fallback];
}

const cydui_glyph *cydui_font_lookup(const cydui_font *f, uint32_t cp)
{
    uint32_t lo = 0, hi;

    if (!f || !f->glyphs || f->glyph_count == 0 || f->fallback >= f->glyph_count) {
        return NULL;
    }
    hi = f->glyph_count; /* search [lo, hi) */
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        uint32_t c = f->glyphs[mid].cp;
        if (c == cp) {
            return &f->glyphs[mid];
        }
        if (c < cp) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return &f->glyphs[f->fallback];
}

const uint8_t *cydui_font_glyph_data(const cydui_font *f, const cydui_glyph *g)
{
    if (!f || !g || cydui_glyph_bytes(g) == 0 || !bitmap_in_range(f, g)) {
        return NULL;
    }
    return f->bitmap + g->offset;
}
