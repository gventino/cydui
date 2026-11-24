/* Rasterizer behavior: exact pixels for simple cases, symmetry and clipping for the AA shapes. */

#include <string.h>

#include "testutil.h"
#include "unity.h"

#define WHITE 0xFFFFu
#define BLACK 0x0000u
#define RED CYDUI_RGB(255, 0, 0)

void setUp(void) {}
void tearDown(void) {}

/* ---- rect ---- */

static void scene_rect_partly_offscreen(cydui_ctx *c)
{
    cydui_draw_rect(c, tu_rect(-10, -10, 30, 30), WHITE, 255); /* visible: 0..19 */
    cydui_draw_rect(c, tu_rect(310, 230, 30, 30), RED, 255);   /* visible: 310..319 x 230..239 */
}

static void test_rect_is_clipped_to_the_screen(void)
{
    int x, y, white = 0, red = 0;

    tu_render(scene_rect_partly_offscreen, TU_H, BLACK);
    TEST_ASSERT_EQUAL_UINT32(0, tu_errors);
    for (y = 0; y < TU_H; y++) {
        for (x = 0; x < TU_W; x++) {
            cydui_color p = tu_px(x, y);
            if (p == WHITE) white++;
            else if (p == RED) red++;
            else TEST_ASSERT_EQUAL_HEX16(BLACK, p);
        }
    }
    TEST_ASSERT_EQUAL_INT(20 * 20, white);
    TEST_ASSERT_EQUAL_INT(10 * 10, red);
}

static void scene_rect_alpha(cydui_ctx *c)
{
    cydui_draw_rect(c, tu_rect(0, 0, 10, 10), WHITE, 128);
}

static void test_rect_alpha_blends_with_the_background(void)
{
    tu_render(scene_rect_alpha, TU_H, BLACK);
    TEST_ASSERT_UINT_WITHIN(1, 16, (tu_px(5, 5) >> 11) & 0x1Fu);
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(10, 10));
}

static void scene_clipped(cydui_ctx *c)
{
    cydui_set_clip(c, tu_rect(20, 20, 10, 10));
    cydui_draw_rect(c, tu_rect(0, 0, 100, 100), WHITE, 255);
    cydui_reset_clip(c);
    cydui_draw_rect(c, tu_rect(50, 50, 4, 4), RED, 255);
}

static void test_clip_rectangle_limits_drawing(void)
{
    int x, y, white = 0;

    tu_render(scene_clipped, TU_H, BLACK);
    for (y = 0; y < 100; y++) {
        for (x = 0; x < 100; x++) {
            if (tu_px(x, y) == WHITE) {
                white++;
                TEST_ASSERT_TRUE(x >= 20 && x < 30 && y >= 20 && y < 30);
            }
        }
    }
    TEST_ASSERT_EQUAL_INT(100, white);
    TEST_ASSERT_EQUAL_HEX16(RED, tu_px(51, 51)); /* clip was reset */
}

/* ---- rounded rect ---- */

static void scene_rrect(cydui_ctx *c)
{
    cydui_draw_rrect(c, tu_rect(10, 10, 100, 60), 20, WHITE, 255);
}

static void test_rrect_corners_are_cut_and_antialiased(void)
{
    int x, y, partial = 0;

    tu_render(scene_rrect, TU_H, BLACK);
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(10, 10));   /* extreme corner stays background */
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(109, 69));
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(60, 40));   /* center */
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(10, 40));   /* middle of the left edge is straight */
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(60, 10));   /* middle of the top edge */
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(9, 40));    /* nothing outside the rect */

    for (y = 10; y < 30; y++) {
        for (x = 10; x < 30; x++) {
            cydui_color p = tu_px(x, y);
            if (p != BLACK && p != WHITE) partial++;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(partial >= 10, "expected antialiased pixels along the corner arc");
}

static void test_rrect_is_symmetric(void)
{
    int x, y;

    tu_render(scene_rrect, TU_H, BLACK);
    for (y = 10; y < 70; y++) {
        for (x = 10; x < 110; x++) {
            TEST_ASSERT_EQUAL_HEX16(tu_px(x, y), tu_px(119 - x, y)); /* horizontal mirror */
            TEST_ASSERT_EQUAL_HEX16(tu_px(x, y), tu_px(x, 79 - y));  /* vertical mirror */
        }
    }
}

/*
 * Covered area of white-on-black pixels in 1/63 pixel units: the sum of the 6-bit green channel
 * (63 = fully covered). Integer only, the bundled Unity has no double support.
 */
static int32_t covered_area63(void)
{
    int32_t sum = 0;
    int i;
    for (i = 0; i < TU_W * TU_H; i++) {
        sum += (int32_t)((tu_fb[i] >> 5) & 0x3Fu);
    }
    return sum;
}

static void scene_circle(cydui_ctx *c)
{
    cydui_draw_rrect(c, tu_rect(100, 100, 60, 60), 30, WHITE, 255);
}

static void test_antialiased_area_matches_the_exact_geometry(void)
{
    /* Rounded rect 100x60, r=20: w*h - (4 - pi) * r^2 = 5656.7 */
    tu_render(scene_rrect, TU_H, BLACK);
    TEST_ASSERT_INT32_WITHIN(15 * 63, 356372, covered_area63()); /* 5656.7 * 63 */

    /* Circle of radius 30: pi * r^2 = 2827.4 */
    tu_render(scene_circle, TU_H, BLACK);
    TEST_ASSERT_INT32_WITHIN(15 * 63, 178126, covered_area63()); /* 2827.4 * 63 */
}

static void scene_pill_huge_radius(cydui_ctx *c)
{
    cydui_draw_rrect(c, tu_rect(10, 10, 100, 40), 255, WHITE, 255);
}

static void scene_pill_exact_radius(cydui_ctx *c)
{
    cydui_draw_rrect(c, tu_rect(10, 10, 100, 40), 20, WHITE, 255);
}

static void test_rrect_radius_is_clamped_to_half_the_smaller_side(void)
{
    uint64_t a = tu_render(scene_pill_huge_radius, TU_H, BLACK);
    uint64_t b = tu_render(scene_pill_exact_radius, TU_H, BLACK);
    TEST_ASSERT_TRUE(a == b);
}

static void scene_outline(cydui_ctx *c)
{
    cydui_draw_rrect_outline(c, tu_rect(10, 10, 100, 60), 12, 3, WHITE, 255);
}

static void test_outline_is_hollow(void)
{
    tu_render(scene_outline, TU_H, BLACK);
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(60, 10));  /* top edge, first row */
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(60, 12));  /* third row */
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(60, 13));  /* inside */
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(60, 40));  /* center */
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(10, 40));
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(12, 40));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(13, 40));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(10, 10));  /* rounded away */
}

static void scene_outline_too_thick(cydui_ctx *c)
{
    cydui_draw_rrect_outline(c, tu_rect(10, 10, 40, 20), 8, 100, WHITE, 255);
}

static void scene_filled_same(cydui_ctx *c)
{
    cydui_draw_rrect(c, tu_rect(10, 10, 40, 20), 8, WHITE, 255);
}

static void test_outline_thicker_than_the_shape_degrades_to_fill(void)
{
    uint64_t a = tu_render(scene_outline_too_thick, TU_H, BLACK);
    uint64_t b = tu_render(scene_filled_same, TU_H, BLACK);
    TEST_ASSERT_TRUE(a == b);
}

/* ---- line ---- */

static void scene_hline(cydui_ctx *c)
{
    cydui_draw_line(c, 10, 50, 100, 50, 1, WHITE, 255);
}

static void test_horizontal_line_is_crisp_and_exact(void)
{
    int x;

    tu_render(scene_hline, TU_H, BLACK);
    for (x = 10; x <= 100; x++) {
        TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(x, 50));
        TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(x, 49));
        TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(x, 51));
    }
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(9, 50));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(101, 50));
}

static void scene_diag(cydui_ctx *c)
{
    cydui_draw_line(c, 10, 10, 90, 90, 1, WHITE, 255);
}

static void test_diagonal_line_is_antialiased_and_symmetric(void)
{
    int x, y, partial = 0;

    tu_render(scene_diag, TU_H, BLACK);
    for (y = 0; y < 100; y++) {
        for (x = 0; x < 100; x++) {
            TEST_ASSERT_EQUAL_HEX16(tu_px(x, y), tu_px(y, x)); /* mirrored across the line */
            if (tu_px(x, y) != BLACK && tu_px(x, y) != WHITE) partial++;
        }
    }
    TEST_ASSERT_TRUE(partial > 40);
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(50, 50));
}

static void scene_thick_line(cydui_ctx *c)
{
    cydui_draw_line(c, 20, 50, 200, 50, 4, WHITE, 255);
}

static void test_thick_line_covers_its_width_symmetrically(void)
{
    int k;

    tu_render(scene_thick_line, TU_H, BLACK);
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(100, 49));
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(100, 50));
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(100, 51));
    for (k = 1; k <= 5; k++) {
        TEST_ASSERT_EQUAL_HEX16(tu_px(100, 50 - k), tu_px(100, 50 + k));
    }
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(100, 46));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(100, 54));
}

static void test_thick_line_area_matches_the_exact_geometry(void)
{
    /* 4 px wide, centers 180 px apart, round caps: 180*4 + pi*2^2 = 732.6 */
    tu_render(scene_thick_line, TU_H, BLACK);
    TEST_ASSERT_INT32_WITHIN(8 * 63, 46154, covered_area63()); /* 732.6 * 63 */
}

static void scene_line_extremes(cydui_ctx *c)
{
    cydui_draw_line(c, -32768, -32768, 32767, 32767, 3, WHITE, 255);
    cydui_draw_line(c, -30000, 100, 30000, 100, 255, RED, 255);
    cydui_draw_line(c, 5, 5, 5, 5, 3, WHITE, 255); /* a point */
}

static void test_extreme_lines_neither_crash_nor_hang(void)
{
    tu_render(scene_line_extremes, TU_H, BLACK);
    TEST_ASSERT_EQUAL_UINT32(0, tu_errors);
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(5, 5));
}

/* ---- bitmap ---- */

static const uint16_t bmp[4] = {0x1111, 0x2222, 0x3333, 0x4444};

static void scene_bitmap(cydui_ctx *c)
{
    cydui_draw_bitmap(c, tu_rect(5, 5, 2, 2), bmp, sizeof(bmp), CYDUI_BITMAP_RGB565, 0, 255);
    cydui_draw_bitmap(c, tu_rect(-1, 20, 2, 2), bmp, sizeof(bmp), CYDUI_BITMAP_RGB565, 0, 255);
}

static void test_rgb565_bitmap_is_copied_and_clipped(void)
{
    tu_render(scene_bitmap, TU_H, BLACK);
    TEST_ASSERT_EQUAL_HEX16(0x1111, tu_px(5, 5));
    TEST_ASSERT_EQUAL_HEX16(0x2222, tu_px(6, 5));
    TEST_ASSERT_EQUAL_HEX16(0x3333, tu_px(5, 6));
    TEST_ASSERT_EQUAL_HEX16(0x4444, tu_px(6, 6));
    /* at x = -1 only the right column (second pixel of each row) is visible */
    TEST_ASSERT_EQUAL_HEX16(0x2222, tu_px(0, 20));
    TEST_ASSERT_EQUAL_HEX16(0x4444, tu_px(0, 21));
}

static const uint8_t mask[2] = {0xF0, 0x84}; /* nibbles: F 0 8 4 */

static void scene_mask(cydui_ctx *c)
{
    cydui_draw_bitmap(c, tu_rect(10, 10, 4, 1), mask, sizeof(mask), CYDUI_BITMAP_A4, WHITE, 255);
}

static void test_a4_mask_tints_with_coverage(void)
{
    unsigned mid, quarter;

    tu_render(scene_mask, TU_H, BLACK);
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(10, 10)); /* F: full */
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(11, 10)); /* 0: untouched */
    mid = (tu_px(12, 10) >> 11) & 0x1Fu;           /* 8/15 ~ 53% of 31 */
    quarter = (tu_px(13, 10) >> 11) & 0x1Fu;       /* 4/15 ~ 27% */
    TEST_ASSERT_UINT_WITHIN(2, 16, mid);
    TEST_ASSERT_UINT_WITHIN(2, 8, quarter);
}

/* ---- gradient ---- */

static void scene_vgrad(cydui_ctx *c)
{
    cydui_draw_gradient(c, tu_rect(0, 0, 10, 50), BLACK, WHITE, true, 255);
}

static void scene_hgrad(cydui_ctx *c)
{
    cydui_draw_gradient(c, tu_rect(0, 0, 50, 10), WHITE, BLACK, false, 255);
}

static void test_gradient_hits_both_endpoints_and_is_monotonic(void)
{
    int y, x;
    unsigned prev = 0;

    tu_render(scene_vgrad, TU_H, 0x1234);
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(3, 0));
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(3, 49));
    TEST_ASSERT_EQUAL_HEX16(0x1234, tu_px(10, 0)); /* outside the rect */
    for (y = 0; y < 50; y++) {
        unsigned g = (tu_px(3, y) >> 5) & 0x3Fu;
        TEST_ASSERT_TRUE(g >= prev);
        TEST_ASSERT_EQUAL_HEX16(tu_px(3, y), tu_px(9, y)); /* constant along x */
        prev = g;
    }

    tu_render(scene_hgrad, TU_H, 0x1234);
    TEST_ASSERT_EQUAL_HEX16(WHITE, tu_px(0, 4));
    TEST_ASSERT_EQUAL_HEX16(BLACK, tu_px(49, 4));
    prev = 63;
    for (x = 0; x < 50; x++) {
        unsigned g = (tu_px(x, 4) >> 5) & 0x3Fu;
        TEST_ASSERT_TRUE(g <= prev);
        prev = g;
    }
}

static void scene_grad_one_row(cydui_ctx *c)
{
    cydui_draw_gradient(c, tu_rect(0, 0, 8, 1), RED, WHITE, true, 255);
}

static void test_one_pixel_gradient_does_not_divide_by_zero(void)
{
    tu_render(scene_grad_one_row, TU_H, BLACK);
    TEST_ASSERT_EQUAL_HEX16(RED, tu_px(0, 0));
}

/* ---- the result must not depend on the strip height ---- */

static void scene_everything(cydui_ctx *c)
{
    scene_rrect(c);
    scene_outline(c);
    scene_diag(c);
    scene_thick_line(c);
    scene_hline(c);
    scene_vgrad(c);
    scene_bitmap(c);
    scene_mask(c);
    cydui_draw_rect(c, tu_rect(150, 10, 80, 200), RED, 100);
    cydui_set_clip(c, tu_rect(100, 100, 100, 50));
    cydui_draw_rrect(c, tu_rect(90, 90, 150, 100), 30, WHITE, 180);
}

static void test_strip_height_does_not_change_the_image(void)
{
    static const int heights[] = {1, 2, 3, 7, 16, 40, 100, 239, 240};
    uint64_t ref = tu_render(scene_everything, TU_H, 0x0841);
    size_t i;

    TEST_ASSERT_EQUAL_UINT32(0, tu_errors);
    for (i = 0; i < sizeof(heights) / sizeof(heights[0]); i++) {
        uint64_t h = tu_render(scene_everything, heights[i], 0x0841);
        TEST_ASSERT_TRUE_MESSAGE(h == ref, "image changed with the strip height");
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rect_is_clipped_to_the_screen);
    RUN_TEST(test_rect_alpha_blends_with_the_background);
    RUN_TEST(test_clip_rectangle_limits_drawing);
    RUN_TEST(test_rrect_corners_are_cut_and_antialiased);
    RUN_TEST(test_rrect_is_symmetric);
    RUN_TEST(test_antialiased_area_matches_the_exact_geometry);
    RUN_TEST(test_rrect_radius_is_clamped_to_half_the_smaller_side);
    RUN_TEST(test_outline_is_hollow);
    RUN_TEST(test_outline_thicker_than_the_shape_degrades_to_fill);
    RUN_TEST(test_horizontal_line_is_crisp_and_exact);
    RUN_TEST(test_diagonal_line_is_antialiased_and_symmetric);
    RUN_TEST(test_thick_line_covers_its_width_symmetrically);
    RUN_TEST(test_thick_line_area_matches_the_exact_geometry);
    RUN_TEST(test_extreme_lines_neither_crash_nor_hang);
    RUN_TEST(test_rgb565_bitmap_is_copied_and_clipped);
    RUN_TEST(test_a4_mask_tints_with_coverage);
    RUN_TEST(test_gradient_hits_both_endpoints_and_is_monotonic);
    RUN_TEST(test_one_pixel_gradient_does_not_divide_by_zero);
    RUN_TEST(test_strip_height_does_not_change_the_image);
    return UNITY_END();
}
