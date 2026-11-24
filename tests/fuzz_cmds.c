/*
 * libFuzzer harness (CT-12). The input is read as a sequence of drawing calls with arbitrary
 * parameters (full int16 coordinates, any radius/thickness/alpha, any bitmap length up to the
 * real buffer size). Under ASan/UBSan this looks for out-of-bounds access and undefined
 * behavior, and it checks the core invariant: the image does not depend on the strip height.
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
    if (cydui_init(&ctx, &cfg) != 0) {
        abort();
    }

    cydui_begin_frame(&ctx, (cydui_color)(u8(&r) | (u8(&r) << 8)));
    for (ops = 0; ops < MAX_OPS && r.n > 0; ops++) {
        switch (u8(&r) % 8u) {
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
            cydui_bitmap_format fmt = (u8(&r) & 1u) ? CYDUI_BITMAP_A4 : CYDUI_BITMAP_RGB565;
            cydui_color tint = (cydui_color)(u8(&r) | (u8(&r) << 8));
            uint8_t seed[8];
            size_t i;
            for (i = 0; i < sizeof(seed); i++) {
                seed[i] = u8(&r);
            }
            for (i = 0; i < 256; i++) { /* a short seed repeated: cheap, but not a flat color */
                bitmap_mem[i] = (uint16_t)(seed[(2 * i) & 7u] | (seed[(2 * i + 1) & 7u] << 8));
            }
            cydui_draw_bitmap(&ctx, q, bitmap_mem, len, fmt, tint, u8(&r));
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
