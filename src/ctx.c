#include <string.h>

#include "cydui/cydui.h"

static cydui_rect screen_rect(const cydui_ctx *ctx)
{
    cydui_rect r = {0, 0, ctx->backend->width, ctx->backend->height};
    return r;
}

uint32_t cydui_init(cydui_ctx *ctx, const cydui_config *cfg)
{
    cydui_arena cmd_arena, strip_arena;
    size_t row_bytes;
    size_t rows, nbuf;

    ctx->backend = NULL;
    ctx->cmds = NULL;
    ctx->strip = NULL;
    ctx->cmd_cap = ctx->cmd_len = 0;
    ctx->strip_h = 0;
    ctx->strip_pixels = 0;
    ctx->strip_buffers = 1;
    ctx->strip_next = 0;
    ctx->dirty = NULL;
    ctx->dirty_cap = 0;
    ctx->dirty_primed = false;
    memset(&ctx->post, 0, sizeof(ctx->post));
    ctx->errors = 0;
    cydui_arena_init(&ctx->text_arena, NULL, 0);

    if (!cfg || !cfg->backend || !cfg->backend->flush_strip || cfg->backend->width <= 0 ||
        cfg->backend->height <= 0) {
        return CYDUI_ERR_BAD_ARG;
    }

    cydui_arena_init(&cmd_arena, cfg->cmd_mem, cfg->cmd_mem_size);
    cydui_arena_init(&strip_arena, cfg->strip_mem, cfg->strip_mem_size);

    if (cfg->strip_buffers > 2) {
        return CYDUI_ERR_BAD_ARG;
    }
    nbuf = cfg->strip_buffers == 2 ? 2u : 1u;
    row_bytes = (size_t)cfg->backend->width * sizeof(cydui_color);
    rows = cfg->strip_mem_size / (row_bytes * nbuf);
    if (rows > (size_t)cfg->backend->height) {
        rows = (size_t)cfg->backend->height;
    }
    if (rows == 0) {
        return CYDUI_ERR_NO_MEMORY;
    }

    ctx->strip = (cydui_color *)cydui_arena_alloc(&strip_arena, rows * row_bytes * nbuf,
                                                  sizeof(cydui_color));
    if (!ctx->strip) {
        return CYDUI_ERR_NO_MEMORY;
    }
    ctx->strip_pixels = rows * (size_t)cfg->backend->width;
    ctx->strip_buffers = (uint8_t)nbuf;

    if (cfg->dirty_mem) {
        size_t strips = (cfg->backend->height + rows - 1u) / rows;
        if (cfg->dirty_mem_size / sizeof(uint64_t) < strips ||
            ((uintptr_t)cfg->dirty_mem & (sizeof(uint64_t) - 1u)) != 0) {
            return CYDUI_ERR_NO_MEMORY;
        }
        ctx->dirty = (uint64_t *)cfg->dirty_mem;
        ctx->dirty_cap = (uint32_t)strips;
    }

    /* Use as many whole commands as fit; alignment padding may cost one slot. */
    {
        size_t n = cmd_arena.cap / sizeof(cydui_cmd);
        while (n > 0 && !(ctx->cmds = (cydui_cmd *)cydui_arena_alloc(
                              &cmd_arena, n * sizeof(cydui_cmd), sizeof(void *)))) {
            cydui_arena_reset(&cmd_arena);
            n--;
        }
        if (n == 0) {
            return CYDUI_ERR_NO_MEMORY;
        }
        ctx->cmd_cap = (uint32_t)n;
    }

    cydui_arena_init(&ctx->text_arena, cfg->text_mem, cfg->text_mem_size);
    ctx->backend = cfg->backend;
    ctx->strip_h = (int16_t)rows;
    ctx->clip = screen_rect(ctx);
    ctx->clear = 0;
    return 0;
}

void cydui_begin_frame(cydui_ctx *ctx, cydui_color clear)
{
    ctx->cmd_len = 0;
    cydui_arena_reset(&ctx->text_arena);
    ctx->clip = screen_rect(ctx);
    ctx->clear = clear;
}

/* ---- dirty regions: a 64-bit FNV-1a over what can change the pixels of a strip ---- */

#define FNV_BASIS 14695981039346656037ull
#define FNV_PRIME 1099511628211ull

static uint64_t fnv(uint64_t h, uint32_t v)
{
    int i;
    for (i = 0; i < 4; i++) { /* byte by byte, so that the result does not depend on the host */
        h = (h ^ ((v >> (8 * i)) & 0xFFu)) * FNV_PRIME;
    }
    return h;
}

static uint64_t fnv_rect(uint64_t h, cydui_rect r)
{
    h = fnv(h, (uint32_t)(uint16_t)r.x | ((uint32_t)(uint16_t)r.y << 16));
    return fnv(h, (uint32_t)(uint16_t)r.w | ((uint32_t)(uint16_t)r.h << 16));
}

static uint64_t fnv_ptr(uint64_t h, const void *p)
{
    uint64_t v = (uint64_t)(uintptr_t)p;
    return fnv(fnv(h, (uint32_t)v), (uint32_t)(v >> 32));
}

/* Does the command draw anything in the rows [y, y + h)? A command only draws inside its clip, and,
 * text apart, inside its rectangle too (text ink may fall outside the box of its line). */
static bool touches(const cydui_cmd *c, int y, int h)
{
    cydui_rect box = c->clip;

    if (c->type != CYDUI_CMD_TEXT) {
        box = cydui_rect_intersect(c->rect, c->clip);
    }
    return box.w > 0 && box.h > 0 && box.y < y + h && box.y + box.h > y;
}

static uint64_t strip_hash(const cydui_ctx *ctx, int y, int h)
{
    uint64_t hash = fnv(FNV_BASIS, ctx->clear);
    uint32_t k, i;

    for (k = 0; k < ctx->cmd_len; k++) {
        const cydui_cmd *c = &ctx->cmds[k];

        if (!touches(c, y, h)) {
            continue;
        }
        hash = fnv(hash, (uint32_t)c->type | ((uint32_t)c->alpha << 8));
        hash = fnv_rect(fnv_rect(hash, c->rect), c->clip);
        switch ((cydui_cmd_type)c->type) {
        case CYDUI_CMD_RECT:
            hash = fnv(hash, c->u.rect.color);
            break;
        case CYDUI_CMD_RRECT:
            hash = fnv(hash, (uint32_t)c->u.rrect.color | ((uint32_t)c->u.rrect.radius << 16) |
                                 ((uint32_t)c->u.rrect.thickness << 24));
            break;
        case CYDUI_CMD_LINE:
            hash = fnv(hash, (uint32_t)(uint16_t)c->u.line.x0 | ((uint32_t)(uint16_t)c->u.line.y0 << 16));
            hash = fnv(hash, (uint32_t)(uint16_t)c->u.line.x1 | ((uint32_t)(uint16_t)c->u.line.y1 << 16));
            hash = fnv(hash, (uint32_t)c->u.line.color | ((uint32_t)c->u.line.thickness << 16));
            break;
        case CYDUI_CMD_BITMAP:
            hash = fnv_ptr(hash, c->u.bitmap.data);
            hash = fnv(hash, (uint32_t)c->u.bitmap.tint | ((uint32_t)c->u.bitmap.format << 16));
            break;
        case CYDUI_CMD_GRADIENT:
            hash = fnv(hash, (uint32_t)c->u.gradient.c0 | ((uint32_t)c->u.gradient.c1 << 16));
            hash = fnv(hash, c->u.gradient.vertical);
            break;
        case CYDUI_CMD_TEXT:
            hash = fnv_ptr(hash, c->u.text.font);
            hash = fnv(hash, (uint32_t)c->u.text.len | ((uint32_t)(uint16_t)c->u.text.wrap << 16));
            hash = fnv(hash, (uint32_t)c->u.text.color | ((uint32_t)c->u.text.align << 16));
            for (i = 0; i < c->u.text.len; i++) { /* the string itself: the arena is reused every frame */
                hash = (hash ^ (uint8_t)c->u.text.str[i]) * FNV_PRIME;
            }
            break;
        }
    }
    return hash;
}

void cydui_invalidate(cydui_ctx *ctx)
{
    ctx->dirty_primed = false;
}

void cydui_set_post_effect(cydui_ctx *ctx, const cydui_post_effect *fx)
{
    if (fx && fx->apply) {
        ctx->post = *fx;
    } else {
        memset(&ctx->post, 0, sizeof(ctx->post));
    }
    ctx->dirty_primed = false;
}

void cydui_end_frame(cydui_ctx *ctx)
{
    const cydui_backend *be = ctx->backend;
    int16_t y;
    uint32_t idx = 0;

    for (y = 0; y < be->height; y = (int16_t)(y + ctx->strip_h), idx++) {
        int16_t h = ctx->strip_h;
        cydui_rect area;
        cydui_color *buf;
        if (h > be->height - y) {
            h = (int16_t)(be->height - y);
        }
        if (ctx->dirty) {
            uint64_t hash = strip_hash(ctx, y, h);
            if (ctx->post.apply) { /* what the effect will do to these rows is part of what they show */
                hash = fnv(hash, ctx->post.key ? ctx->post.key(ctx->post.user, y, h) : 0u);
            }
            bool same = ctx->dirty_primed && ctx->dirty[idx] == hash;
            ctx->dirty[idx] = hash;
            if (same) {
                continue; /* the panel still shows exactly this */
            }
        }
        buf = ctx->strip + (size_t)ctx->strip_next * ctx->strip_pixels;
        ctx->strip_next = (uint8_t)((ctx->strip_next + 1u) % ctx->strip_buffers);
        cydui_raster_strip(ctx->cmds, ctx->cmd_len, buf, be->width, y, h, ctx->clear);
        if (ctx->post.apply) {
            ctx->post.apply(ctx->post.user, buf, be->width, y, h);
        }
        area.x = 0;
        area.y = y;
        area.w = be->width;
        area.h = h;
        be->flush_strip(be->user, &area, buf);
    }
    ctx->dirty_primed = ctx->dirty != NULL;
    if (be->frame_end) {
        be->frame_end(be->user);
    }
}

uint32_t cydui_errors(const cydui_ctx *ctx)
{
    return ctx->errors;
}

void cydui_clear_errors(cydui_ctx *ctx)
{
    ctx->errors = 0;
}

uint32_t cydui_cmd_count(const cydui_ctx *ctx)
{
    return ctx->cmd_len;
}

int16_t cydui_strip_height(const cydui_ctx *ctx)
{
    return ctx->strip_h;
}

static void cmd_reverse(cydui_ctx *ctx, uint32_t from, uint32_t to)
{
    while (from + 1 < to) {
        cydui_cmd tmp = ctx->cmds[from];
        ctx->cmds[from] = ctx->cmds[to - 1];
        ctx->cmds[to - 1] = tmp;
        from++;
        to--;
    }
}

void cydui__cmd_move_tail(cydui_ctx *ctx, uint32_t start, uint32_t mid)
{
    uint32_t end = ctx->cmd_len;

    if (start >= mid || mid >= end) {
        return; /* nothing before, or nothing after: already in order */
    }
    cmd_reverse(ctx, start, mid);
    cmd_reverse(ctx, mid, end);
    cmd_reverse(ctx, start, end);
}

void cydui_set_clip(cydui_ctx *ctx, cydui_rect r)
{
    ctx->clip = cydui_rect_intersect(r, screen_rect(ctx));
}

void cydui_reset_clip(cydui_ctx *ctx)
{
    ctx->clip = screen_rect(ctx);
}

/*
 * Reserves the next command slot. Returns NULL (without consuming capacity) if the visible part
 * of `geometry` is empty, or NULL with CYDUI_ERR_CMD_OVERFLOW if the list is full.
 */
static cydui_cmd *push_reaching(cydui_ctx *ctx, cydui_cmd_type type, cydui_rect geometry,
                                int reach, uint8_t alpha)
{
    cydui_cmd *c;
    int32_t gx0 = (int32_t)geometry.x - reach, gx1 = (int32_t)geometry.x + geometry.w + reach;
    int32_t gy0 = (int32_t)geometry.y - reach, gy1 = (int32_t)geometry.y + geometry.h + reach;

    /* `reach` widens the geometry for commands whose pixels may lie outside it (text). */
    if (alpha == 0 || geometry.w <= 0 || geometry.h <= 0 || gx0 >= ctx->clip.x + ctx->clip.w ||
        gx1 <= ctx->clip.x || gy0 >= ctx->clip.y + ctx->clip.h || gy1 <= ctx->clip.y) {
        return NULL;
    }
    if (ctx->cmd_len >= ctx->cmd_cap) {
        ctx->errors |= CYDUI_ERR_CMD_OVERFLOW;
        return NULL;
    }
    c = &ctx->cmds[ctx->cmd_len++];
    c->type = (uint8_t)type;
    c->alpha = alpha;
    c->rect = geometry;
    c->clip = ctx->clip;
    return c;
}

static cydui_cmd *push(cydui_ctx *ctx, cydui_cmd_type type, cydui_rect geometry, uint8_t alpha)
{
    return push_reaching(ctx, type, geometry, 0, alpha);
}

void cydui_draw_rect(cydui_ctx *ctx, cydui_rect r, cydui_color color, uint8_t alpha)
{
    cydui_cmd *c = push(ctx, CYDUI_CMD_RECT, r, alpha);
    if (c) {
        c->u.rect.color = color;
    }
}

void cydui_draw_rrect(cydui_ctx *ctx, cydui_rect r, uint8_t radius, cydui_color color,
                      uint8_t alpha)
{
    cydui_draw_rrect_outline(ctx, r, radius, 0, color, alpha);
}

void cydui_draw_rrect_outline(cydui_ctx *ctx, cydui_rect r, uint8_t radius, uint8_t thickness,
                              cydui_color color, uint8_t alpha)
{
    cydui_cmd *c = push(ctx, CYDUI_CMD_RRECT, r, alpha);
    if (c) {
        c->u.rrect.color = color;
        c->u.rrect.radius = radius;
        c->u.rrect.thickness = thickness;
    }
}

void cydui_draw_line(cydui_ctx *ctx, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     uint8_t thickness, cydui_color color, uint8_t alpha)
{
    int32_t pad, minx, maxx, miny, maxy;
    cydui_rect box;
    cydui_cmd *c;

    if (thickness == 0) {
        ctx->errors |= CYDUI_ERR_BAD_ARG;
        return;
    }
    pad = thickness / 2 + 2;
    minx = (x0 < x1 ? x0 : x1) - pad;
    maxx = (x0 < x1 ? x1 : x0) + pad + 1;
    miny = (y0 < y1 ? y0 : y1) - pad;
    maxy = (y0 < y1 ? y1 : y0) + pad + 1;
    /* Clamp to int16: anything beyond is far off screen anyway. */
    if (minx < -32768) minx = -32768;
    if (miny < -32768) miny = -32768;
    if (maxx > 32767) maxx = 32767;
    if (maxy > 32767) maxy = 32767;
    box.x = (int16_t)minx;
    box.y = (int16_t)miny;
    box.w = (int16_t)(maxx - minx);
    box.h = (int16_t)(maxy - miny);

    c = push(ctx, CYDUI_CMD_LINE, box, alpha);
    if (c) {
        c->u.line.x0 = x0;
        c->u.line.y0 = y0;
        c->u.line.x1 = x1;
        c->u.line.y1 = y1;
        c->u.line.color = color;
        c->u.line.thickness = thickness;
    }
}

size_t cydui_bitmap_size(cydui_bitmap_format fmt, int16_t w, int16_t h)
{
    size_t W, H;
    if (w <= 0 || h <= 0) {
        return 0;
    }
    W = (size_t)w;
    H = (size_t)h;
    switch (fmt) {
    case CYDUI_BITMAP_RGB565:
        return W * H * 2u;
    case CYDUI_BITMAP_A4:
        return ((W + 1u) / 2u) * H;
    case CYDUI_BITMAP_A1:
        return ((W + 7u) / 8u) * H;
    default:
        return 0;
    }
}

void cydui_draw_bitmap(cydui_ctx *ctx, cydui_rect r, const void *data, size_t len,
                       cydui_bitmap_format fmt, cydui_color tint, uint8_t alpha)
{
    cydui_cmd *c;
    size_t need = cydui_bitmap_size(fmt, r.w, r.h);

    if (r.w <= 0 || r.h <= 0) {
        return; /* nothing to draw */
    }
    if (!data || need == 0) {
        ctx->errors |= CYDUI_ERR_BAD_BITMAP;
        return;
    }
    if (len < need) {
        ctx->errors |= CYDUI_ERR_BAD_BITMAP;
        return;
    }
    c = push(ctx, CYDUI_CMD_BITMAP, r, alpha);
    if (c) {
        c->u.bitmap.data = data;
        c->u.bitmap.tint = tint;
        c->u.bitmap.format = (uint8_t)fmt;
        c->u.bitmap.scale = 1;
    }
}

void cydui_draw_bitmap_a1(cydui_ctx *ctx, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t scale, const void *data,
                          size_t len, cydui_color tint, uint8_t alpha)
{
    cydui_cmd *c;
    size_t need;
    long sw = (long)w * (long)scale, sh = (long)h * (long)scale;

    if (w <= 0 || h <= 0) {
        return; /* nothing to draw */
    }
    if (scale < 1 || scale > 8 || sw > 32767L || sh > 32767L) {
        ctx->errors |= CYDUI_ERR_BAD_ARG;
        return;
    }
    need = cydui_bitmap_size(CYDUI_BITMAP_A1, w, h);
    if (!data || len < need) {
        ctx->errors |= CYDUI_ERR_BAD_BITMAP;
        return;
    }
    c = push(ctx, CYDUI_CMD_BITMAP, (cydui_rect){x, y, (int16_t)sw, (int16_t)sh}, alpha);
    if (c) {
        c->u.bitmap.data = data;
        c->u.bitmap.tint = tint;
        c->u.bitmap.format = (uint8_t)CYDUI_BITMAP_A1;
        c->u.bitmap.scale = scale;
    }
}

void cydui_draw_gradient(cydui_ctx *ctx, cydui_rect r, cydui_color c0, cydui_color c1,
                         bool vertical, uint8_t alpha)
{
    cydui_cmd *c = push(ctx, CYDUI_CMD_GRADIENT, r, alpha);
    if (c) {
        c->u.gradient.c0 = c0;
        c->u.gradient.c1 = c1;
        c->u.gradient.vertical = vertical ? 1u : 0u;
    }
}

void cydui_draw_text(cydui_ctx *ctx, const cydui_font *font, int16_t x, int16_t y, int16_t wrap_w,
                     cydui_align align, const char *s, size_t len, cydui_color color,
                     uint8_t alpha)
{
    bool truncated;
    size_t n;
    cydui_text_extent e;
    cydui_rect box;
    cydui_cmd *c;
    char *copy;

    if (alpha == 0) {
        return;
    }
    if (!cydui_font_fallback(font)) {
        ctx->errors |= CYDUI_ERR_BAD_ARG; /* no font, or one that cannot be used */
        return;
    }
    n = cydui_text_clip_len(s, len, &truncated);
    if (truncated) {
        ctx->errors |= CYDUI_ERR_TEXT_TRUNCATED;
    }
    if (n == 0) {
        return;
    }
    e = cydui_text_size(font, s, n, wrap_w);
    if (e.lines == 0) {
        return;
    }
    box.x = x;
    box.y = y;
    box.w = wrap_w > 0 ? wrap_w : e.w;
    box.h = e.h;

    c = push_reaching(ctx, CYDUI_CMD_TEXT, box, CYDUI_TEXT_REACH(font), alpha);
    if (!c) {
        return;
    }
    copy = (char *)cydui_arena_alloc(&ctx->text_arena, n, 1);
    if (!copy) { /* no room for the string: take the command back */
        ctx->cmd_len--;
        ctx->errors |= CYDUI_ERR_NO_MEMORY;
        return;
    }
    memcpy(copy, s, n);
    c->u.text.font = font;
    c->u.text.str = copy;
    c->u.text.len = (uint16_t)n;
    c->u.text.wrap = wrap_w > 0 ? wrap_w : 0;
    c->u.text.color = color;
    c->u.text.align = (uint8_t)align;
}
