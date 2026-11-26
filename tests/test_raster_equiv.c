/*
 * The rasterizer against its plain reference (tests/ref_raster.c): random strips of random commands of every
 * kind, with every alpha, clip, size and position the fuzzer-style generator can think of, must give the same
 * pixels. This is what lets the fast paths of src/raster.c stay fast without anyone having to trust them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/cmd.h"
#include "cydui_sans_12.h"
#include "cydui_sans_16.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void cydui_ref_raster_strip(const cydui_cmd *cmds, uint32_t n, cydui_color *buf, int16_t width, int16_t y0,
                            int16_t h, cydui_color clear);

static uint64_t rng = 88172645463325252ull;

static uint32_t rnd(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return (uint32_t)(rng >> 11);
}

static int rr(int lo, int hi)
{
    return lo + (int)(rnd() % (uint32_t)(hi - lo + 1));
}

static uint8_t mask[4096];
static uint16_t pix565[4096];

static cydui_cmd random_cmd(int w, int h, int y0)
{
    static const char *const texts[] = {"Hello, world!", "The quick brown fox jumps over the lazy dog", "  spaced  out ",
                                        "\xC3\xA1\xC3\xA7\xC3\xA3o \xE2\x82\xAC \xE2\x80\xA2 ok", "W",
                                        "a b c d e f g h i j k l m n o p"};
    cydui_cmd c;
    int al = rr(0, 6);

    memset(&c, 0, sizeof(c));
    c.type = (uint8_t)rr(0, 5);
    c.alpha = (uint8_t)(al < 3 ? 255 : (al == 3 ? rr(0, 255) : (al == 4 ? rr(240, 255) : rr(1, 60))));
    c.rect = (cydui_rect){(int16_t)rr(-20, w + 10), (int16_t)(y0 + rr(-20, h + 10)), (int16_t)rr(0, rnd() % 4 ? 120 : 330),
                          (int16_t)rr(0, rnd() % 4 ? 60 : 90)};
    if (rnd() % 5 == 0) {
        c.rect.w = (int16_t)rr(0, 8);
    }
    c.clip = rnd() % 3 ? (cydui_rect){0, (int16_t)(y0 - 3), (int16_t)(w + 3), (int16_t)(h + 6)}
                       : (cydui_rect){(int16_t)rr(0, w), (int16_t)(y0 + rr(-3, h)), (int16_t)rr(0, w), (int16_t)rr(0, h + 3)};
    switch (c.type) {
    case CYDUI_CMD_RECT:
        c.u.rect.color = (cydui_color)rnd();
        break;
    case CYDUI_CMD_RRECT:
        c.u.rrect.color = (cydui_color)rnd();
        c.u.rrect.radius = (uint8_t)(rnd() % 4 ? rr(0, 30) : rr(0, 255));
        c.u.rrect.thickness = (uint8_t)(rnd() % 2 ? 0 : rr(0, 12));
        break;
    case CYDUI_CMD_LINE: {
        int x0, x1, ya, yb;
        c.u.line.x0 = (int16_t)rr(-30, w + 30);
        c.u.line.y0 = (int16_t)(y0 + rr(-30, h + 30));
        c.u.line.x1 = (int16_t)(rnd() % 6 ? rr(-30, w + 30) : c.u.line.x0);
        c.u.line.y1 = (int16_t)(rnd() % 6 ? y0 + rr(-30, h + 30) : c.u.line.y0);
        if (rnd() % 8 == 0) c.u.line.y1 = c.u.line.y0; /* horizontal */
        if (rnd() % 8 == 0) c.u.line.x1 = c.u.line.x0; /* vertical, or a dot */
        c.u.line.color = (cydui_color)rnd();
        c.u.line.thickness = (uint8_t)rr(1, 9);
        x0 = c.u.line.x0 < c.u.line.x1 ? c.u.line.x0 : c.u.line.x1;
        x1 = c.u.line.x0 < c.u.line.x1 ? c.u.line.x1 : c.u.line.x0;
        ya = c.u.line.y0 < c.u.line.y1 ? c.u.line.y0 : c.u.line.y1;
        yb = c.u.line.y0 < c.u.line.y1 ? c.u.line.y1 : c.u.line.y0;
        c.rect = (cydui_rect){(int16_t)(x0 - 10), (int16_t)(ya - 10), (int16_t)(x1 - x0 + 20), (int16_t)(yb - ya + 20)};
        break;
    }
    case CYDUI_CMD_BITMAP:
        c.rect.w = (int16_t)rr(1, 60);
        c.rect.h = (int16_t)rr(1, 60);
        if (rnd() % 3 == 0) { /* a 1 bit mask, every bit a square of 1 to 8 pixels */
            int scale = rr(1, 8);
            c.u.bitmap.format = CYDUI_BITMAP_A1;
            c.u.bitmap.scale = (uint8_t)scale;
            c.u.bitmap.tint = (cydui_color)rnd();
            c.u.bitmap.data = mask;
            c.rect.w = (int16_t)(rr(1, 40) * scale);
            c.rect.h = (int16_t)(rr(1, 40) * scale);
        } else if (rnd() % 2) {
            c.u.bitmap.format = CYDUI_BITMAP_A4;
            c.u.bitmap.tint = (cydui_color)rnd();
            c.u.bitmap.data = mask;
        } else {
            c.u.bitmap.format = CYDUI_BITMAP_RGB565;
            c.u.bitmap.data = pix565;
        }
        break;
    case CYDUI_CMD_GRADIENT:
        c.u.gradient.c0 = (cydui_color)rnd();
        c.u.gradient.c1 = (cydui_color)rnd();
        c.u.gradient.vertical = (uint8_t)(rnd() % 2);
        break;
    default: /* text */
        c.u.text.font = rnd() % 2 ? &cydui_sans_16 : &cydui_sans_12;
        c.u.text.str = texts[rnd() % 6];
        c.u.text.len = (uint16_t)strlen(c.u.text.str);
        c.u.text.wrap = (int16_t)(rnd() % 2 ? 0 : rr(10, 150));
        c.u.text.color = (cydui_color)rnd();
        c.u.text.align = (uint8_t)rr(0, 2);
        break;
    }
    return c;
}

#define ITERATIONS 20000

static cydui_color got[400 * 30], want[400 * 30];

static void test_random_strips_match_the_plain_rasterizer(void)
{
    int it, i;

    for (i = 0; i < 4096; i++) {
        mask[i] = (uint8_t)(rnd() % 3 == 0 ? 0 : rnd());
        pix565[i] = (uint16_t)rnd();
    }
    for (it = 0; it < ITERATIONS; it++) {
        int w = rr(1, 330), h = rr(1, 24), y0 = rr(-5, 230), n = rr(1, 6);
        cydui_cmd cmds[6];
        cydui_color clear = (cydui_color)rnd();

        for (i = 0; i < n; i++) {
            cmds[i] = random_cmd(w, h, y0);
        }
        cydui_raster_strip(cmds, (uint32_t)n, got, (int16_t)w, (int16_t)y0, (int16_t)h, clear);
        cydui_ref_raster_strip(cmds, (uint32_t)n, want, (int16_t)w, (int16_t)y0, (int16_t)h, clear);
        if (memcmp(got, want, (size_t)w * (size_t)h * sizeof(cydui_color)) != 0) {
            char msg[96];
            snprintf(msg, sizeof(msg), "iteration %d (%d x %d at y %d, %d commands): pixels differ", it, w, h, y0, n);
            TEST_FAIL_MESSAGE(msg);
        }
    }
}

/* The shapes that the fast paths cut differently: thick outlines, radii past half the box, rows and columns of one pixel. */
static void test_corner_cases_match_the_plain_rasterizer(void)
{
    static const int radii[] = {0, 1, 2, 7, 8, 9, 15, 16, 60, 255};
    static const int thick[] = {0, 1, 2, 3, 8, 40};
    size_t a, b;

    for (a = 0; a < sizeof(radii) / sizeof(radii[0]); a++) {
        for (b = 0; b < sizeof(thick) / sizeof(thick[0]); b++) {
            int sz;
            for (sz = 1; sz <= 130; sz += (sz < 20 ? 1 : 37)) {
                cydui_cmd c;
                memset(&c, 0, sizeof(c));
                c.type = CYDUI_CMD_RRECT;
                c.alpha = (uint8_t)(b % 2 ? 255 : 97);
                c.rect = (cydui_rect){3, 2, (int16_t)sz, (int16_t)(sz % 7 + 1 + sz / 2)};
                c.clip = (cydui_rect){0, 0, 200, 40};
                c.u.rrect.color = 0xA55A;
                c.u.rrect.radius = (uint8_t)radii[a];
                c.u.rrect.thickness = (uint8_t)thick[b];
                cydui_raster_strip(&c, 1, got, 200, 0, 40, 0x1234);
                cydui_ref_raster_strip(&c, 1, want, 200, 0, 40, 0x1234);
                TEST_ASSERT_EQUAL_MEMORY(want, got, 200u * 40u * sizeof(cydui_color));
            }
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_random_strips_match_the_plain_rasterizer);
    RUN_TEST(test_corner_cases_match_the_plain_rasterizer);
    return UNITY_END();
}
