/* Golden images: each scene's framebuffer hash is pinned in tests/golden/<name>.hash. */

#include <string.h>

#include "golden.h"
#include "cydui_sans_12.h"
#include "cydui_sans_16.h"
#include "cydui_sans_24.h"
#include "scene.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define WHITE 0xFFFFu

static void scene_shapes(cydui_ctx *c)
{
    shapes_draw(c);
}

/* Corner radii from 0 up, outlines of several thicknesses, tiny and degenerate sizes. */
static void scene_rounded_cards(cydui_ctx *c)
{
    static const uint8_t radii[] = {0, 1, 2, 3, 5, 8, 12, 20};
    int i;

    cydui_draw_rect(c, tu_rect(0, 0, 320, 240), CYDUI_RGB(24, 28, 40), 255);
    for (i = 0; i < 8; i++) {
        cydui_rect r = tu_rect(8 + (i % 4) * 78, 8 + (i / 4) * 58, 70, 50);
        cydui_draw_rrect(c, r, radii[i], CYDUI_RGB(70 + i * 20, 120, 200 - i * 15), 255);
        cydui_draw_rrect_outline(c, r, radii[i], 1 + (i % 3), WHITE, 255);
    }
    /* thick outlines, a pill, a circle, and shapes too small for their radius */
    cydui_draw_rrect_outline(c, tu_rect(8, 128, 90, 50), 14, 6, CYDUI_RGB(255, 200, 40), 255);
    cydui_draw_rrect(c, tu_rect(110, 128, 100, 30), 255, CYDUI_RGB(60, 200, 120), 255);
    cydui_draw_rrect(c, tu_rect(220, 128, 80, 80), 40, CYDUI_RGB(220, 80, 120), 255);
    cydui_draw_rrect(c, tu_rect(110, 170, 5, 5), 20, WHITE, 255);
    cydui_draw_rrect(c, tu_rect(120, 170, 1, 12), 20, WHITE, 255);
    cydui_draw_rrect_outline(c, tu_rect(130, 170, 3, 3), 4, 2, WHITE, 255);
}

/* Lines in every direction, several thicknesses, with alpha. */
static void scene_lines_fan(cydui_ctx *c)
{
    static const int16_t dir[16][2] = {{100, 0},  {92, 38},  {71, 71},  {38, 92},
                                       {0, 100},  {-38, 92}, {-71, 71}, {-92, 38},
                                       {-100, 0}, {-92, -38}, {-71, -71}, {-38, -92},
                                       {0, -100}, {38, -92}, {71, -71}, {92, -38}};
    int i;

    cydui_draw_rect(c, tu_rect(0, 0, 320, 240), CYDUI_RGB(10, 10, 20), 255);
    for (i = 0; i < 16; i++) {
        cydui_draw_line(c, 110, 120, (int16_t)(110 + dir[i][0]), (int16_t)(120 + dir[i][1]),
                        (uint8_t)(1 + i % 4), CYDUI_RGB(80 + i * 10, 255 - i * 12, 120), 255);
    }
    /* shallow and steep slopes, translucent */
    for (i = 0; i < 8; i++) {
        cydui_draw_line(c, 220, (int16_t)(20 + i * 26), 310, (int16_t)(30 + i * 26 + i * 3), 3,
                        CYDUI_RGB(255, 255, 255), 120);
        cydui_draw_line(c, (int16_t)(225 + i * 11), 12, (int16_t)(228 + i * 11), 228, 1,
                        CYDUI_RGB(255, 120, 60), 255);
    }
    cydui_draw_line(c, 5, 5, 5, 5, 5, WHITE, 255);           /* a dot */
    cydui_draw_line(c, 300, 235, 310, 238, 8, WHITE, 255);   /* thick short line near the edge */
}

static void scene_gradient_bg(cydui_ctx *c)
{
    cydui_draw_gradient(c, tu_rect(0, 0, 320, 240), CYDUI_RGB(0, 30, 90), CYDUI_RGB(200, 60, 120),
                        true, 255);
    cydui_draw_gradient(c, tu_rect(20, 20, 130, 90), CYDUI_RGB(255, 255, 0), CYDUI_RGB(0, 120, 255),
                        false, 255);
    cydui_draw_gradient(c, tu_rect(170, 20, 130, 90), CYDUI_RGB(0, 0, 0), CYDUI_RGB(255, 255, 255),
                        true, 255);
    cydui_draw_gradient(c, tu_rect(20, 130, 280, 90), CYDUI_RGB(255, 0, 0), CYDUI_RGB(0, 255, 0),
                        false, 128); /* translucent over the background */
    cydui_draw_gradient(c, tu_rect(-20, -20, 60, 60), CYDUI_RGB(255, 255, 255), 0, true, 255);
}

static void scene_alpha_overlap(cydui_ctx *c)
{
    static const uint8_t alphas[] = {255, 220, 180, 140, 100, 60, 30, 10};
    int i;

    cydui_draw_rect(c, tu_rect(0, 0, 320, 120), CYDUI_RGB(240, 240, 240), 255);
    cydui_draw_rect(c, tu_rect(0, 120, 320, 120), CYDUI_RGB(20, 20, 20), 255);
    for (i = 0; i < 8; i++) {
        cydui_draw_rect(c, tu_rect(10 + i * 38, 10, 30, 220), CYDUI_RGB(255, 40, 40), alphas[i]);
        cydui_draw_rrect(c, tu_rect(10 + i * 38, 60 + i * 10, 30, 100), 10, CYDUI_RGB(40, 90, 255),
                         alphas[i]);
    }
}

static void scene_bitmap_mask(cydui_ctx *c)
{
    static cydui_color tile[24 * 24];
    static uint8_t mask[12 * 24]; /* 24 px wide, 12 bytes per row, 24 rows */
    int x, y;

    for (y = 0; y < 24; y++) {
        for (x = 0; x < 24; x++) {
            tile[y * 24 + x] = (cydui_color)(((x * 11) << 11) | ((y * 11) << 5) | ((x + y) & 31));
            /* soft disc: coverage 15 at the center falling to 0 */
            {
                int dx = x - 12, dy = y - 12, d2 = dx * dx + dy * dy;
                int cov = 15 - d2 / 10;
                cov = cov < 0 ? 0 : cov;
                if (x & 1) mask[y * 12 + x / 2] = (uint8_t)(mask[y * 12 + x / 2] | cov);
                else mask[y * 12 + x / 2] = (uint8_t)(cov << 4);
            }
        }
    }
    cydui_draw_rect(c, tu_rect(0, 0, 320, 240), CYDUI_RGB(30, 30, 50), 255);
    cydui_draw_bitmap(c, tu_rect(10, 10, 24, 24), tile, sizeof(tile), CYDUI_BITMAP_RGB565, 0, 255);
    cydui_draw_bitmap(c, tu_rect(50, 10, 24, 24), tile, sizeof(tile), CYDUI_BITMAP_RGB565, 0, 128);
    cydui_draw_bitmap(c, tu_rect(90, 10, 24, 24), mask, sizeof(mask), CYDUI_BITMAP_A4,
                      CYDUI_RGB(255, 200, 60), 255);
    cydui_draw_bitmap(c, tu_rect(130, 10, 24, 24), mask, sizeof(mask), CYDUI_BITMAP_A4, WHITE, 100);
    /* partly outside the screen and inside a clip rectangle */
    cydui_draw_bitmap(c, tu_rect(-12, 60, 24, 24), tile, sizeof(tile), CYDUI_BITMAP_RGB565, 0, 255);
    cydui_draw_bitmap(c, tu_rect(308, 60, 24, 24), mask, sizeof(mask), CYDUI_BITMAP_A4, WHITE, 255);
    cydui_set_clip(c, tu_rect(40, 100, 30, 30));
    cydui_draw_bitmap(c, tu_rect(30, 90, 24, 24), tile, sizeof(tile), CYDUI_BITMAP_RGB565, 0, 255);
    cydui_draw_bitmap(c, tu_rect(50, 110, 24, 24), mask, sizeof(mask), CYDUI_BITMAP_A4, WHITE, 255);
}

/* ---- text, set in the bundled fonts ---- */

static const cydui_font *const sans[3] = {&cydui_sans_12, &cydui_sans_16, &cydui_sans_24};

/* Draws `s` wrapped at 308 px and returns the y for the next block. */
static int put_line(cydui_ctx *c, const cydui_font *f, int y, const char *s, cydui_color color)
{
    cydui_text_extent e = cydui_text_size(f, s, CYDUI_NTS, 308);
    cydui_draw_text(c, f, 6, (int16_t)y, 308, CYDUI_ALIGN_LEFT, s, CYDUI_NTS, color, 255);
    return y + e.h + 4;
}

static void scene_text_sizes(cydui_ctx *c)
{
    int i, y = 4;

    cydui_draw_rect(c, tu_rect(0, 0, 320, 240), CYDUI_RGB(16, 20, 32), 255);
    for (i = 0; i < 3; i++) {
        y = put_line(c, sans[i], y, "Olá, mundo! Ação é ótima.", WHITE);
    }
    y = put_line(c, sans[0], y, "ÁÉÍÓÚ ÀÂÃÊÔÕ ÇÑ üÜ 0123456789 @#$%&*()[]{}<>", CYDUI_RGB(255, 210, 90));
    y = put_line(c, sans[1], y, "ÁÉÍÓÚ ÀÂÃÊÔÕ ÇÑ üÜ 0123", CYDUI_RGB(255, 210, 90));
    y = put_line(c, sans[2], y, "ÇÃO ÉÚ 0123", CYDUI_RGB(255, 210, 90));
    /* symbols in the subset, and one that is not (the check mark): it shows the fallback box */
    y = put_line(c, sans[1], y, "\xE2\x86\x90 \xE2\x86\x91 \xE2\x86\x92 \xE2\x86\x93  \xE2\x82\xAC  \xE2\x80\xA2  "
                              "\xE2\x80\xA6  \xE2\x9C\x93", CYDUI_RGB(120, 220, 160));
    (void)y;
}

static void scene_text_wrap(cydui_ctx *c)
{
    static const char para[] = "Manter o foco em uma lista curta de tarefas ajuda a terminar o dia "
                               "com a sensação de dever cumprido.";
    static const cydui_align al[3] = {CYDUI_ALIGN_LEFT, CYDUI_ALIGN_CENTER, CYDUI_ALIGN_RIGHT};
    int i;

    cydui_draw_rect(c, tu_rect(0, 0, 320, 240), CYDUI_RGB(16, 20, 32), 255);
    for (i = 0; i < 3; i++) { /* three boxes, one per alignment, with the box drawn around the text */
        cydui_rect box = tu_rect(8 + i * 104, 8, 96, 124);
        cydui_draw_rrect_outline(c, box, 4, 1, CYDUI_RGB(70, 90, 140), 255);
        cydui_draw_text(c, &cydui_sans_12, (int16_t)(box.x + 2), (int16_t)(box.y + 4), 92, al[i], para,
                        sizeof(para) - 1, WHITE, 255);
    }
    cydui_draw_text(c, &cydui_sans_12, 10, 136, 300, CYDUI_ALIGN_CENTER,
                    "Primeira linha\nSegunda linha, bem mais longa que a primeira\n\nQuarta, depois de um vazio",
                    CYDUI_NTS, CYDUI_RGB(255, 210, 90), 255);
    /* a clip rectangle cuts the text; the faint copy shows what is hidden */
    cydui_draw_text(c, &cydui_sans_16, 192, 206, 0, CYDUI_ALIGN_LEFT, "Texto cortado por clip", CYDUI_NTS,
                    CYDUI_RGB(70, 70, 80), 255);
    cydui_set_clip(c, tu_rect(200, 204, 100, 24));
    cydui_draw_text(c, &cydui_sans_16, 192, 206, 0, CYDUI_ALIGN_LEFT, "Texto cortado por clip", CYDUI_NTS,
                    WHITE, 255);
    cydui_reset_clip(c);
    cydui_draw_text(c, &cydui_sans_12, 6, 220, 180, CYDUI_ALIGN_LEFT, "translucido", CYDUI_NTS, WHITE, 110);
}

static void scene_text_hostile(cydui_ctx *c)
{
    static char long_text[300];
    static const char *const lines[] = {
        ("invalid: ab\xFF" "cd\xC3(" "ef\xE2\x82" "gh"),
        ("bidi: ab\xE2\x80\xAE" "cd\xE2\x81\xA6" "ef\xE2\x80\x8F" "gh"),
        ("ctrl: a\x01\x02\t\x7F\xC2\x85" "z"),
        "overlong: \xC0\x80 \xE0\x80\x80 \xF0\x80\x80\x80",
        "surrogate: \xED\xA0\x80 \xED\xBF\xBF end",
        "range: \xF4\x90\x80\x80 \xF5 end",
        "emoji, CJK: \xF0\x9F\x98\x80 \xE4\xB8\xAD\xE6\x96\x87 .",
        "cut off: x\xE2\x82",
        "fine again: Olá, ação!",
    };
    size_t i;
    int y = 4;

    cydui_draw_rect(c, tu_rect(0, 0, 320, 240), CYDUI_RGB(16, 20, 32), 255);
    for (i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
        y = put_line(c, &cydui_sans_12, y, lines[i], i + 1 == sizeof(lines) / sizeof(lines[0]) ? WHITE
                                                          : CYDUI_RGB(255, 170, 120));
    }
    memset(long_text, 'x', sizeof(long_text) - 1); /* 299 characters: cut to CYDUI_MAX_STR */
    long_text[299] = '\0';
    long_text[20] = ' ';
    long_text[60] = ' ';
    (void)put_line(c, &cydui_sans_12, y + 2, long_text, CYDUI_RGB(120, 220, 160));
}

static void test_shapes(void) { golden_check("shapes", scene_shapes); }
static void test_rounded_cards(void) { golden_check("rounded_cards", scene_rounded_cards); }
static void test_lines_fan(void) { golden_check("lines_fan", scene_lines_fan); }
static void test_gradient_bg(void) { golden_check("gradient_bg", scene_gradient_bg); }
static void test_alpha_overlap(void) { golden_check("alpha_overlap", scene_alpha_overlap); }
static void test_bitmap_mask(void) { golden_check("bitmap_mask", scene_bitmap_mask); }
static void test_text_sizes(void) { golden_check("text_sizes", scene_text_sizes); }
static void test_text_wrap(void) { golden_check("text_wrap", scene_text_wrap); }
static void test_text_hostile(void)
{
    golden_check("text_hostile", scene_text_hostile);
    /* the 299 character line must have been cut, and that must have been reported */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_TEXT_TRUNCATED, tu_errors);
}

/* Every golden scene must also be independent of the strip height. */
static void test_goldens_do_not_depend_on_strip_height(void)
{
    static const tu_scene scenes[] = {scene_shapes,        scene_rounded_cards, scene_lines_fan,
                                      scene_gradient_bg,   scene_alpha_overlap, scene_bitmap_mask,
                                      scene_text_sizes,    scene_text_wrap,     scene_text_hostile};
    static const int heights[] = {1, 5, 24, 40, 97};
    size_t i, k;

    for (i = 0; i < sizeof(scenes) / sizeof(scenes[0]); i++) {
        uint64_t ref = tu_render(scenes[i], TU_H, 0);
        for (k = 0; k < sizeof(heights) / sizeof(heights[0]); k++) {
            TEST_ASSERT_TRUE_MESSAGE(tu_render(scenes[i], heights[k], 0) == ref,
                                     "scene changed with the strip height");
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_shapes);
    RUN_TEST(test_rounded_cards);
    RUN_TEST(test_lines_fan);
    RUN_TEST(test_gradient_bg);
    RUN_TEST(test_alpha_overlap);
    RUN_TEST(test_bitmap_mask);
    RUN_TEST(test_text_sizes);
    RUN_TEST(test_text_wrap);
    RUN_TEST(test_text_hostile);
    RUN_TEST(test_goldens_do_not_depend_on_strip_height);
    return UNITY_END();
}
