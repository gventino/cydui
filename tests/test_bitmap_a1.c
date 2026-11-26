/* CYDUI_BITMAP_A1: one bit per pixel, tinted, with an integer scale. */

#include <string.h>

#include "testutil.h"
#include "unity.h"

#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED 0xF800

void setUp(void) {}
void tearDown(void) {}

/* ---- size ---- */

void test_the_size_of_an_a1_bitmap_is_its_rows_padded_to_a_byte(void)
{
    TEST_ASSERT_EQUAL_size_t(1, cydui_bitmap_size(CYDUI_BITMAP_A1, 1, 1));
    TEST_ASSERT_EQUAL_size_t(1, cydui_bitmap_size(CYDUI_BITMAP_A1, 8, 1));
    TEST_ASSERT_EQUAL_size_t(2, cydui_bitmap_size(CYDUI_BITMAP_A1, 9, 1));
    TEST_ASSERT_EQUAL_size_t(6, cydui_bitmap_size(CYDUI_BITMAP_A1, 9, 3));
    TEST_ASSERT_EQUAL_size_t(0, cydui_bitmap_size(CYDUI_BITMAP_A1, 0, 3));
    TEST_ASSERT_EQUAL_size_t(0, cydui_bitmap_size(CYDUI_BITMAP_A1, 3, -1));
    TEST_ASSERT_EQUAL_size_t(0, cydui_bitmap_size((cydui_bitmap_format)7, 3, 3));
    TEST_ASSERT_EQUAL_INT(2, CYDUI_BITMAP_A1);
}

/* ---- pixels ---- */

static const uint8_t ROW[1] = {0xA0}; /* 1010 0000: bits 0 and 2 of 8 */
static void scene_row(cydui_ctx *c) { cydui_draw_bitmap_a1(c, 10, 10, 8, 1, 1, ROW, sizeof(ROW), WHITE, 255); }

void test_the_leftmost_pixel_is_the_most_significant_bit_and_a_zero_draws_nothing(void)
{
    int x;
    tu_render(scene_row, TU_H, BLACK);
    for (x = 10; x < 18; x++) {
        TEST_ASSERT_EQUAL_HEX16_MESSAGE((x == 10 || x == 12) ? WHITE : BLACK, tu_px(x, 10), "pixel");
    }
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(9, 10));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(18, 10));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(10, 9));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(10, 11));
}

static const uint8_t NINE[4] = {0x80, 0x00, 0x00, 0x80}; /* 9 bits wide, 2 rows: row 0 = bit 0, row 1 = bit 8 */
static void scene_nine(cydui_ctx *c) { cydui_draw_bitmap_a1(c, 0, 0, 9, 2, 1, NINE, sizeof(NINE), RED, 255); }

void test_a_row_wider_than_a_byte_continues_in_the_next_byte(void)
{
    tu_render(scene_nine, TU_H, BLACK);
    TEST_ASSERT_EQUAL_HEX16(RED, tu_px(0, 0));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(8, 0));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(0, 1));
    TEST_ASSERT_EQUAL_HEX16(RED, tu_px(8, 1));
}

static const uint8_t DIAG[2] = {0x80, 0x40}; /* 2 x 2: top-left and bottom-right */
static void scene_scaled(cydui_ctx *c) { cydui_draw_bitmap_a1(c, 20, 30, 2, 2, 3, DIAG, sizeof(DIAG), WHITE, 255); }

void test_every_bit_becomes_a_square_of_scale_pixels(void)
{
    int x, y;
    tu_render(scene_scaled, TU_H, BLACK);
    for (y = 28; y < 38; y++) {
        for (x = 18; x < 28; x++) {
            int bx = x - 20, by = y - 30;
            cydui_color want = BLACK;
            if (bx >= 0 && bx < 6 && by >= 0 && by < 6) {
                want = ((bx < 3) == (by < 3)) ? WHITE : BLACK;
            }
            TEST_ASSERT_EQUAL_HEX16_MESSAGE(want, tu_px(x, y), "scaled pixel");
        }
    }
}

static void scene_clipped(cydui_ctx *c)
{
    cydui_draw_bitmap_a1(c, -5, -2, 2, 2, 4, DIAG, sizeof(DIAG), WHITE, 255); /* bit (0,0) is x -5..-2: gone; bit (1,1) is x -1..2, y 2..5 */
}

void test_a_bitmap_partly_off_the_screen_shows_the_part_that_is_on_it(void)
{
    tu_render(scene_clipped, TU_H, BLACK);
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(0, 2));
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(2, 5));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(3, 2));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(0, 1));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(0, 6));
}

static void scene_translucent(cydui_ctx *c) { cydui_draw_bitmap_a1(c, 0, 0, 2, 2, 2, DIAG, sizeof(DIAG), WHITE, 128); }

void test_alpha_blends_the_tint_and_zero_alpha_draws_nothing(void)
{
    cydui_color px;
    tu_render(scene_translucent, TU_H, BLACK);
    px = tu_px(0, 0);
    TEST_ASSERT_TRUE(px != BLACK && px != WHITE);
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(3, 0));
}

static void scene_invisible(cydui_ctx *c) { cydui_draw_bitmap_a1(c, 0, 0, 2, 2, 2, DIAG, sizeof(DIAG), WHITE, 0); }

void test_a_bitmap_with_alpha_zero_changes_nothing(void)
{
    tu_render(scene_invisible, TU_H, BLACK);
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(0, 0));
}

/* ---- the same picture whatever the strips ---- */

static uint8_t PATTERN[13 * 2]; /* 13 bits wide: two bytes a row, 13 rows */
static void scene_pattern(cydui_ctx *c) { cydui_draw_bitmap_a1(c, 7, 9, 13, 13, 5, PATTERN, sizeof(PATTERN), RED, 200); }

void test_the_picture_does_not_depend_on_how_the_screen_is_split_into_strips(void)
{
    static const int heights[] = {1, 2, 3, 7, 8, 16, 33, 240};
    uint64_t first = 0;
    size_t i;
    for (i = 0; i < sizeof(PATTERN); i++) {
        PATTERN[i] = (uint8_t)(i * 37u + 11u);
    }
    for (i = 0; i < sizeof(heights) / sizeof(heights[0]); i++) {
        uint64_t h = tu_render(scene_pattern, heights[i], BLACK);
        if (i == 0) {
            first = h;
        }
        TEST_ASSERT_TRUE_MESSAGE(h == first, "same hash for any strip height");
    }
}

/* ---- the same as an A4 mask of the drawn size ---- */

#define MODULES 21
#define SCALE 4
static uint8_t QR1[((MODULES + 7) / 8) * MODULES];
static uint8_t QR4[((MODULES * SCALE + 1) / 2) * MODULES * SCALE];
static unsigned alpha_under_test;

static bool module(int mx, int my)
{
    return ((mx * 7 + my * 13 + (mx ^ my)) % 3) == 0;
}

static void build_qr(void)
{
    int mx, my, x, y;
    memset(QR1, 0, sizeof(QR1));
    memset(QR4, 0, sizeof(QR4));
    for (my = 0; my < MODULES; my++) {
        for (mx = 0; mx < MODULES; mx++) {
            if (module(mx, my)) {
                QR1[my * ((MODULES + 7) / 8) + mx / 8] |= (uint8_t)(0x80u >> (mx % 8));
            }
        }
    }
    for (y = 0; y < MODULES * SCALE; y++) {
        for (x = 0; x < MODULES * SCALE; x++) {
            if (module(x / SCALE, y / SCALE)) {
                size_t i = (size_t)y * ((MODULES * SCALE + 1) / 2) + (size_t)(x / 2);
                QR4[i] |= (uint8_t)((x & 1) ? 0x0F : 0xF0);
            }
        }
    }
}

static void scene_qr_a1(cydui_ctx *c) { cydui_draw_bitmap_a1(c, 31, 17, MODULES, MODULES, SCALE, QR1, sizeof(QR1), 0x1234, (uint8_t)alpha_under_test); }
static void scene_qr_a4(cydui_ctx *c)
{
    cydui_draw_bitmap(c, tu_rect(31, 17, MODULES * SCALE, MODULES * SCALE), QR4, sizeof(QR4), CYDUI_BITMAP_A4, 0x1234, (uint8_t)alpha_under_test);
}

void test_a_qr_code_as_a1_at_scale_4_is_pixel_for_pixel_the_a4_mask_of_its_drawn_size(void)
{
    static const unsigned alphas[] = {255, 200, 128, 9, 3, 0};
    size_t i;
    build_qr();
    TEST_ASSERT_TRUE(sizeof(QR1) * 50 < sizeof(QR4)); /* the point: a mask 1/50 of the size or less */
    for (i = 0; i < sizeof(alphas) / sizeof(alphas[0]); i++) {
        uint64_t a, b;
        alpha_under_test = alphas[i];
        a = tu_render(scene_qr_a1, 7, 0xFFFF);
        b = tu_render(scene_qr_a4, 7, 0xFFFF);
        TEST_ASSERT_TRUE_MESSAGE(a == b, "equal to the A4 drawing");
    }
}

/* ---- errors ---- */

static void check_error(cydui_ctx *c, uint32_t want, uint32_t *errs)
{
    (void)c;
    TEST_ASSERT_EQUAL_HEX32(want, *errs);
}

static uint32_t errors_after(int16_t w, int16_t h, uint8_t scale, const void *data, size_t len)
{
    static cydui_cmd cmds[8];
    static cydui_mem mem;
    static cydui_color fb[16 * 16], strip[16 * 4];
    cydui_ctx ctx;
    cydui_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cydui_mem_init(&mem, fb, 16, 16);
    cfg.backend = &mem.backend;
    cfg.cmd_mem = cmds;
    cfg.cmd_mem_size = sizeof(cmds);
    cfg.strip_mem = strip;
    cfg.strip_mem_size = sizeof(strip);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_init(&ctx, &cfg));
    cydui_begin_frame(&ctx, 0);
    cydui_draw_bitmap_a1(&ctx, 0, 0, w, h, scale, data, len, WHITE, 255);
    {
        uint32_t e = cydui_errors(&ctx);
        uint32_t n = cydui_cmd_count(&ctx);
        cydui_end_frame(&ctx);
        (void)check_error;
        return e | (n ? 0x80000000u : 0u); /* the top bit says a command was made */
    }
}

void test_wrong_arguments_are_dropped_with_the_right_flag_and_nothing_is_drawn(void)
{
    static const uint8_t d[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    TEST_ASSERT_EQUAL_HEX32(0x80000000u, errors_after(4, 4, 2, d, 4));                  /* good: one command, no error */
    TEST_ASSERT_EQUAL_HEX32(CYDUI_ERR_BAD_ARG, errors_after(4, 4, 0, d, 4));            /* scale 0 */
    TEST_ASSERT_EQUAL_HEX32(CYDUI_ERR_BAD_ARG, errors_after(4, 4, 9, d, 4));            /* scale 9 */
    TEST_ASSERT_EQUAL_HEX32(0x80000000u, errors_after(4, 4, 8, d, 4));                  /* scale 8 is the largest */
    TEST_ASSERT_EQUAL_HEX32(CYDUI_ERR_BAD_ARG, errors_after(4100, 1, 8, d, 8));         /* 32800 pixels: over an int16 */
    TEST_ASSERT_EQUAL_HEX32(CYDUI_ERR_BAD_ARG, errors_after(4096, 1, 8, d, 512));       /* 32768 pixels: one over */
    TEST_ASSERT_EQUAL_HEX32(CYDUI_ERR_BAD_BITMAP, errors_after(4, 4, 2, d, 3));         /* one byte short */
    TEST_ASSERT_EQUAL_HEX32(CYDUI_ERR_BAD_BITMAP, errors_after(9, 2, 1, d, 3));         /* 2 bytes a row: needs 4 */
    TEST_ASSERT_EQUAL_HEX32(CYDUI_ERR_BAD_BITMAP, errors_after(4, 4, 2, NULL, 4));
    TEST_ASSERT_EQUAL_HEX32(0, errors_after(0, 4, 2, d, 4));                            /* nothing to draw is not an error */
    TEST_ASSERT_EQUAL_HEX32(0, errors_after(4, -1, 2, d, 4));
}

void test_a_command_is_still_48_bytes(void)
{
    TEST_ASSERT_EQUAL_size_t(48, sizeof(cydui_cmd)); /* the memory budgets of every application depend on it */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_size_of_an_a1_bitmap_is_its_rows_padded_to_a_byte);
    RUN_TEST(test_the_leftmost_pixel_is_the_most_significant_bit_and_a_zero_draws_nothing);
    RUN_TEST(test_a_row_wider_than_a_byte_continues_in_the_next_byte);
    RUN_TEST(test_every_bit_becomes_a_square_of_scale_pixels);
    RUN_TEST(test_a_bitmap_partly_off_the_screen_shows_the_part_that_is_on_it);
    RUN_TEST(test_alpha_blends_the_tint_and_zero_alpha_draws_nothing);
    RUN_TEST(test_a_bitmap_with_alpha_zero_changes_nothing);
    RUN_TEST(test_the_picture_does_not_depend_on_how_the_screen_is_split_into_strips);
    RUN_TEST(test_a_qr_code_as_a1_at_scale_4_is_pixel_for_pixel_the_a4_mask_of_its_drawn_size);
    RUN_TEST(test_wrong_arguments_are_dropped_with_the_right_flag_and_nothing_is_drawn);
    RUN_TEST(test_a_command_is_still_48_bytes);
    return UNITY_END();
}
