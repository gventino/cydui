/*
 * Slider, progress and tabs (FR-31, FR-32). Positions are worked out by hand with the default theme
 * (12 px font, 32 px touch targets, margin and gaps of 8):
 *
 *   slider alone      (8,8) 160 x 32; the track is the whole widget, the knob (20 px) travels from
 *                     x = 18 to x = 158 (a run of 140 px), the track is 6 px thick at y = 21..26
 *   progress alone    (8,8) 160 x 8
 *   tabs alone        (8,8) 304 x 32; three tabs: x = 8, 109, 210 (the last one 102 wide)
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
}

static cydui_color px(int x, int y)
{
    return rig.fb[y * RIG_W + x];
}

/* ---- slider geometry (pure functions) ---- */

static void test_knob_geometry_is_monotonic_and_inverts(void)
{
    cydui_rect track = {8, 8, 160, 32};
    int x, prev_knob = -1;
    int32_t prev_val = -1;

    TEST_ASSERT_EQUAL_INT(18, cydui_slider_knob_x(track, 0, 0, 100));
    TEST_ASSERT_EQUAL_INT(158, cydui_slider_knob_x(track, 100, 0, 100));
    TEST_ASSERT_EQUAL_INT(88, cydui_slider_knob_x(track, 50, 0, 100));
    TEST_ASSERT_EQUAL_INT(18, cydui_slider_knob_x(track, -5, 0, 100));   /* clamped */
    TEST_ASSERT_EQUAL_INT(158, cydui_slider_knob_x(track, 500, 0, 100));

    for (x = 0; x < RIG_W; x++) {
        int32_t v = cydui_slider_value_at(track, x, 0, 100);
        TEST_ASSERT_TRUE(v >= 0 && v <= 100 && v >= prev_val);
        prev_val = v;
    }
    TEST_ASSERT_EQUAL_INT32(0, cydui_slider_value_at(track, -1000, 0, 100));
    TEST_ASSERT_EQUAL_INT32(100, cydui_slider_value_at(track, 1000, 0, 100));
    TEST_ASSERT_EQUAL_INT32(50, cydui_slider_value_at(track, 88, 0, 100));
    TEST_ASSERT_EQUAL_INT32(59, cydui_slider_value_at(track, 100, 0, 100)); /* 82 / 140 * 100 = 58.57 */

    for (x = 0; x <= 100; x++) { /* knob_x never goes backwards, and value_at(knob_x(v)) is v within a step */
        int k = cydui_slider_knob_x(track, x, 0, 100);
        int32_t back = cydui_slider_value_at(track, k, 0, 100);
        TEST_ASSERT_TRUE(k >= prev_knob);
        TEST_ASSERT_TRUE(back >= x - 1 && back <= x + 1);
        prev_knob = k;
    }
}

static void test_knob_geometry_with_degenerate_tracks_and_ranges(void)
{
    cydui_rect tiny = {10, 0, 15, 20}; /* narrower than a knob: no travel at all */
    cydui_rect neg = {10, 0, -50, 20};

    TEST_ASSERT_EQUAL_INT(20, cydui_slider_knob_x(tiny, 50, 0, 100));
    TEST_ASSERT_EQUAL_INT32(0, cydui_slider_value_at(tiny, 500, 0, 100));
    TEST_ASSERT_EQUAL_INT(20, cydui_slider_knob_x(neg, 50, 0, 100));
    TEST_ASSERT_EQUAL_INT32(7, cydui_slider_value_at(neg, 0, 7, 100));

    TEST_ASSERT_EQUAL_INT(18, cydui_slider_knob_x((cydui_rect){8, 0, 160, 32}, 3, 5, 5)); /* min == max */
    TEST_ASSERT_EQUAL_INT32(9, cydui_slider_value_at((cydui_rect){8, 0, 160, 32}, 100, 9, 9));
    TEST_ASSERT_EQUAL_INT32(9, cydui_slider_value_at((cydui_rect){8, 0, 160, 32}, 100, 9, 2)); /* min > max */
}

static void test_knob_geometry_covers_the_whole_int32_range(void)
{
    cydui_rect track = {8, 8, 160, 32};

    TEST_ASSERT_EQUAL_INT(18, cydui_slider_knob_x(track, INT32_MIN, INT32_MIN, INT32_MAX));
    TEST_ASSERT_EQUAL_INT(158, cydui_slider_knob_x(track, INT32_MAX, INT32_MIN, INT32_MAX));
    TEST_ASSERT_EQUAL_INT(87, cydui_slider_knob_x(track, -1, INT32_MIN, INT32_MAX)); /* 69.99997 px, floored */
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, cydui_slider_value_at(track, 18, INT32_MIN, INT32_MAX));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, cydui_slider_value_at(track, 158, INT32_MIN, INT32_MAX));
    TEST_ASSERT_EQUAL_INT32(0, cydui_slider_value_at(track, 88, INT32_MIN, INT32_MAX)); /* the exact middle, rounded: worked out independently */
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, cydui_slider_value_at(track, 0, INT32_MIN, INT32_MAX));
}

/* ---- the slider widget ---- */

static int32_t value;

static bool slider_frame(int32_t min, int32_t max)
{
    bool changed;
    cydui_ui_begin(&rig.ui, 0);
    changed = cydui_slider(&rig.ui, "##v", &value, min, max);
    cydui_ui_end(&rig.ui);
    return changed;
}

static void test_a_touch_sets_the_value_where_it_lands(void)
{
    static const int xs[] = {18, 88, 158, 4, 171}; /* the touch area reaches 4 px past each end of the track */
    static const int32_t want[] = {0, 50, 100, 0, 100};
    size_t i;

    for (i = 0; i < sizeof(xs) / sizeof(xs[0]); i++) {
        start();
        value = 25;
        rig_down(&rig, xs[i], 24);
        TEST_ASSERT_TRUE_MESSAGE(slider_frame(0, 100) == (want[i] != 25), "wrong change report");
        TEST_ASSERT_EQUAL_INT32(want[i], value);
    }
}

static void test_the_value_follows_a_drag_also_outside_the_track(void)
{
    start();
    value = 40;
    rig_down(&rig, 18, 24);
    TEST_ASSERT_TRUE(slider_frame(0, 100));
    TEST_ASSERT_EQUAL_INT32(0, value);

    rig_move(&rig, 100, 24);
    TEST_ASSERT_TRUE(slider_frame(0, 100));
    TEST_ASSERT_EQUAL_INT32(59, value);

    TEST_ASSERT_FALSE_MESSAGE(slider_frame(0, 100), "reported a change when nothing moved");
    TEST_ASSERT_EQUAL_INT32(59, value);

    rig_move(&rig, 300, 200); /* far below and to the right of the track: still ours */
    TEST_ASSERT_TRUE(slider_frame(0, 100));
    TEST_ASSERT_EQUAL_INT32(100, value);

    rig_move(&rig, 0, 24); /* off the left edge */
    TEST_ASSERT_TRUE(slider_frame(0, 100));
    TEST_ASSERT_EQUAL_INT32(0, value);

    rig_move(&rig, 60, 5);
    slider_frame(0, 100);
    rig_up(&rig, 60, 5); /* let go above the track: the value stays where the finger was */
    slider_frame(0, 100);
    TEST_ASSERT_EQUAL_INT32(30, value); /* (42 * 100 * 2 + 140) / 280 */
    TEST_ASSERT_FALSE(slider_frame(0, 100));
    TEST_ASSERT_EQUAL_INT32(30, value);
}

static void test_a_touch_just_outside_the_touch_area_does_nothing(void)
{
    start();
    value = 25;
    rig_down(&rig, 3, 24); /* the area starts at x = 4 */
    TEST_ASSERT_FALSE(slider_frame(0, 100));
    start();
    value = 25;
    rig_down(&rig, 172, 24); /* and ends at x = 171 */
    TEST_ASSERT_FALSE(slider_frame(0, 100));
    TEST_ASSERT_EQUAL_INT32(25, value);
}

static void test_pressing_where_the_value_already_is_reports_nothing(void)
{
    start();
    value = 0;
    rig_down(&rig, 18, 24);
    TEST_ASSERT_FALSE(slider_frame(0, 100));
    TEST_ASSERT_EQUAL_INT32(0, value);
}

static void test_a_value_out_of_range_is_clamped_quietly(void)
{
    start();
    value = 500;
    TEST_ASSERT_FALSE(slider_frame(0, 100));
    TEST_ASSERT_EQUAL_INT32(100, value);
    value = -500;
    TEST_ASSERT_FALSE(slider_frame(0, 100));
    TEST_ASSERT_EQUAL_INT32(0, value);
    value = 5;
    slider_frame(0, 100);
    TEST_ASSERT_EQUAL_INT32(5, value); /* in range: untouched */
}

static void test_an_empty_or_reversed_range_does_nothing(void)
{
    start();
    value = 42;
    rig_down(&rig, 88, 24);
    TEST_ASSERT_FALSE(slider_frame(5, 5));
    TEST_ASSERT_EQUAL_INT32(42, value);
    rig_up(&rig, 88, 24);
    slider_frame(5, 5);
    rig_down(&rig, 88, 24);
    TEST_ASSERT_FALSE(slider_frame(10, 0));
    TEST_ASSERT_EQUAL_INT32(42, value);
    rig_up(&rig, 88, 24);
    slider_frame(10, 0);
    TEST_ASSERT_FALSE(slider_frame(INT32_MAX, INT32_MIN));
}

static void test_the_whole_int32_range_works_without_overflow(void)
{
    start();
    value = 0;
    rig_down(&rig, 18, 24);
    TEST_ASSERT_TRUE(slider_frame(INT32_MIN, INT32_MAX));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, value);
    rig_move(&rig, 158, 24);
    slider_frame(INT32_MIN, INT32_MAX);
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, value);
    rig_move(&rig, 88, 24);
    slider_frame(INT32_MIN, INT32_MAX);
    TEST_ASSERT_EQUAL_INT32(0, value);
}

static void test_a_disabled_slider_does_not_change_and_looks_it(void)
{
    uint64_t on, off;

    start();
    value = 25;
    slider_frame(0, 100);
    on = cydui_mem_hash(&rig.mem);
    rig_down(&rig, 158, 24);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    TEST_ASSERT_FALSE(cydui_slider(&rig.ui, "##v", &value, 0, 100));
    cydui_ui_end(&rig.ui);
    off = cydui_mem_hash(&rig.mem);
    TEST_ASSERT_EQUAL_INT32(25, value);
    TEST_ASSERT_TRUE(on != off);
}

static void test_a_slider_keeps_the_press_and_a_button_does_not(void)
{
    start();
    value = 25;
    rig_down(&rig, 30, 24);
    slider_frame(0, 100);
    TEST_ASSERT_TRUE(rig.ui.active_capture);

    start();
    rig_down(&rig, 30, 24);
    cydui_ui_begin(&rig.ui, 0);
    cydui_button(&rig.ui, "OK");
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE(rig.ui.active_capture);
}

static void test_the_label_takes_room_and_the_track_follows(void)
{
    cydui_text_extent e;
    int x0;

    start();
    e = cydui_text_size(rig.theme.style.font, "Vol", 3, 0);
    x0 = 8 + e.w + 8 + 10; /* label, gap, half a knob */
    value = 50;
    rig_down(&rig, x0, 24);
    cydui_ui_begin(&rig.ui, 0);
    cydui_slider(&rig.ui, "Vol", &value, 0, 100);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT32(0, value); /* the first knob position moved with the label */
}

static void test_a_slider_can_be_given_a_width(void)
{
    start();
    value = 0;
    rig_down(&rig, 98, 24); /* a 100 px slider at x = 8 ends its run at 8 + 100 - 10 */
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_width(&rig.ui, 100);
    cydui_slider(&rig.ui, "##v", &value, 0, 100);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT32(100, value);
}

static void test_the_knob_is_drawn_where_the_value_says(void)
{
    const cydui_wstyle *w;

    start();
    w = &rig.theme.table[CYDUI_WIDGET_SLIDER][0][CYDUI_STATE_NORMAL];
    value = 0;
    slider_frame(0, 100);
    TEST_ASSERT_EQUAL_HEX16(w->text, px(18, 24));   /* the knob at the left end */
    TEST_ASSERT_EQUAL_HEX16(w->fill, px(60, 24));   /* bare track beyond it */
    value = 100;
    slider_frame(0, 100);
    TEST_ASSERT_EQUAL_HEX16(w->text, px(158, 24));  /* the knob at the right end */
    TEST_ASSERT_EQUAL_HEX16(w->accent, px(60, 24)); /* the done part is the accent */
    value = 50;
    slider_frame(0, 100);
    TEST_ASSERT_EQUAL_HEX16(w->accent, px(50, 24)); /* left of the knob (88) */
    TEST_ASSERT_EQUAL_HEX16(w->fill, px(130, 24));  /* right of it */
}

static void test_two_sliders_with_one_label_collide_like_any_widget(void)
{
    int32_t a = 0, b = 0;
    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_slider(&rig.ui, "##v", &a, 0, 10);
    cydui_slider(&rig.ui, "##v", &b, 0, 10);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_ID_COLLISION);
}

static void test_a_null_value_is_tolerated(void)
{
    start();
    rig_down(&rig, 88, 24);
    cydui_ui_begin(&rig.ui, 0);
    cydui_slider(&rig.ui, "##v", NULL, 0, 100);
    cydui_slider(&rig.ui, NULL, NULL, 0, 100);
    cydui_ui_end(&rig.ui);
}

/* ---- progress ---- */

static void progress_frame(int32_t v, int32_t max)
{
    cydui_ui_begin(&rig.ui, 0);
    cydui_progress(&rig.ui, v, max);
    cydui_ui_end(&rig.ui);
}

static void test_progress_fills_the_proportion_of_the_bar(void)
{
    const cydui_wstyle *w;

    start();
    w = &rig.theme.table[CYDUI_WIDGET_PROGRESS][0][CYDUI_STATE_NORMAL];
    progress_frame(50, 100); /* 50 * 160 / 100 = 80 px of the bar */
    TEST_ASSERT_EQUAL_HEX16(w->accent, px(8 + 79, 12));
    TEST_ASSERT_EQUAL_HEX16(w->fill, px(8 + 82, 12));
    progress_frame(100, 100);
    TEST_ASSERT_EQUAL_HEX16(w->accent, px(8 + 150, 12));
    progress_frame(25, 100); /* 40 px */
    TEST_ASSERT_EQUAL_HEX16(w->accent, px(8 + 39, 12));
    TEST_ASSERT_EQUAL_HEX16(w->fill, px(8 + 45, 12));
}

static int accent_pixels_in_bar(void)
{
    int x, y, n = 0;
    for (y = 8; y < 16; y++) {
        for (x = 8; x < 168; x++) {
            n += px(x, y) == rig.theme.table[CYDUI_WIDGET_PROGRESS][0][CYDUI_STATE_NORMAL].accent;
        }
    }
    return n;
}

static void test_progress_is_clamped_at_both_ends_and_survives_odd_numbers(void)
{
    start();
    progress_frame(0, 100);
    TEST_ASSERT_EQUAL_INT(0, accent_pixels_in_bar());
    progress_frame(-50, 100);
    TEST_ASSERT_EQUAL_INT(0, accent_pixels_in_bar());
    progress_frame(INT32_MIN, 100);
    TEST_ASSERT_EQUAL_INT(0, accent_pixels_in_bar());
    progress_frame(50, 0); /* nothing to measure against */
    TEST_ASSERT_EQUAL_INT(0, accent_pixels_in_bar());
    progress_frame(50, -10);
    TEST_ASSERT_EQUAL_INT(0, accent_pixels_in_bar());

    progress_frame(100, 100);
    {
        int full = accent_pixels_in_bar();
        TEST_ASSERT_TRUE(full > 1000);
        progress_frame(9999, 100); /* above max: drawn as max */
        TEST_ASSERT_EQUAL_INT(full, accent_pixels_in_bar());
        progress_frame(INT32_MAX, INT32_MAX);
        TEST_ASSERT_EQUAL_INT(full, accent_pixels_in_bar());
        progress_frame(INT32_MAX, 100);
        TEST_ASSERT_EQUAL_INT(full, accent_pixels_in_bar());
    }
    progress_frame(1, INT32_MAX); /* a speck: no overflow, no pixel */
    TEST_ASSERT_EQUAL_INT(0, accent_pixels_in_bar());
}

static void test_progress_is_a_pill_and_does_not_react(void)
{
    start();
    progress_frame(100, 100);
    TEST_ASSERT_EQUAL_HEX16(0, px(8, 8)); /* the corner of an 8 px tall pill is background */
    rig_down(&rig, 50, 12);
    rig_up(&rig, 50, 12);
    progress_frame(40, 100);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    cydui_ui_begin(&rig.ui, 0);
    cydui_progress(&rig.ui, 5, 10);
    cydui_progress(&rig.ui, 5, 10); /* no id: the same call twice is fine */
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

/* ---- tabs ---- */

static const char *const labels3[3] = {"Um", "Dois", "Tres"};
static int sel;

static bool tabs_frame(const char *const *labels, int count)
{
    bool changed;
    cydui_ui_begin(&rig.ui, 0);
    changed = cydui_tabs(&rig.ui, labels, count, &sel);
    cydui_ui_end(&rig.ui);
    return changed;
}

static void tap(int x, int y)
{
    rig_down(&rig, x, y);
    rig_up(&rig, x, y);
}

static void test_a_tap_chooses_a_tab_once(void)
{
    start();
    sel = 0;
    tap(120, 24); /* the second tab spans x = 109..209 */
    TEST_ASSERT_TRUE(tabs_frame(labels3, 3));
    TEST_ASSERT_EQUAL_INT(1, sel);
    TEST_ASSERT_FALSE(tabs_frame(labels3, 3));
    tap(120, 24); /* the one that is already chosen */
    TEST_ASSERT_FALSE(tabs_frame(labels3, 3));
    TEST_ASSERT_EQUAL_INT(1, sel);
    tap(250, 24);
    TEST_ASSERT_TRUE(tabs_frame(labels3, 3));
    TEST_ASSERT_EQUAL_INT(2, sel);
    tap(12, 24);
    TEST_ASSERT_TRUE(tabs_frame(labels3, 3));
    TEST_ASSERT_EQUAL_INT(0, sel);
}

static void test_the_tab_edges_are_where_the_division_puts_them(void)
{
    start();
    sel = 0;
    tap(108, 24); /* the last pixel of the first tab (x = 8..108) */
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(0, sel);
    tap(109, 24); /* the first of the second */
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(1, sel);
    tap(209, 24); /* the last of the second */
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(1, sel);
    tap(210, 24);
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(2, sel);
    tap(12, 24);
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(0, sel);
    tap(311, 24); /* the last pixel of the bar: the last tab takes the remainder, x = 210..311 */
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(2, sel);
}

static void test_the_chosen_tab_is_drawn_chosen_in_the_same_frame(void)
{
    start();
    sel = 0;
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_TAB][1][CYDUI_STATE_NORMAL].fill, px(8 + 14, 8 + 8));
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_TAB][0][CYDUI_STATE_NORMAL].fill, px(109 + 14, 8 + 8));
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_TAB][1][CYDUI_STATE_NORMAL].accent, px(30, 8 + 32 - 2));
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_TAB][0][CYDUI_STATE_NORMAL].fill, px(130, 8 + 32 - 2));

    tap(120, 24); /* the frame that takes the tap already has the new choice (and the tab still looks pressed) */
    TEST_ASSERT_TRUE(tabs_frame(labels3, 3));
    TEST_ASSERT_EQUAL_INT(1, sel);
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_TAB][1][CYDUI_STATE_PRESSED].fill, px(109 + 14, 8 + 8));
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_TAB][1][CYDUI_STATE_NORMAL].fill, px(109 + 14, 8 + 8));
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_TAB][0][CYDUI_STATE_NORMAL].fill, px(8 + 14, 8 + 8));
}

static void test_the_selection_is_clamped_into_range(void)
{
    start();
    sel = -5;
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(0, sel);
    sel = 99;
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(2, sel);
    sel = INT_MIN;
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(0, sel);
    sel = INT_MAX;
    tabs_frame(labels3, 3);
    TEST_ASSERT_EQUAL_INT(2, sel);
}

static void test_odd_counts_and_labels_are_safe(void)
{
    const char *none[2] = {NULL, NULL};
    cydui_rect after;
    const char **exact = (const char **)malloc(CYDUI_MAX_TABS * sizeof(*exact)); /* exactly 8 entries */
    int i;

    TEST_ASSERT_NOT_NULL(exact);
    for (i = 0; i < CYDUI_MAX_TABS; i++) {
        exact[i] = "t";
    }
    start();
    sel = 0;
    TEST_ASSERT_FALSE(tabs_frame(labels3, 0));
    TEST_ASSERT_FALSE(tabs_frame(labels3, -4));
    TEST_ASSERT_FALSE(tabs_frame(NULL, 3));
    cydui_ui_begin(&rig.ui, 0);
    cydui_tabs(&rig.ui, labels3, 0, &sel);
    after = cydui_layout_avail(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT_MESSAGE(8, after.y, "an empty tab bar took room");

    tabs_frame(none, 2); /* NULL labels are empty labels */
    tabs_frame(labels3, 1); /* one tab */
    TEST_ASSERT_EQUAL_INT(0, sel);

    tabs_frame((const char *const *)exact, 100); /* more than the bar can show: only 8 are read */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    tap(8 + 304 - 3, 24); /* the last of eight tabs, each 38 px wide */
    tabs_frame((const char *const *)exact, 100);
    TEST_ASSERT_EQUAL_INT(7, sel);
    free(exact);
}

static void test_equal_labels_and_no_selection_pointer(void)
{
    static const char *const same[3] = {"A", "A", "A"};

    start();
    sel = 0;
    tap(250, 24);
    TEST_ASSERT_TRUE(tabs_frame(same, 3));
    TEST_ASSERT_EQUAL_INT(2, sel);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui) & CYDUI_ERR_ID_COLLISION);

    tap(250, 24);
    cydui_ui_begin(&rig.ui, 0);
    cydui_tabs(&rig.ui, same, 3, NULL); /* nothing to write the choice to: no crash */
    cydui_ui_end(&rig.ui);
}

static void test_disabled_tabs_do_not_change(void)
{
    start();
    sel = 0;
    tap(250, 24);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    TEST_ASSERT_FALSE(cydui_tabs(&rig.ui, labels3, 3, &sel));
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(0, sel);
}

static void test_tabs_fill_the_room_of_their_container(void)
{
    start();
    sel = 0;
    tap(50, 24); /* inside a 100 px column the second of two tabs starts at x = 8 + 50 */
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_width(&rig.ui, 100);
    cydui_begin_column(&rig.ui);
    cydui_tabs(&rig.ui, labels3, 2, &sel);
    cydui_end_column(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(0, sel); /* x = 50 is still in the first tab (8..57) */
    tap(60, 24);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_width(&rig.ui, 100);
    cydui_begin_column(&rig.ui);
    cydui_tabs(&rig.ui, labels3, 2, &sel);
    cydui_end_column(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(1, sel);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_knob_geometry_is_monotonic_and_inverts);
    RUN_TEST(test_knob_geometry_with_degenerate_tracks_and_ranges);
    RUN_TEST(test_knob_geometry_covers_the_whole_int32_range);
    RUN_TEST(test_a_touch_sets_the_value_where_it_lands);
    RUN_TEST(test_the_value_follows_a_drag_also_outside_the_track);
    RUN_TEST(test_a_touch_just_outside_the_touch_area_does_nothing);
    RUN_TEST(test_pressing_where_the_value_already_is_reports_nothing);
    RUN_TEST(test_a_value_out_of_range_is_clamped_quietly);
    RUN_TEST(test_an_empty_or_reversed_range_does_nothing);
    RUN_TEST(test_the_whole_int32_range_works_without_overflow);
    RUN_TEST(test_a_disabled_slider_does_not_change_and_looks_it);
    RUN_TEST(test_a_slider_keeps_the_press_and_a_button_does_not);
    RUN_TEST(test_the_label_takes_room_and_the_track_follows);
    RUN_TEST(test_a_slider_can_be_given_a_width);
    RUN_TEST(test_the_knob_is_drawn_where_the_value_says);
    RUN_TEST(test_two_sliders_with_one_label_collide_like_any_widget);
    RUN_TEST(test_a_null_value_is_tolerated);
    RUN_TEST(test_progress_fills_the_proportion_of_the_bar);
    RUN_TEST(test_progress_is_clamped_at_both_ends_and_survives_odd_numbers);
    RUN_TEST(test_progress_is_a_pill_and_does_not_react);
    RUN_TEST(test_a_tap_chooses_a_tab_once);
    RUN_TEST(test_the_tab_edges_are_where_the_division_puts_them);
    RUN_TEST(test_the_chosen_tab_is_drawn_chosen_in_the_same_frame);
    RUN_TEST(test_the_selection_is_clamped_into_range);
    RUN_TEST(test_odd_counts_and_labels_are_safe);
    RUN_TEST(test_equal_labels_and_no_selection_pointer);
    RUN_TEST(test_disabled_tabs_do_not_change);
    RUN_TEST(test_tabs_fill_the_room_of_their_container);
    return UNITY_END();
}
