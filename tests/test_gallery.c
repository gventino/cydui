/*
 * The touch-test page of the gallery (the one the board uses to check its calibration), driven by
 * synthetic taps. With the geometry of the `clean` theme (16 px font) the page is: tabs at y = 8..39, a row
 * of controls 34 px tall at y = 48..81 and the test area at (8, 90), 304 x 142, whose four targets are at
 * 25 % and 75 % of it: x = 84 and 236, y = 125 and 196 (found by looking at the picture and then pinned).
 */

#include <string.h>

#include "gallery.h"
#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;
static gallery g;

static void start(int tab)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    gallery_init(&g, 1, tab);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &g.themes[1]));
}

static void frame(void)
{
    cydui_ui_begin(&rig.ui, cydui_theme_of(&rig.ui)->style.bg);
    gallery_frame(&rig.ui, &g);
    cydui_ui_end(&rig.ui);
    gallery_apply(&rig.ui, &g);
}

static void settle(void)
{
    int i;
    for (i = 0; i < 4; i++) frame();
}

static void tap(int x, int y)
{
    rig_down(&rig, x, y);
    frame();
    rig_up(&rig, x, y);
    frame();
}

static void test_taps_on_the_targets_measure_zero_and_the_tap_after_the_fourth_starts_over(void)
{
    start(3);
    settle();
    tap(84, 125);
    TEST_ASSERT_EQUAL_INT(1, g.check_n);
    tap(236, 125);
    tap(84, 196);
    tap(236, 196);
    TEST_ASSERT_EQUAL_INT(4, g.check_n);
    TEST_ASSERT_EQUAL_INT(0, g.check_err[0] + g.check_err[1] + g.check_err[2] + g.check_err[3]);
    tap(100, 100); /* any tap in the area: again from the first */
    TEST_ASSERT_EQUAL_INT(0, g.check_n);
}

static void test_the_error_is_the_distance_in_pixels(void)
{
    start(3);
    settle();
    tap(84 + 3, 125 + 4); /* a 3-4-5 triangle */
    tap(236, 125 - 12);
    tap(84 - 6, 196 - 8);
    tap(236 + 20, 196);
    TEST_ASSERT_EQUAL_INT(5, g.check_err[0]);
    TEST_ASSERT_EQUAL_INT(12, g.check_err[1]);
    TEST_ASSERT_EQUAL_INT(10, g.check_err[2]);
    TEST_ASSERT_EQUAL_INT(20, g.check_err[3]);
}

static void test_the_guided_test_goes_from_targets_to_taps_to_a_verdict(void)
{
    int worst = -1, i;

    start(3);
    g.acceptance = true;
    settle();
    TEST_ASSERT_EQUAL_INT(GALLERY_ACCEPT_TARGETS, gallery_acceptance(&g, &worst));
    tap(84 + 3, 125 + 4); /* 5 px */
    tap(236, 125);
    tap(84, 196);
    TEST_ASSERT_EQUAL_INT(GALLERY_ACCEPT_TARGETS, gallery_acceptance(&g, &worst));
    TEST_ASSERT_EQUAL_INT(5, worst);
    tap(236, 196);
    g.touch.lat_n = GALLERY_ACCEPT_TAPS - 1;
    g.touch.lat_p95_us = 20000;
    TEST_ASSERT_EQUAL_INT(GALLERY_ACCEPT_TAPS_LEFT, gallery_acceptance(&g, &worst));
    tap(100, 100); /* in the guided test a tap after the four does not start over */
    TEST_ASSERT_EQUAL_INT(4, g.check_n);

    g.touch.lat_n = GALLERY_ACCEPT_TAPS;
    TEST_ASSERT_EQUAL_INT(GALLERY_ACCEPT_PASS, gallery_acceptance(&g, &worst));
    g.touch.lat_p95_us = GALLERY_ACCEPT_P95_US; /* the limit itself is not good enough */
    TEST_ASSERT_EQUAL_INT(GALLERY_ACCEPT_FAIL, gallery_acceptance(&g, &worst));
    g.touch.lat_p95_us = 20000;
    g.check_err[3] = GALLERY_ACCEPT_MAX_PX; /* the limit itself passes */
    TEST_ASSERT_EQUAL_INT(GALLERY_ACCEPT_PASS, gallery_acceptance(&g, &worst));
    g.check_err[3] = GALLERY_ACCEPT_MAX_PX + 1;
    TEST_ASSERT_EQUAL_INT(GALLERY_ACCEPT_FAIL, gallery_acceptance(&g, &worst));
    TEST_ASSERT_EQUAL_INT(GALLERY_ACCEPT_MAX_PX + 1, worst);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    for (i = 0; i < 3; i++) frame(); /* the verdict text draws without errors */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_the_calibrate_button_asks_the_platform_and_the_other_pages_do_not(void)
{
    start(3);
    settle();
    TEST_ASSERT_FALSE(g.calibrate_requested);
    tap(40, 64); /* the button, left of the row */
    TEST_ASSERT_TRUE(g.calibrate_requested);

    start(0);
    settle();
    tap(40, 64); /* on the first page that is another button */
    TEST_ASSERT_FALSE(g.calibrate_requested);
}

static void test_the_animation_moves_with_the_frames_and_the_pages_are_all_reachable(void)
{
    uint64_t h1, h2;
    int p;

    start(4);
    frame();
    h1 = cydui_mem_hash(&rig.mem);
    frame();
    h2 = cydui_mem_hash(&rig.mem);
    TEST_ASSERT_TRUE(h1 != h2);

    for (p = 0; p < GALLERY_PAGES; p++) {
        start(p);
        settle();
        TEST_ASSERT_EQUAL_INT(p, g.tab);
        TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_taps_on_the_targets_measure_zero_and_the_tap_after_the_fourth_starts_over);
    RUN_TEST(test_the_error_is_the_distance_in_pixels);
    RUN_TEST(test_the_guided_test_goes_from_targets_to_taps_to_a_verdict);
    RUN_TEST(test_the_calibrate_button_asks_the_platform_and_the_other_pages_do_not);
    RUN_TEST(test_the_animation_moves_with_the_frames_and_the_pages_are_all_reachable);
    return UNITY_END();
}
