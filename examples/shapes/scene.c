#include "scene.h"

/* Endpoints of the fan, as offsets from its center: 180 degrees in 15 degree steps, radius 70. */
static const int16_t fan[13][2] = {{-70, 0},  {-68, -18}, {-61, -35}, {-49, -49}, {-35, -61},
                                   {-18, -68}, {0, -70},   {18, -68},  {35, -61},  {49, -49},
                                   {61, -35},  {68, -18},  {70, 0}};

/* 16 columns -> 8 bytes per row, 4 rows: a left-to-right coverage ramp 0..F. */
static const uint8_t ramp_a4[32] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};

void shapes_draw(cydui_ctx *ctx)
{
    static cydui_color checker[16 * 16];
    cydui_rect r;
    int i, x, y;

    for (y = 0; y < 16; y++) {
        for (x = 0; x < 16; x++) {
            checker[y * 16 + x] = (((x >> 2) + (y >> 2)) & 1) ? CYDUI_RGB(240, 240, 240)
                                                              : CYDUI_RGB(200, 60, 60);
        }
    }

    /* background */
    r.x = 0; r.y = 0; r.w = 320; r.h = 240;
    cydui_draw_gradient(ctx, r, CYDUI_RGB(12, 16, 48), CYDUI_RGB(70, 20, 90), true, 255);

    /* cards */
    r.x = 10; r.y = 10; r.w = 150; r.h = 90;
    cydui_draw_rrect(ctx, r, 16, CYDUI_RGB(60, 80, 130), 255);
    cydui_draw_rrect_outline(ctx, r, 16, 2, CYDUI_RGB(230, 235, 255), 255);

    r.x = 10; r.y = 110; r.w = 150; r.h = 50;
    cydui_draw_rrect(ctx, r, 25, CYDUI_RGB(240, 150, 30), 255);

    r.x = 10; r.y = 170; r.w = 150; r.h = 60;
    cydui_draw_rrect_outline(ctx, r, 12, 4, CYDUI_RGB(40, 220, 220), 255);

    /* line fan from (240, 110) */
    for (i = 0; i < 13; i++) {
        cydui_color col = (i & 1) ? CYDUI_RGB(255, 220, 80) : CYDUI_RGB(120, 255, 160);
        cydui_draw_line(ctx, 240, 110, (int16_t)(240 + fan[i][0]), (int16_t)(110 + fan[i][1]),
                        (uint8_t)(1 + (i % 3 == 0)), col, 255);
    }

    /* translucent circles */
    r.y = 160; r.w = 60; r.h = 60;
    r.x = 180; cydui_draw_rrect(ctx, r, 30, CYDUI_RGB(255, 40, 40), 150);
    r.x = 210; cydui_draw_rrect(ctx, r, 30, CYDUI_RGB(40, 255, 40), 150);
    r.x = 240; cydui_draw_rrect(ctx, r, 30, CYDUI_RGB(40, 40, 255), 150);

    /* bitmaps */
    r.x = 292; r.y = 8; r.w = 16; r.h = 16;
    cydui_draw_bitmap(ctx, r, checker, sizeof(checker), CYDUI_BITMAP_RGB565, 0, 255);
    r.x = 292; r.y = 30; r.w = 16; r.h = 4;
    cydui_draw_bitmap(ctx, r, ramp_a4, sizeof(ramp_a4), CYDUI_BITMAP_A4, CYDUI_RGB(255, 255, 255),
                      255);
}
