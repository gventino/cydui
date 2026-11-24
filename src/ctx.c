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
    size_t rows;

    ctx->backend = NULL;
    ctx->cmds = NULL;
    ctx->strip = NULL;
    ctx->cmd_cap = ctx->cmd_len = 0;
    ctx->strip_h = 0;
    ctx->errors = 0;

    if (!cfg || !cfg->backend || !cfg->backend->flush_strip || cfg->backend->width <= 0 ||
        cfg->backend->height <= 0) {
        return CYDUI_ERR_BAD_ARG;
    }

    cydui_arena_init(&cmd_arena, cfg->cmd_mem, cfg->cmd_mem_size);
    cydui_arena_init(&strip_arena, cfg->strip_mem, cfg->strip_mem_size);

    row_bytes = (size_t)cfg->backend->width * sizeof(cydui_color);
    rows = cfg->strip_mem_size / row_bytes;
    if (rows > (size_t)cfg->backend->height) {
        rows = (size_t)cfg->backend->height;
    }
    if (rows == 0) {
        return CYDUI_ERR_NO_MEMORY;
    }

    ctx->strip = (cydui_color *)cydui_arena_alloc(&strip_arena, rows * row_bytes,
                                                  sizeof(cydui_color));
    if (!ctx->strip) {
        return CYDUI_ERR_NO_MEMORY;
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

    ctx->backend = cfg->backend;
    ctx->strip_h = (int16_t)rows;
    ctx->clip = screen_rect(ctx);
    ctx->clear = 0;
    return 0;
}

void cydui_begin_frame(cydui_ctx *ctx, cydui_color clear)
{
    ctx->cmd_len = 0;
    ctx->clip = screen_rect(ctx);
    ctx->clear = clear;
}

void cydui_end_frame(cydui_ctx *ctx)
{
    const cydui_backend *be = ctx->backend;
    int16_t y;

    for (y = 0; y < be->height; y = (int16_t)(y + ctx->strip_h)) {
        int16_t h = ctx->strip_h;
        cydui_rect area;
        if (h > be->height - y) {
            h = (int16_t)(be->height - y);
        }
        cydui_raster_strip(ctx->cmds, ctx->cmd_len, ctx->strip, be->width, y, h, ctx->clear);
        area.x = 0;
        area.y = y;
        area.w = be->width;
        area.h = h;
        be->flush_strip(be->user, &area, ctx->strip);
    }
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
static cydui_cmd *push(cydui_ctx *ctx, cydui_cmd_type type, cydui_rect geometry, uint8_t alpha)
{
    cydui_cmd *c;

    if (alpha == 0 || cydui_rect_empty(cydui_rect_intersect(geometry, ctx->clip))) {
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
