/*
 * libFuzzer harness (CT-12). The input is read as a sequence of drawing calls with arbitrary
 * parameters (full int16 coordinates, any radius/thickness/alpha, any bitmap length up to the
 * real buffer size). Text is drawn with arbitrary bytes, wrapping and alignment, and with fonts
 * assembled from the fuzz input itself: glyph tables that are unsorted, with offsets, sizes,
 * bearings and metrics out of range (AB-16). Under ASan/UBSan this looks for out-of-bounds access
 * and undefined behavior, and it checks the core invariant: the image does not depend on the
 * strip height.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/cydui.h"
#include "cydui_mem.h"

#define W 80
#define H 60
#define MAX_OPS 200

typedef struct {
    const uint8_t *p;
    size_t n;
} reader;

static uint8_t u8(reader *r)
{
    if (r->n == 0) {
        return 0;
    }
    r->n--;
    return *r->p++;
}

static int16_t i16(reader *r)
{
    unsigned lo = u8(r);
    unsigned hi = u8(r);
    return (int16_t)(lo | (hi << 8));
}

/*
 * Parameters are mostly small so that shapes land on the 80x60 screen, with an escape to the full
 * int16 range (positions: 0x80, sizes: >= 0xF0) to probe overflow and clipping at the extremes.
 */
static int16_t pos(reader *r)
{
    uint8_t b = u8(r);
    return b == 0x80u ? i16(r) : (int16_t)(int8_t)b;
}

static int16_t dim(reader *r)
{
    uint8_t b = u8(r);
    return b >= 0xF0u ? i16(r) : (int16_t)b;
}

static cydui_rect rect16(reader *r)
{
    cydui_rect q;
    q.x = pos(r);
    q.y = pos(r);
    q.w = dim(r);
    q.h = dim(r);
    return q;
}

static cydui_cmd cmd_mem[64];
static cydui_color strip_mem[W * H];
static cydui_color fb[W * H];
static uint16_t bitmap_mem[256]; /* 512 bytes, aligned for RGB565 */
static char text_mem[2048];

/* A small well-formed font, used by half of the text operations. */
static const uint8_t good_bits[12] = {0xFF, 0xFF, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xFF, 0xFF};
static const cydui_glyph good_glyphs[] = {
    {0x20, 0, 0, 0, 0, 0, 4}, {'a', 0, 4, 6, 0, 6, 5}, {'b', 0, 4, 6, 0, 6, 5},
    {'c', 0, 4, 6, 0, 6, 5},  {'d', 0, 4, 6, 0, 6, 5}, {0xFFFD, 0, 4, 6, 1, 6, 5}};
static const cydui_font good_font = {good_glyphs, 6, good_bits, sizeof(good_bits), 8, 6, 5};

#define FUZZ_GLYPHS 6
static cydui_glyph fuzz_glyphs[FUZZ_GLYPHS];
static uint8_t fuzz_bits[64];
static cydui_font fuzz_font;

/*
 * Builds a font straight from the input. Nothing about it needs to be valid, except that the
 * arrays it points to exist and `bitmap_len` does not claim more than the buffer holds. Half of
 * the fonts use mild values (small glyphs that point into the data, but a tall ascent over a short
 * line, so ink lands far from its line box); the others use the full range of every field.
 */
static const cydui_font *font_from_input(reader *r)
{
    size_t i;
    uint8_t flags = u8(r);
    int wild = (flags & 16u) != 0;
    uint32_t len;

    if (flags & 1u) {
        return &good_font;
    }
    for (i = 0; i < sizeof(fuzz_bits); i++) {
        fuzz_bits[i] = u8(r);
    }
    len = (flags & 8u) ? 0u : 1u + u8(r) % sizeof(fuzz_bits);
    for (i = 0; i < FUZZ_GLYPHS; i++) {
        uint32_t off_lo = u8(r), off_hi = u8(r);
        uint8_t w = u8(r), h = u8(r), adv = u8(r);
        int8_t left = (int8_t)u8(r), top = (int8_t)u8(r);

        fuzz_glyphs[i].cp = (flags & 2u) ? (uint32_t)u8(r) : (uint32_t)(0x20 + i * 3 + (u8(r) & 1u));
        if (wild) {
            fuzz_glyphs[i].offset = (flags & 4u) ? (off_lo | (off_hi << 8) | (off_hi << 24)) : off_lo;
            fuzz_glyphs[i].w = w;
            fuzz_glyphs[i].h = h;
            fuzz_glyphs[i].left = left;
            fuzz_glyphs[i].top = top;
            fuzz_glyphs[i].advance = adv;
        } else {
            fuzz_glyphs[i].offset = len ? off_lo % len : 0;
            fuzz_glyphs[i].w = (uint8_t)(w % 7u);
            fuzz_glyphs[i].h = (uint8_t)(h % 7u);
            fuzz_glyphs[i].left = (int8_t)(left % 12);
            fuzz_glyphs[i].top = (int8_t)(top % 40);   /* up to 39 above or below the baseline */
            fuzz_glyphs[i].advance = (uint8_t)(adv % 9u);
        }
    }
    fuzz_font.glyphs = fuzz_glyphs;
    fuzz_font.glyph_count = (uint16_t)(1u + u8(r) % FUZZ_GLYPHS); /* 1..6: never more than exist */
    fuzz_font.bitmap = (flags & 8u) ? NULL : fuzz_bits;
    fuzz_font.bitmap_len = len;
    if (wild) {
        fuzz_font.line_height = u8(r);
        fuzz_font.ascent = u8(r);
        fuzz_font.fallback = (uint16_t)(u8(r) % 8u);
    } else {
        fuzz_font.line_height = (uint8_t)(1u + u8(r) % 6u);  /* short lines ... */
        fuzz_font.ascent = (uint8_t)(u8(r) % 50u);           /* ... with a tall ascent */
        fuzz_font.fallback = (uint16_t)(u8(r) % fuzz_font.glyph_count);
    }
    return &fuzz_font;
}

/* Renders the call sequence in `data` with strips of `rows` rows; returns the framebuffer hash. */
static uint64_t render(const uint8_t *data, size_t size, int rows)
{
    cydui_mem mem;
    cydui_ctx ctx;
    cydui_config cfg;
    reader r;
    int ops;

    r.p = data;
    r.n = size;
    cydui_mem_init(&mem, fb, W, H);
    memset(&cfg, 0, sizeof(cfg));
    cfg.backend = &mem.backend;
    cfg.cmd_mem = cmd_mem;
    cfg.cmd_mem_size = sizeof(cmd_mem);
    cfg.strip_mem = strip_mem;
    cfg.strip_mem_size = (size_t)rows * W * sizeof(cydui_color);
    cfg.text_mem = text_mem;
    cfg.text_mem_size = sizeof(text_mem);
    if (cydui_init(&ctx, &cfg) != 0) {
        abort();
    }

    cydui_begin_frame(&ctx, (cydui_color)(u8(&r) | (u8(&r) << 8)));
    for (ops = 0; ops < MAX_OPS && r.n > 0; ops++) {
        switch (u8(&r) % 9u) {
        case 0: {
            cydui_rect q = rect16(&r);
            cydui_color c = (cydui_color)(u8(&r) | (u8(&r) << 8));
            cydui_draw_rect(&ctx, q, c, u8(&r));
            break;
        }
        case 1: {
            cydui_rect q = rect16(&r);
            uint8_t radius = u8(&r);
            cydui_color c = (cydui_color)(u8(&r) | (u8(&r) << 8));
            cydui_draw_rrect(&ctx, q, radius, c, u8(&r));
            break;
        }
        case 2: {
            cydui_rect q = rect16(&r);
            uint8_t radius = u8(&r), thickness = u8(&r);
            cydui_color c = (cydui_color)(u8(&r) | (u8(&r) << 8));
            cydui_draw_rrect_outline(&ctx, q, radius, thickness, c, u8(&r));
            break;
        }
        case 3: {
            int16_t x0 = pos(&r), y0 = pos(&r), x1 = pos(&r), y1 = pos(&r);
            uint8_t thickness = u8(&r);
            cydui_color c = (cydui_color)(u8(&r) | (u8(&r) << 8));
            cydui_draw_line(&ctx, x0, y0, x1, y1, thickness, c, u8(&r));
            break;
        }
        case 4: {
            cydui_rect q = rect16(&r);
            size_t len = (size_t)u8(&r) * 2u; /* 0..510, never larger than the real buffer */
            unsigned kind = u8(&r) % 3u;
            cydui_bitmap_format fmt = kind == 0 ? CYDUI_BITMAP_RGB565 : (kind == 1 ? CYDUI_BITMAP_A4 : CYDUI_BITMAP_A1);
            cydui_color tint = (cydui_color)(u8(&r) | (u8(&r) << 8));
            uint8_t seed[8];
            size_t i;
            for (i = 0; i < sizeof(seed); i++) {
                seed[i] = u8(&r);
            }
            for (i = 0; i < 256; i++) { /* a short seed repeated: cheap, but not a flat color */
                bitmap_mem[i] = (uint16_t)(seed[(2 * i) & 7u] | (seed[(2 * i + 1) & 7u] << 8));
            }
            if (fmt == CYDUI_BITMAP_A1) { /* the scaled form, with a scale that may be wrong and a length that may be short */
                cydui_draw_bitmap_a1(&ctx, q.x, q.y, (int16_t)(q.w % 70), (int16_t)(q.h % 70), (uint8_t)(u8(&r) % 11u), bitmap_mem, len, tint, u8(&r));
            } else {
                cydui_draw_bitmap(&ctx, q, bitmap_mem, len, fmt, tint, u8(&r));
            }
            break;
        }
        case 5: {
            cydui_rect q = rect16(&r);
            cydui_color c0 = (cydui_color)(u8(&r) | (u8(&r) << 8));
            cydui_color c1 = (cydui_color)(u8(&r) | (u8(&r) << 8));
            uint8_t flags = u8(&r);
            cydui_draw_gradient(&ctx, q, c0, c1, (flags & 1u) != 0, u8(&r));
            break;
        }
        case 6:
            cydui_set_clip(&ctx, rect16(&r));
            break;
        case 8: {
            const cydui_font *font = font_from_input(&r);
            int16_t x = pos(&r), y = pos(&r), wrap = pos(&r);
            uint8_t align = u8(&r) % 4u; /* 3 is out of range on purpose */
            uint8_t n = u8(&r) % 48u, flags = u8(&r);
            char buf[49];
            size_t i;
            cydui_color color = (cydui_color)(u8(&r) | (u8(&r) << 8));
            for (i = 0; i < n; i++) {
                buf[i] = (char)u8(&r);
            }
            buf[n] = '\0'; /* an embedded NUL is fine: NTS stops there, an explicit length does not */
            cydui_draw_text(&ctx, font, x, y, wrap, (cydui_align)align, buf,
                            (flags & 1u) ? CYDUI_NTS : n, color, u8(&r));
            break;
        }
        default:
            cydui_reset_clip(&ctx);
            break;
        }
    }
    cydui_end_frame(&ctx);
    return cydui_mem_hash(&mem);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    int rows = 1 + (size ? data[0] % H : 0);
    uint64_t full = render(data, size, H);
    uint64_t strips = render(data, size, rows);

    if (full != strips) {
        abort(); /* the image depended on the strip height */
    }
    return 0;
}
