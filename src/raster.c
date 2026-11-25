/*
 * Strip rasterizer. Integer arithmetic only, so results are bit-identical across compilers and
 * targets. Antialiasing uses signed distances in 1/16 pixel; pixel centers sit at (x*16+8).
 *
 * Speed (R-02): the output of every command is defined pixel by pixel, and the fast paths below
 * only skip the work whose result is known (a run of one coverage, a pixel too far from a shape,
 * a transparent nibble); what is left is the same arithmetic as the plain version.
 */

#include <string.h>

#include "blend.h"
#include "cydui/cmd.h"
#include "cydui/text.h"

typedef struct {
    cydui_color *buf;
    int16_t width, y0, h;
} strip_t;

#if defined(__GNUC__)
typedef uint32_t __attribute__((may_alias)) word_t;
#else
typedef uint32_t word_t;
#endif

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

/* The same floor square root without 64-bit arithmetic (slow on a 32-bit core). */
static uint32_t isqrt32(uint32_t x)
{
    uint32_t r = 0, b = (uint32_t)1 << 30;
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
    return r;
}

static void put(cydui_color *p, cydui_color c, unsigned a)
{
    if (a >= 252u) {
        *p = c;
    } else if (a >= 4u) {
        *p = cydui__blend565(*p, c, (uint8_t)a);
    }
}

/* put() over `n` pixels: opaque is a fill by words, translucent remembers the last result because
 * a run over one background (the usual case) blends the same pair again and again. */
static void span_put(cydui_color *p, int n, cydui_color c, unsigned a)
{
    if (n <= 0) {
        return;
    }
    if (a >= 252u) {
        uint32_t w = (uint32_t)c | ((uint32_t)c << 16);

        if (((uintptr_t)p & 2u) != 0u) {
            *p++ = c;
            n--;
        }
        for (; n >= 2; n -= 2, p += 2) {
            *(word_t *)p = w;
        }
        if (n > 0) {
            *p = c;
        }
    } else if (a >= 4u) {
        cydui_color last_d = 0, last_r = 0;
        bool have = false;
        int i;

        for (i = 0; i < n; i++) {
            cydui_color d = p[i];
            if (!have || d != last_d) {
                last_d = d;
                last_r = cydui__blend565(d, c, (uint8_t)a);
                have = true;
            }
            p[i] = last_r;
        }
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
    int y;

    if (!visible(c, s, &v)) {
        return;
    }
    for (y = v.y; y < v.y + v.h; y++) {
        span_put(pixel_at(s, v.x, y), v.w, c->u.rect.color, c->alpha);
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
        if (c->u.gradient.vertical) { /* one color for the whole row */
            span_put(p, v.w, lerp_color(a, b, y - c->rect.y, c->rect.h - 1), c->alpha);
            continue;
        }
        for (x = 0; x < v.w; x++) {
            put(&p[x], lerp_color(a, b, v.x + x - c->rect.x, c->rect.w - 1), c->alpha);
        }
    }
}

/* ---- bitmap and glyph blitting ---- */

/* tab[n] is the blend alpha of a 4-bit coverage n under the command alpha. */
static void a4_table(uint8_t tab[16], unsigned alpha)
{
    unsigned n;
    for (n = 0; n < 16; n++) {
        tab[n] = (uint8_t)div255(n * 17u * alpha);
    }
}

/* Draws a w x h A4 coverage mask with its top-left at (dx, dy), within `clip` and the strip. */
static void blit_a4(const strip_t *s, int dx, int dy, int w, int h, const uint8_t *data,
                    cydui_color tint, const uint8_t tab[16], const cydui_rect *clip)
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
    if (x0 >= x1) {
        return;
    }

    for (y = y0; y < y1; y++) {
        cydui_color *p = pixel_at(s, 0, y);
        const uint8_t *q = data + (size_t)(y - dy) * row_bytes + (size_t)((x0 - dx) >> 1);

        x = x0;
        if (((x0 - dx) & 1) != 0) { /* the row starts on the low nibble of a byte */
            put(&p[x++], tint, tab[*q++ & 0xFu]);
        }
        for (; x + 1 < x1; x += 2, q++) { /* a byte is two pixels, and most are empty */
            if (*q != 0) {
                put(&p[x], tint, tab[*q >> 4]);
                put(&p[x + 1], tint, tab[*q & 0xFu]);
            }
        }
        if (x < x1) {
            put(&p[x], tint, tab[*q >> 4]);
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
        uint8_t tab[16];
        a4_table(tab, c->alpha);
        blit_a4(s, c->rect.x, c->rect.y, c->rect.w, c->rect.h, data, c->u.bitmap.tint, tab, &c->clip);
        return;
    }
    for (y = v.y; y < v.y + v.h; y++) {
        cydui_color *p = pixel_at(s, v.x, y);
        int sy = y - c->rect.y;
        const uint8_t *src = data + ((size_t)sy * (size_t)c->rect.w + (size_t)(v.x - c->rect.x)) * 2u;

        if (c->alpha >= 252u) { /* opaque: the row is copied */
            memcpy(p, src, (size_t)v.w * 2u);
            continue;
        }
        for (x = 0; x < v.w; x++) {
            cydui_color px;
            memcpy(&px, src + (size_t)x * 2u, 2u);
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
    uint8_t tab[16];

    if (!f || !c->u.text.str) {
        return;
    }
    a4_table(tab, c->alpha);
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
                            c->u.text.color, tab, &c->clip);
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

/* Coverage 0..16 of pixel (px, py). Straight edges are pixel-aligned; only corners need AA, and
 * of those only the pixels within half a pixel of the arc need the square root. */
static unsigned rr_cov16(const rr_t *q, int px, int py)
{
    int pxc, pyc, lx, rx, ty, by, dx, dy, big, d;
    uint32_t dist2;

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
    big = q->r * 16;
    dist2 = (uint32_t)(dx * dx + dy * dy);
    /* floor(sqrt(dist2)) >= big + 8 gives 0 and <= big - 8 gives 16, whatever the root is exactly */
    if (dist2 >= (uint32_t)((big + 8) * (big + 8))) {
        return 0;
    }
    if (big > 7 && dist2 < (uint32_t)((big - 7) * (big - 7))) {
        return 16;
    }
    d = (int)isqrt32(dist2) - big;
    d = 8 - d;
    return (unsigned)(d < 0 ? 0 : (d > 16 ? 16 : d));
}

/* The coverage of the outline or fill: the outer shape minus the inner one. */
static unsigned rr_cov(const rr_t *outer, const rr_t *inner, bool has_inner, int t, int px, int py)
{
    unsigned cov = rr_cov16(outer, px, py);
    if (cov && t > 0) {
        unsigned in = has_inner ? rr_cov16(inner, px, py) : 0u;
        cov = cov > in ? cov - in : 0u;
    }
    return cov;
}

/* Is column px of row py among the pixels where the arc of `q` can change the coverage? */
static bool rr_in_corner(const rr_t *q, int px, int py)
{
    if (py >= q->y0 + q->r && py < q->y1 - q->r) {
        return false;
    }
    return (px >= q->x0 && px < q->x0 + q->r) || (px >= q->x1 - q->r && px < q->x1);
}

static void draw_rrect(const cydui_cmd *c, const strip_t *s)
{
    cydui_rect v;
    rr_t outer, inner;
    int t = c->u.rrect.thickness;
    int r = c->u.rrect.radius;
    int y;
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
        /* A row is cut into runs at the edges of the corners and of the inner shape: inside a run
         * the coverage is one value (computed once) or, in a corner of the outer shape, it varies
         * and each pixel is computed. The arc of the inner shape lies inside that corner. */
        bool inner_row = has_inner && y >= inner.y0 && y < inner.y1;
        int cuts[6], n = 0, i, j, x = v.x, end = v.x + v.w;

        cuts[n++] = outer.x0;
        cuts[n++] = outer.x0 + outer.r;
        cuts[n++] = outer.x1 - outer.r;
        cuts[n++] = outer.x1;
        if (inner_row) {
            cuts[n++] = inner.x0;
            cuts[n++] = inner.x1;
        }
        for (i = 1; i < n; i++) { /* insertion sort of at most 6 */
            int k = cuts[i];
            for (j = i - 1; j >= 0 && cuts[j] > k; j--) {
                cuts[j + 1] = cuts[j];
            }
            cuts[j + 1] = k;
        }
        while (x < end) {
            int stop = end;
            bool varies;

            for (i = 0; i < n; i++) {
                if (cuts[i] > x) {
                    stop = cuts[i] < end ? cuts[i] : end;
                    break;
                }
            }
            varies = rr_in_corner(&outer, x, y);
            if (varies) {
                for (i = x; i < stop; i++) {
                    unsigned cov = rr_cov(&outer, &inner, has_inner, t, i, y);
                    if (cov) {
                        put(&p[i - v.x], c->u.rrect.color, cov_alpha(cov, c->alpha));
                    }
                }
            } else {
                unsigned cov = rr_cov(&outer, &inner, has_inner, t, x, y);
                if (cov) {
                    span_put(&p[x - v.x], stop - x, c->u.rrect.color, cov_alpha(cov, c->alpha));
                }
            }
            x = stop;
        }
    }
}

/* ---- line ---- */

static int64_t floor_div(int64_t a, int64_t b) /* b > 0 */
{
    int64_t q = a / b;
    return (a % b != 0 && a < 0) ? q - 1 : q;
}

/* The columns [*lo, *hi) of row `pcy` (in 1/16 px) where the stroke can have any coverage: a
 * superset taken from the capsule of radius `reach` around the segment, where the exact test of
 * the pixel decides. Returns false when the row cannot be touched at all. */
static bool line_columns(int64_t ax, int64_t ay, int64_t bx, int64_t by, int64_t reach, int64_t pcy,
                         int x0, int x1, int *lo, int *hi)
{
    int64_t vx = bx - ax, vy = by - ay;
    int64_t xmin, xmax, l, h;

    if (vy == 0) {
        int64_t dy = pcy - ay;
        if (dy < -reach || dy > reach) {
            return false;
        }
        xmin = ax < bx ? ax : bx;
        xmax = ax < bx ? bx : ax;
    } else {
        int64_t nlo = vy > 0 ? 0 : vy, nhi = vy > 0 ? vy : 0; /* vy * t for t in [0, 1] */
        int64_t u0 = pcy - reach - ay, u1 = pcy + reach - ay; /* what vy * t may be to reach the row */
        int64_t c0, c1, xs, xe;

        if (u1 < nlo || u0 > nhi) {
            return false;
        }
        c0 = u0 < nlo ? nlo : (u0 > nhi ? nhi : u0);
        c1 = u1 < nlo ? nlo : (u1 > nhi ? nhi : u1);
        xs = ax + vx * c0 / vy; /* the division truncates: the pad below covers it */
        xe = ax + vx * c1 / vy;
        xmin = (xs < xe ? xs : xe) - 1;
        xmax = (xs < xe ? xe : xs) + 1;
    }
    l = floor_div(xmin - reach - 8, 16) - 1;
    h = floor_div(xmax + reach - 8, 16) + 2;
    *lo = l < x0 ? x0 : (l > x1 ? x1 : (int)l);
    *hi = h < x0 ? x0 : (h > x1 ? x1 : (int)h);
    return *lo < *hi;
}

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
        int lo, hi;

        if (!line_columns(ax, ay, bx, by, reach, (int64_t)y * 16 + 8, v.x, v.x + v.w, &lo, &hi)) {
            continue;
        }
        for (x = lo - v.x; x < hi - v.x; x++) {
            int64_t pcx = (int64_t)(v.x + x) * 16 + 8, pcy = (int64_t)y * 16 + 8;
            int64_t wx = pcx - ax, wy = pcy - ay;
            int64_t num = wx * vx + wy * vy;
            int64_t d;
            int64_t cov;

            if (vv == 0 || lenv == 0 || num <= 0) {
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
    size_t total = (size_t)width * (size_t)h;
    uint32_t k;

    s.buf = buf;
    s.width = width;
    s.y0 = y0;
    s.h = h;

    if (total > 0) {
        span_put(buf, (int)total, clear, 255u);
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
