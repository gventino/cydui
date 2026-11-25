/*
 * The calibration of the touch screen (FR-42, CT-06): the fit, its checks, the rounding and clamping of
 * the result, the default map, and the blob that goes to the flash. The true map used throughout is the
 * one of a CYD: raw Y is horizontal and raw X vertical,
 *     x = 0.1 * raw_y - 40        y = 0.075 * raw_x - 30
 * (so raw 400..3600 covers 0..320 and -0..240), the five targets are 20 px in from the corners and the center.
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "touch_cal.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static const float tx[5] = {20, 300, 20, 300, 160};
static const float ty[5] = {20, 20, 220, 220, 120};

static cydui_cal_sample ideal(int i)
{
    cydui_cal_sample s;
    s.x = tx[i];
    s.y = ty[i];
    s.raw_y = (tx[i] + 40.0f) / 0.1f;
    s.raw_x = (ty[i] + 30.0f) / 0.075f;
    return s;
}

static void ideal5(cydui_cal_sample *p)
{
    int i;
    for (i = 0; i < 5; i++) p[i] = ideal(i);
}

static void test_the_fit_recovers_the_map_it_was_made_from(void)
{
    cydui_cal_sample p[5];
    cydui_touch_cal c;
    float worst = -1;

    ideal5(p);
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_OK, cydui_cal_fit(&c, p, 5, 3.0f, &worst));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, c.m[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.1f, c.m[1]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -40.0f, c.m[2]);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.075f, c.m[3]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, c.m[4]);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -30.0f, c.m[5]);
    TEST_ASSERT_TRUE(worst < 0.01f);
}

static void test_three_points_are_enough_and_a_touch_with_noise_stays_within_a_few_pixels(void)
{
    static const float noise[5][2] = {{9, -12}, {-14, 7}, {11, 13}, {-8, -10}, {6, -9}}; /* raw units, a few px */
    cydui_cal_sample p[5];
    cydui_touch_cal c;
    float worst;
    int i;

    ideal5(p);
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_OK, cydui_cal_fit(&c, p, 3, 0.5f, &worst)); /* exactly determined */
    for (i = 0; i < 5; i++) {
        p[i].raw_x += noise[i][0];
        p[i].raw_y += noise[i][1];
    }
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_OK, cydui_cal_fit(&c, p, 5, 4.0f, &worst));
    TEST_ASSERT_TRUE(worst > 0.2f && worst < 4.0f);
    for (i = 0; i < 5; i++) { /* the map put on the targets, from the raw readings that had noise */
        int16_t x, y;
        cydui_cal_apply(&c, (int)p[i].raw_x, (int)p[i].raw_y, 320, 240, &x, &y);
        TEST_ASSERT_INT_WITHIN(4, (int)tx[i], x);
        TEST_ASSERT_INT_WITHIN(4, (int)ty[i], y);
    }
}

static void test_a_wild_touch_is_refused_by_the_residual(void)
{
    cydui_cal_sample p[5];
    cydui_touch_cal c, before;
    float worst = 0;

    ideal5(p);
    p[4].raw_y += 1500.0f; /* the finger slipped: 150 px off */
    memset(&c, 0x5A, sizeof(c));
    before = c;
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_RESIDUAL, cydui_cal_fit(&c, p, 5, 12.0f, &worst));
    TEST_ASSERT_TRUE(worst > 30.0f);
    TEST_ASSERT_EQUAL_MEMORY(&before, &c, sizeof(c)); /* nothing was written */
}

static void test_the_residual_limit_is_exact(void)
{
    cydui_cal_sample p[5];
    cydui_touch_cal c;
    float worst = 0, limit;

    ideal5(p);
    p[4].raw_y += 90.0f; /* 9 px */
    (void)cydui_cal_fit(&c, p, 5, 1000.0f, &worst);
    TEST_ASSERT_TRUE(worst > 1.0f);
    limit = worst;
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_OK, cydui_cal_fit(&c, p, 5, limit, NULL));            /* equal is accepted */
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_RESIDUAL, cydui_cal_fit(&c, p, 5, limit * 0.999f, NULL));
}

static void test_points_that_do_not_span_the_plane_are_degenerate(void)
{
    cydui_cal_sample p[5];
    cydui_touch_cal c;
    int i;

    for (i = 0; i < 5; i++) { /* all on a line */
        p[i].raw_x = 1000.0f + 400.0f * (float)i;
        p[i].raw_y = 1000.0f + 400.0f * (float)i;
        p[i].x = 10.0f + 50.0f * (float)i;
        p[i].y = 10.0f + 40.0f * (float)i;
    }
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_DEGENERATE, cydui_cal_fit(&c, p, 5, 1000.0f, NULL));
    for (i = 0; i < 5; i++) { /* all the same */
        p[i].raw_x = p[i].raw_y = 2000.0f;
        p[i].x = p[i].y = 100.0f;
    }
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_DEGENERATE, cydui_cal_fit(&c, p, 5, 1000.0f, NULL));
    ideal5(p); /* a map that squashes the screen to a line: fine on the plane, not invertible */
    for (i = 0; i < 5; i++) {
        p[i].x = 100.0f;
        p[i].y = 100.0f;
    }
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_DEGENERATE, cydui_cal_fit(&c, p, 5, 1000.0f, NULL));
}

static void test_bad_arguments_and_numbers_are_refused(void)
{
    cydui_cal_sample p[CYDUI_CAL_MAX_POINTS + 1];
    cydui_touch_cal c;
    int i;

    for (i = 0; i < CYDUI_CAL_MAX_POINTS + 1; i++) {
        p[i] = ideal(i % 5);
        p[i].raw_x += (float)(i * 3);
    }
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_ARG, cydui_cal_fit(NULL, p, 5, 10.0f, NULL));
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_ARG, cydui_cal_fit(&c, NULL, 5, 10.0f, NULL));
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_ARG, cydui_cal_fit(&c, p, 2, 10.0f, NULL));
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_ARG, cydui_cal_fit(&c, p, 0, 10.0f, NULL));
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_ARG, cydui_cal_fit(&c, p, -3, 10.0f, NULL));
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_ARG, cydui_cal_fit(&c, p, CYDUI_CAL_MAX_POINTS + 1, 10.0f, NULL));
    ideal5(p);
    p[2].raw_x = NAN;
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_NOT_FINITE, cydui_cal_fit(&c, p, 5, 10.0f, NULL));
    ideal5(p);
    p[3].y = INFINITY;
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_NOT_FINITE, cydui_cal_fit(&c, p, 5, 10.0f, NULL));
    ideal5(p);
    TEST_ASSERT_EQUAL_INT(CYDUI_CAL_ERR_RESIDUAL, cydui_cal_fit(&c, p, 5, NAN, NULL)); /* NaN limit accepts nothing */
}

static void test_applying_rounds_to_the_nearest_pixel_and_clamps_to_the_screen(void)
{
    cydui_touch_cal c = {{1.0f, 0.0f, 0.5f, 0.0f, 1.0f, 0.49f}};
    int16_t x, y;

    cydui_cal_apply(&c, 10, 10, 320, 240, &x, &y);
    TEST_ASSERT_EQUAL_INT16(11, x); /* 10.5 rounds up */
    TEST_ASSERT_EQUAL_INT16(10, y); /* 10.49 down */
    cydui_cal_apply(&c, -500, -500, 320, 240, &x, &y);
    TEST_ASSERT_EQUAL_INT16(0, x);
    TEST_ASSERT_EQUAL_INT16(0, y);
    cydui_cal_apply(&c, 9000, 9000, 320, 240, &x, &y);
    TEST_ASSERT_EQUAL_INT16(319, x);
    TEST_ASSERT_EQUAL_INT16(239, y);
    cydui_cal_apply(&c, 319, 239, 320, 240, &x, &y); /* the last pixel is reachable */
    TEST_ASSERT_EQUAL_INT16(319, x);
    TEST_ASSERT_EQUAL_INT16(239, y);
    cydui_cal_apply(&c, 0, 0, 320, 240, &x, &y); /* 0.5 rounds to 1, 0.49 to 0 */
    TEST_ASSERT_EQUAL_INT16(1, x);
    TEST_ASSERT_EQUAL_INT16(0, y);
}

static void test_the_default_map_of_a_cyd_puts_the_finger_under_the_dot(void)
{
    cydui_touch_cal c;
    int16_t x, y;

    cydui_cal_default(&c, 320, 240, 400, 3600, true, false, false);
    TEST_ASSERT_TRUE(cydui_cal_valid(&c));
    cydui_cal_apply(&c, 400, 400, 320, 240, &x, &y); /* raw X is vertical, raw Y horizontal */
    TEST_ASSERT_EQUAL_INT16(0, x);
    TEST_ASSERT_EQUAL_INT16(0, y);
    cydui_cal_apply(&c, 2000, 2000, 320, 240, &x, &y);
    TEST_ASSERT_EQUAL_INT16(160, x);
    TEST_ASSERT_EQUAL_INT16(120, y);
    cydui_cal_apply(&c, 3600, 400, 320, 240, &x, &y); /* raw X at the top of its range: the bottom of the screen */
    TEST_ASSERT_EQUAL_INT16(0, x);
    TEST_ASSERT_EQUAL_INT16(239, y);
    cydui_cal_apply(&c, 400, 3600, 320, 240, &x, &y);
    TEST_ASSERT_EQUAL_INT16(319, x);
    TEST_ASSERT_EQUAL_INT16(0, y);

    cydui_cal_default(&c, 320, 240, 400, 3600, true, true, true); /* the screen turned over */
    cydui_cal_apply(&c, 400, 400, 320, 240, &x, &y);
    TEST_ASSERT_EQUAL_INT16(319, x);
    TEST_ASSERT_EQUAL_INT16(239, y);
    cydui_cal_apply(&c, 2000, 2000, 320, 240, &x, &y);
    TEST_ASSERT_EQUAL_INT16(160, x);
    TEST_ASSERT_EQUAL_INT16(120, y);

    cydui_cal_default(&c, 320, 240, 400, 3600, false, false, false); /* a panel whose axes are not swapped */
    cydui_cal_apply(&c, 2000, 400, 320, 240, &x, &y);
    TEST_ASSERT_EQUAL_INT16(160, x);
    TEST_ASSERT_EQUAL_INT16(0, y);
}

static void test_what_a_usable_map_is(void)
{
    cydui_touch_cal good = {{0.0f, 0.1f, -40.0f, 0.075f, 0.0f, -30.0f}}, bad;
    int i;

    TEST_ASSERT_TRUE(cydui_cal_valid(&good));
    TEST_ASSERT_FALSE(cydui_cal_valid(NULL));
    for (i = 0; i < 6; i++) {
        bad = good;
        bad.m[i] = NAN;
        TEST_ASSERT_FALSE(cydui_cal_valid(&bad));
        bad.m[i] = INFINITY;
        TEST_ASSERT_FALSE(cydui_cal_valid(&bad));
    }
    memset(&bad, 0, sizeof(bad)); /* everything zero: not invertible */
    TEST_ASSERT_FALSE(cydui_cal_valid(&bad));
    bad = good;
    bad.m[1] = 0.0f; /* raw Y no longer reaches x: determinant zero */
    TEST_ASSERT_FALSE(cydui_cal_valid(&bad));
    bad = good;
    bad.m[1] = 50.0f; /* a scale no touch screen has */
    TEST_ASSERT_FALSE(cydui_cal_valid(&bad));
    bad = good;
    bad.m[2] = 1e6f;
    TEST_ASSERT_FALSE(cydui_cal_valid(&bad));
}

static const uint8_t pinned[CYDUI_CAL_BLOB_SIZE] = { /* computed with Python's struct and zlib, not with this code */
    0x43, 0x41, 0x4C, 0x31, 0x00, 0x00, 0x00, 0x00, 0xCD, 0xCC, 0xCC, 0x3D, 0x00, 0x00, 0x20, 0xC2,
    0x9A, 0x99, 0x99, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xC1, 0x6F, 0xCC, 0x82, 0xA7};

static void test_the_blob_has_the_layout_it_was_given_and_round_trips(void)
{
    cydui_touch_cal good = {{0.0f, 0.1f, -40.0f, 0.075f, 0.0f, -30.0f}}, back;
    uint8_t blob[CYDUI_CAL_BLOB_SIZE];

    cydui_cal_pack(&good, blob);
    TEST_ASSERT_EQUAL_MEMORY(pinned, blob, sizeof(blob));
    memset(&back, 0, sizeof(back));
    TEST_ASSERT_TRUE(cydui_cal_unpack(&back, pinned, sizeof(pinned)));
    TEST_ASSERT_EQUAL_MEMORY(&good, &back, sizeof(good));
    TEST_ASSERT_EQUAL_HEX32(0xA782CC6Fu, cydui_crc32(pinned, 28));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, cydui_crc32((const uint8_t *)"123456789", 9)); /* the standard check value */
    TEST_ASSERT_EQUAL_HEX32(0u, cydui_crc32((const uint8_t *)"", 0));
}

static void test_a_damaged_blob_is_refused_whatever_the_damage(void)
{
    uint8_t b[CYDUI_CAL_BLOB_SIZE];
    cydui_touch_cal c, before;
    int byte, bit;

    memset(&c, 0x33, sizeof(c));
    before = c;
    for (byte = 0; byte < CYDUI_CAL_BLOB_SIZE; byte++) { /* every single bit of the 32 bytes */
        for (bit = 0; bit < 8; bit++) {
            memcpy(b, pinned, sizeof(b));
            b[byte] ^= (uint8_t)(1u << bit);
            TEST_ASSERT_FALSE_MESSAGE(cydui_cal_unpack(&c, b, sizeof(b)), "a flipped bit was accepted");
        }
    }
    TEST_ASSERT_EQUAL_MEMORY(&before, &c, sizeof(c));
    TEST_ASSERT_FALSE(cydui_cal_unpack(&c, pinned, sizeof(pinned) - 1));
    TEST_ASSERT_FALSE(cydui_cal_unpack(&c, pinned, sizeof(pinned) + 1));
    TEST_ASSERT_FALSE(cydui_cal_unpack(&c, pinned, 0));
    TEST_ASSERT_FALSE(cydui_cal_unpack(&c, NULL, sizeof(pinned)));
    TEST_ASSERT_FALSE(cydui_cal_unpack(NULL, pinned, sizeof(pinned)));
}

static void test_a_blob_with_a_good_crc_but_a_useless_map_is_refused(void)
{
    cydui_touch_cal nan_map = {{NAN, 0.1f, 0, 0.075f, 0, 0}}, zero_map;
    uint8_t b[CYDUI_CAL_BLOB_SIZE];
    cydui_touch_cal out;

    memset(&zero_map, 0, sizeof(zero_map));
    cydui_cal_pack(&nan_map, b); /* the CRC is right: the numbers are not */
    TEST_ASSERT_FALSE(cydui_cal_unpack(&out, b, sizeof(b)));
    cydui_cal_pack(&zero_map, b);
    TEST_ASSERT_FALSE(cydui_cal_unpack(&out, b, sizeof(b)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_fit_recovers_the_map_it_was_made_from);
    RUN_TEST(test_three_points_are_enough_and_a_touch_with_noise_stays_within_a_few_pixels);
    RUN_TEST(test_a_wild_touch_is_refused_by_the_residual);
    RUN_TEST(test_the_residual_limit_is_exact);
    RUN_TEST(test_points_that_do_not_span_the_plane_are_degenerate);
    RUN_TEST(test_bad_arguments_and_numbers_are_refused);
    RUN_TEST(test_applying_rounds_to_the_nearest_pixel_and_clamps_to_the_screen);
    RUN_TEST(test_the_default_map_of_a_cyd_puts_the_finger_under_the_dot);
    RUN_TEST(test_what_a_usable_map_is);
    RUN_TEST(test_the_blob_has_the_layout_it_was_given_and_round_trips);
    RUN_TEST(test_a_damaged_blob_is_refused_whatever_the_damage);
    RUN_TEST(test_a_blob_with_a_good_crc_but_a_useless_map_is_refused);
    return UNITY_END();
}
