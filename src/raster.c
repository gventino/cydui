/*
 * Strip rasterizer. Integer arithmetic only, so results are bit-identical across compilers and
 * targets. Antialiasing uses signed distances in 1/16 pixel; pixel centers sit at (x*16+8).
 */

#include <string.h>

#include "cydui/cmd.h"
#include "cydui/text.h"

typedef struct {
    cydui_color *buf;
    int16_t width, y0, h;
} strip_t;

static cydui_color *pixel_at(const strip_t *s, int x, int y)
{
    return s->buf + (size_t)(y - s->y0) * (size_t)s->width + (size_t)x;
}

/* Exact for 0..65535. */
static unsigned div255(unsigned x)
{
    x += 128u;
    return (x + (x >> 8)) >> 8;
}

static uint32_t isqrt64(uint64_t x)
{
    uint64_t r = 0, b = (uint64_t)1 << 62;
    while (b > x) {
        b >>= 2;
    }
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return (uint32_t)r;
}

static uint32_t isqrt32(uint32_t x)
{
    return isqrt64(x);
}

static void put(cydui_color *p, cydui_color c, unsigned a)
{
    if (a >= 252u) {
        *p = c;
    } else if (a >= 4u) {
        *p = cydui_blend(*p, c, (uint8_t)a);
    }
}

/* Coverage 0..16 (1/16 units) to blend alpha 0..255, combined with the command alpha. */
static unsigned cov_alpha(unsigned cov16, unsigned alpha)
{
    return div255(((cov16 * 255u + 8u) >> 4) * alpha);
}

static bool visible(const cydui_cmd *c, const strip_t *s, cydui_rect *out)
{
    cydui_rect strip;
    strip.x = 0;
    strip.y = s->y0;
    strip.w = s->width;
    strip.h = s->h;
    *out = cydui_rect_intersect(cydui_rect_intersect(c->rect, c->clip), strip);
    return !cydui_rect_empty(*out);
}

/* ---- rect ---- */

static void draw_rect(const cydui_cmd *c, const strip_t *s)
{
    cydui_rect v;
    int y, x;

    if (!visible(c, s, &v)) {
        return;
    }
    for (y = v.y; y < v.y + v.h; y++) {
        cydui_color *p = pixel_at(s, v.x, y);
        for (x = 0; x < v.w; x++) {
            put(&p[x], c->u.rect.color, c->alpha);
        }
    }
}

/* ---- gradient ---- */

static int lerp_int(int a, int b, int pos, int den)
{
    return den <= 0 ? a : a + (b - a) * pos / den;
}

static cydui_color lerp_color(const uint8_t a[3], const uint8_t b[3], int pos, int den)
{
    return CYDUI_RGB(lerp_int(a[0], b[0], pos, den), lerp_int(a[1], b[1], pos, den),
                     lerp_int(a[2], b[2], pos, den));
}

static void draw_gradient(const cydui_cmd *c, const strip_t *s)
{
    cydui_rect v;
    uint8_t a[3], b[3];
    int y, x;

    if (!visible(c, s, &v)) {
        return;
    }
    cydui_color_unpack(c->u.gradient.c0, &a[0], &a[1], &a[2]);
    cydui_color_unpack(c->u.gradient.c1, &b[0], &b[1], &b[2]);

    for (y = v.y; y < v.y + v.h; y++) {
        cydui_color *p = pixel_at(s, v.x, y);
        cydui_color row = 0;
        if (c->u.gradient.vertical) {
            row = lerp_color(a, b, y - c->rect.y, c->rect.h - 1);
        }
        for (x = 0; x < v.w; x++) {
            cydui_color col = c->u.gradient.vertical
                                  ? row
                                  : lerp_color(a, b, v.x + x - c->rect.x, c->rect.w - 1);
            put(&p[x], col, c->alpha);
        }
    }
}

/* ---- bitmap and glyph blitting ---- */

/* Draws a w x h A4 coverage mask with its top-left at (dx, dy), within `clip` and the strip. */
static void blit_a4(const strip_t *s, int dx, int dy, int w, int h, const uint8_t *data,
                    cydui_color tint, unsigned alpha, const cydui_rect *clip)
{
    int x0 = dx, x1 = dx + w, y0 = dy, y1 = dy + h, x, y;
    size_t row_bytes = ((size_t)w + 1u) / 2u;

    if (x0 < clip->x) x0 = clip->x;
    if (x1 > clip->x + clip->w) x1 = clip->x + clip->w;
    if (y0 < clip->y) y0 = clip->y;
    if (y1 > clip->y + clip->h) y1 = clip->y + clip->h;
    if (x0 < 0) x0 = 0;
    if (x1 > s->width) x1 = s->width;
    if (y0 < s->y0) y0 = s->y0;
    if (y1 > s->y0 + s->h) y1 = s->y0 + s->h;

    for (y = y0; y < y1; y++) {
        cydui_color *p = pixel_at(s, 0, y);
        const uint8_t *row = data + (size_t)(y - dy) * row_bytes;
        for (x = x0; x < x1; x++) {
            int sx = x - dx;
            unsigned byte = row[sx / 2];
            unsigned nib = (sx & 1) ? (byte & 0xFu) : (byte >> 4);
            if (nib) {
                put(&p[x], tint, div255(nib * 17u * alpha));
            }
        }
    }
}

static void draw_bitmap(const cydui_cmd *c, const strip_t *s)
{
    cydui_rect v;
    const uint8_t *data = (const uint8_t *)c->u.bitmap.data;
    int y, x;

    if (!visible(c, s, &v) || !data) {
        return;
    }
    if (c->u.bitmap.format == CYDUI_BITMAP_A4) {
        blit_a4(s, c->rect.x, c->rect.y, c->rect.w, c->rect.h, data, c->u.bitmap.tint, c->alpha,
                &c->clip);
        return;
    }
    for (y = v.y; y < v.y + v.h; y++) {
        cydui_color *p = pixel_at(s, v.x, y);
        int sy = y - c->rect.y;
        for (x = 0; x < v.w; x++) {
            int sx = v.x + x - c->rect.x;
            cydui_color px;
            memcpy(&px, data + ((size_t)sy * (size_t)c->rect.w + (size_t)sx) * 2u, 2u);
            put(&p[x], px, c->alpha);
        }
    }
}

/* ---- text ---- */

static void draw_text(const cydui_cmd *c, const strip_t *s)
{
    const cydui_font *f = c->u.text.font;
    int reach = f ? CYDUI_TEXT_REACH(f) : 0;
    cydui_text_iter it;
    cydui_text_line ln;
    int line_no = 0;

    if (!f || !c->u.text.str) {
        return;
    }
    cydui_text_iter_init(&it, f, c->u.text.str, c->u.text.len, c->u.text.wrap);
    while (cydui_text_next_line(&it, &ln)) {
        int line_y = c->rect.y + line_no * f->line_height;
        int pen, slack;
        size_t pos;

        line_no++;
        /* Lines only go down: past the strip means done. Both tests use the font's real reach. */
        if (line_y - reach >= s->y0 + s->h) {
            break;
        }
        if (line_y + f->line_height + reach <= s->y0) {
            continue;
        }
        slack = c->rect.w - ln.width;
        pen = c->rect.x;
        if (slack > 0 && c->u.text.align == CYDUI_ALIGN_CENTER) {
            pen += slack / 2;
        } else if (slack > 0 && c->u.text.align == CYDUI_ALIGN_RIGHT) {
            pen += slack;
        }
        for (pos = ln.start; pos < ln.end;) {
            uint32_t cp;
            size_t used = cydui_utf8_decode(it.s + pos, ln.end - pos, &cp);
            const cydui_glyph *g = cydui_text_resolve(f, cp);

            pos += used;
            if (!g) {
                continue;
            }
            {
                const uint8_t *data = cydui_font_glyph_data(f, g);
                if (data) {
                    blit_a4(s, pen + g->left, line_y + f->ascent - g->top, g->w, g->h, data,
                            c->u.text.color, c->alpha, &c->clip);
                }
            }
            pen += g->advance;
        }
    }
}

/* ---- rounded rect ---- */

typedef struct {
    int x0, y0, x1, y1; /* x1, y1 exclusive */
    int r;
} rr_t;

/* Coverage 0..16 of pixel (px, py). Straight edges are pixel-aligned; only corners need AA. */
static unsigned rr_cov16(const rr_t *q, int px, int py)
{
    int pxc, pyc, lx, rx, ty, by, dx, dy, d;

    if (px < q->x0 || px >= q->x1 || py < q->y0 || py >= q->y1) {
        return 0;
    }
    pxc = px * 16 + 8;
    pyc = py * 16 + 8;
    lx = (q->x0 + q->r) * 16;
    rx = (q->x1 - q->r) * 16;
    ty = (q->y0 + q->r) * 16;
    by = (q->y1 - q->r) * 16;
    dx = pxc < lx ? lx - pxc : (pxc > rx ? pxc - rx : 0);
    dy = pyc < ty ? ty - pyc : (pyc > by ? pyc - by : 0);
    if (dx == 0 || dy == 0) {
        return 16;
    }
    d = (int)isqrt32((uint32_t)(dx * dx + dy * dy)) - q->r * 16;
    d = 8 - d;
    return (unsigned)(d < 0 ? 0 : (d > 16 ? 16 : d));
}

static void draw_rrect(const cydui_cmd *c, const strip_t *s)
{
    cydui_rect v;
    rr_t outer, inner;
    int t = c->u.rrect.thickness;
    int r = c->u.rrect.radius;
    int y, x;
    bool has_inner;

    if (!visible(c, s, &v)) {
        return;
    }
    if (r > c->rect.w / 2) r = c->rect.w / 2;
    if (r > c->rect.h / 2) r = c->rect.h / 2;

    outer.x0 = c->rect.x;
    outer.y0 = c->rect.y;
    outer.x1 = c->rect.x + c->rect.w;
    outer.y1 = c->rect.y + c->rect.h;
    outer.r = r;

    inner.x0 = outer.x0 + t;
    inner.y0 = outer.y0 + t;
    inner.x1 = outer.x1 - t;
    inner.y1 = outer.y1 - t;
    inner.r = r > t ? r - t : 0;
    has_inner = t > 0 && inner.x1 > inner.x0 && inner.y1 > inner.y0;

    for (y = v.y; y < v.y + v.h; y++) {
        cydui_color *p = pixel_at(s, v.x, y);
        bool zone_row = y < outer.y0 + r || y >= outer.y1 - r;

        if (t == 0 && !zone_row) { /* body row: solid span */
            for (x = 0; x < v.w; x++) {
                put(&p[x], c->u.rrect.color, c->alpha);
            }
            continue;
        }
        for (x = 0; x < v.w; x++) {
            int px = v.x + x;
            unsigned cov = rr_cov16(&outer, px, y);
            if (cov && t > 0) {
                unsigned in = has_inner ? rr_cov16(&inner, px, y) : 0u;
                cov = cov > in ? cov - in : 0u;
            }
            if (cov) {
                put(&p[x], c->u.rrect.color, cov_alpha(cov, c->alpha));
            }
        }
    }
}

/* ---- line ---- */

static void draw_line(const cydui_cmd *c, const strip_t *s)
{
    cydui_rect v;
    int64_t ax = (int64_t)c->u.line.x0 * 16 + 8, ay = (int64_t)c->u.line.y0 * 16 + 8;
    int64_t bx = (int64_t)c->u.line.x1 * 16 + 8, by = (int64_t)c->u.line.y1 * 16 + 8;
    int64_t vx = bx - ax, vy = by - ay;
    int64_t vv = vx * vx + vy * vy;
    int64_t lenv = vv > 0 ? (int64_t)isqrt64((uint64_t)vv) : 0;
    int64_t hw16 = (int64_t)c->u.line.thickness * 8;
    int64_t reach = hw16 + 8; /* farther than this: coverage 0 */
    int y, x;

    if (!visible(c, s, &v)) {
        return;
    }
    for (y = v.y; y < v.y + v.h; y++) {
        cydui_color *p = pixel_at(s, v.x, y);
        for (x = 0; x < v.w; x++) {
            int64_t pcx = (int64_t)(v.x + x) * 16 + 8, pcy = (int64_t)y * 16 + 8;
            int64_t wx = pcx - ax, wy = pcy - ay;
            int64_t num = wx * vx + wy * vy;
            int64_t d;
            int64_t cov;

            if (vv == 0 || num <= 0) {
                int64_t d2 = wx * wx + wy * wy;
                if (d2 >= reach * reach) continue;
                d = (int64_t)isqrt64((uint64_t)d2);
            } else if (num >= vv) {
                int64_t ex = pcx - bx, ey = pcy - by;
                int64_t d2 = ex * ex + ey * ey;
                if (d2 >= reach * reach) continue;
                d = (int64_t)isqrt64((uint64_t)d2);
            } else {
                int64_t cross = vx * wy - vy * wx;
                if (cross < 0) cross = -cross;
                if (cross >= reach * lenv) continue;
                d = (cross + lenv / 2) / lenv;
            }
            cov = hw16 + 8 - d;
            cov = cov < 0 ? 0 : (cov > 16 ? 16 : cov);
            if (cov) {
                put(&p[x], c->u.line.color, cov_alpha((unsigned)cov, c->alpha));
            }
        }
    }
}

/* ---- entry point ---- */

void cydui_raster_strip(const cydui_cmd *cmds, uint32_t n, cydui_color *buf, int16_t width,
                        int16_t y0, int16_t h, cydui_color clear)
{
    strip_t s;
    size_t i, total = (size_t)width * (size_t)h;
    uint32_t k;

    s.buf = buf;
    s.width = width;
    s.y0 = y0;
    s.h = h;

    for (i = 0; i < total; i++) {
        buf[i] = clear;
    }
    for (k = 0; k < n; k++) {
        const cydui_cmd *c = &cmds[k];
        switch ((cydui_cmd_type)c->type) {
        case CYDUI_CMD_RECT:
            draw_rect(c, &s);
            break;
        case CYDUI_CMD_RRECT:
            draw_rrect(c, &s);
            break;
        case CYDUI_CMD_LINE:
            draw_line(c, &s);
            break;
        case CYDUI_CMD_BITMAP:
            draw_bitmap(c, &s);
            break;
        case CYDUI_CMD_GRADIENT:
            draw_gradient(c, &s);
            break;
        case CYDUI_CMD_TEXT:
            draw_text(c, &s);
            break;
        }
    }
}
