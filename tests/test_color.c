#include "cydui/types.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_rgb_packing(void)
{
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, CYDUI_RGB(255, 255, 255));
    TEST_ASSERT_EQUAL_HEX16(0x0000, CYDUI_RGB(0, 0, 0));
    TEST_ASSERT_EQUAL_HEX16(0xF800, CYDUI_RGB(255, 0, 0));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, CYDUI_RGB(0, 255, 0));
    TEST_ASSERT_EQUAL_HEX16(0x001F, CYDUI_RGB(0, 0, 255));
}

static void test_unpack_round_trips_every_color(void)
{
    unsigned c;
    for (c = 0; c < 65536u; c++) {
        uint8_t r, g, b;
        cydui_color back;
        cydui_color_unpack((cydui_color)c, &r, &g, &b);
        back = CYDUI_RGB(r, g, b);
        TEST_ASSERT_EQUAL_HEX16((cydui_color)c, back);
    }
}

static void test_unpack_reaches_full_range(void)
{
    uint8_t r, g, b;
    cydui_color_unpack(0xFFFF, &r, &g, &b);
    TEST_ASSERT_EQUAL_UINT8(255, r);
    TEST_ASSERT_EQUAL_UINT8(255, g);
    TEST_ASSERT_EQUAL_UINT8(255, b);
    cydui_color_unpack(0, &r, &g, &b);
    TEST_ASSERT_EQUAL_UINT8(0, r);
    TEST_ASSERT_EQUAL_UINT8(0, g);
    TEST_ASSERT_EQUAL_UINT8(0, b);
}

static void test_blend_endpoints_are_exact(void)
{
    unsigned d, s;
    for (d = 0; d < 65536u; d += 251u) {
        for (s = 0; s < 65536u; s += 257u) {
            TEST_ASSERT_EQUAL_HEX16((cydui_color)d, cydui_blend((cydui_color)d, (cydui_color)s, 0));
            TEST_ASSERT_EQUAL_HEX16((cydui_color)s, cydui_blend((cydui_color)d, (cydui_color)s, 255));
        }
    }
}

static void test_blend_same_color_is_identity(void)
{
    unsigned c, a;
    for (c = 0; c < 65536u; c += 97u) {
        for (a = 0; a < 256u; a += 15u) {
            TEST_ASSERT_EQUAL_HEX16((cydui_color)c,
                                    cydui_blend((cydui_color)c, (cydui_color)c, (uint8_t)a));
        }
    }
}

static void test_blend_midpoint_and_monotonic(void)
{
    unsigned a;
    unsigned prev = 0;
    cydui_color mid = cydui_blend(0x0000, 0xFFFF, 128);

    /* About half of each channel: 5 bits -> ~15..16, 6 bits -> ~31..32. */
    TEST_ASSERT_UINT_WITHIN(1, 16, (mid >> 11) & 0x1Fu);
    TEST_ASSERT_UINT_WITHIN(1, 32, (mid >> 5) & 0x3Fu);
    TEST_ASSERT_UINT_WITHIN(1, 16, mid & 0x1Fu);

    for (a = 0; a < 256u; a++) {
        unsigned red = (cydui_blend(0x0000, 0xFFFF, (uint8_t)a) >> 11) & 0x1Fu;
        TEST_ASSERT_TRUE(red >= prev);
        prev = red;
    }
    TEST_ASSERT_EQUAL_UINT(31, prev);
}

static void test_rect_intersect(void)
{
    cydui_rect a = {0, 0, 10, 10}, b = {5, 5, 10, 10}, r;

    r = cydui_rect_intersect(a, b);
    TEST_ASSERT_EQUAL_INT(5, r.x);
    TEST_ASSERT_EQUAL_INT(5, r.y);
    TEST_ASSERT_EQUAL_INT(5, r.w);
    TEST_ASSERT_EQUAL_INT(5, r.h);

    b.x = 10; /* touching edges: empty */
    TEST_ASSERT_TRUE(cydui_rect_empty(cydui_rect_intersect(a, b)));

    b.x = -100; b.y = -100; b.w = 50; b.h = 50;
    TEST_ASSERT_TRUE(cydui_rect_empty(cydui_rect_intersect(a, b)));

    b.w = 0; b.x = 1; b.y = 1; b.h = 5; /* empty input */
    TEST_ASSERT_TRUE(cydui_rect_empty(cydui_rect_intersect(a, b)));
}

static void test_rect_intersect_extremes_do_not_overflow(void)
{
    cydui_rect a = {-32768, -32768, 32767, 32767};
    cydui_rect b = {32000, 32000, 767, 767};
    cydui_rect c = {-32768, -32768, 32767, 32767};
    cydui_rect r = cydui_rect_intersect(a, b);

    TEST_ASSERT_TRUE(cydui_rect_empty(r));
    r = cydui_rect_intersect(a, c);
    TEST_ASSERT_EQUAL_INT(32767, r.w);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rgb_packing);
    RUN_TEST(test_unpack_round_trips_every_color);
    RUN_TEST(test_unpack_reaches_full_range);
    RUN_TEST(test_blend_endpoints_are_exact);
    RUN_TEST(test_blend_same_color_is_identity);
    RUN_TEST(test_blend_midpoint_and_monotonic);
    RUN_TEST(test_rect_intersect);
    RUN_TEST(test_rect_intersect_extremes_do_not_overflow);
    return UNITY_END();
}
