/*
 * Scroll areas, coasting and virtual lists (FR-25, FR-33). The default theme, a 12 px font, margin 8:
 * the area is at (8,8), 304 px wide, 100 px tall unless a test says otherwise, so it covers
 * x = 8..311 and y = 8..107. The offsets are kept in 1/256 px. The numbers of the coasting test were
 * worked out by a separate script that follows the rules written in src/scroll.c (speed = mean of the
 * recent frames; every coasting frame moves v * dt and takes max(1, v * dt / 325) off the speed).
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;
static int content_h = 400;
static int view_h = 100;
static bool coasting; /* what the last cydui_ui_end said */

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    content_h = 400;
    view_h = 100;
}

static int32_t offset(void)
{
    cydui_state *st = cydui_state_get(&rig.ui, cydui_id_str(&rig.ui, "s"), false);
    TEST_ASSERT_NOT_NULL(st);
    return st->a;
}

static int32_t speed(void)
{
    cydui_state *st = cydui_state_get(&rig.ui, cydui_id_str(&rig.ui, "s"), false);
    TEST_ASSERT_NOT_NULL(st);
    return st->b;
}

static cydui_color px(int x, int y)
{
    return rig.fb[y * RIG_W + x];
}

/* A frame: one area with a tall empty content. 16 ms pass before it. */
static void frame(void)
{
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "s", view_h) {
        cydui_spacer(&rig.ui, content_h);
    }
    coasting = cydui_ui_end(&rig.ui);
}

static void warm_up(void)
{
    frame();
    frame(); /* the first frame measures the content, the second one knows its height */
}

/* ---- dragging ---- */

static void test_dragging_moves_the_content_by_the_travel_beyond_the_slop(void)
{
    start();
    warm_up();
    TEST_ASSERT_EQUAL_INT32(0, offset());
    rig_down(&rig, 100, 60);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, offset()); /* a press alone does not move anything */
    rig_move(&rig, 100, 55);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, offset()); /* 5 px: still a tap */
    rig_move(&rig, 100, 40); /* 20 px from the press: the area takes the drag; the 15 px of this frame count */
    frame();
    TEST_ASSERT_EQUAL_INT32(15 * 256, offset());
    rig_move(&rig, 100, 30);
    frame();
    TEST_ASSERT_EQUAL_INT32(25 * 256, offset());
    rig_move(&rig, 100, 31); /* back down: the content follows the finger down */
    frame();
    TEST_ASSERT_EQUAL_INT32(24 * 256, offset());
}

static void test_the_offset_stops_at_both_ends(void)
{
    start();
    warm_up();
    rig_down(&rig, 100, 100);
    frame();
    rig_move(&rig, 100, 20); /* 80 px up */
    frame();
    rig_move(&rig, 100, 8);
    frame();
    TEST_ASSERT_EQUAL_INT32(92 * 256, offset());
    rig_move(&rig, 100, 107);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, offset()); /* 99 px down from 92: not below the top */
    rig_move(&rig, 100, 8);
    frame();
    rig_move(&rig, 100, 107);
    frame();
    rig_move(&rig, 100, 8);
    frame();
    rig_move(&rig, 100, 107);
    frame();
    rig_move(&rig, 100, 8);
    frame();
    rig_move(&rig, 100, 107);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, offset());
    /* the bottom: 400 - 100 = 300 px at most */
    {
        int i;
        for (i = 0; i < 6; i++) {
            rig_move(&rig, 100, i % 2 ? 107 : 8);
            frame();
            rig_move(&rig, 100, i % 2 ? 8 : 107);
            frame();
        }
    }
    TEST_ASSERT_TRUE(offset() <= 300 * 256 && offset() >= 0);
}

static void test_the_bottom_is_content_minus_view(void)
{
    int i;

    start();
    warm_up();
    for (i = 0; i < 8; i++) { /* a long drag upward, one swipe after the other */
        rig_down(&rig, 100, 107);
        frame();
        rig_move(&rig, 100, 60);
        frame();
        rig_move(&rig, 100, 8);
        frame();
        rig_up(&rig, 100, 8);
        frame();
        frame();
    }
    {
        int k;
        for (k = 0; k < 120; k++) frame(); /* let any coasting finish */
    }
    TEST_ASSERT_EQUAL_INT32(300 * 256, offset());
    TEST_ASSERT_FALSE(coasting);
}

static void test_content_shorter_than_the_view_does_not_scroll_or_show_a_bar(void)
{
    start();
    content_h = 60;
    warm_up();
    rig_down(&rig, 100, 100);
    frame();
    rig_move(&rig, 100, 40);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, offset());
    TEST_ASSERT_EQUAL_HEX16(0, px(308, 20));
    TEST_ASSERT_EQUAL_HEX16(0, px(308, 12));
}

static void test_the_scroll_bar_follows_the_offset(void)
{
    start();
    warm_up();
    /* track y = 10..105 (96 px), thumb = 96 * 100 / 400 = 24 px, at the top: y = 10..33 */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_SCROLLBAR][0][CYDUI_STATE_NORMAL].accent, px(308, 22));
    TEST_ASSERT_EQUAL_HEX16(0, px(308, 60));
    TEST_ASSERT_EQUAL_HEX16(0, px(300, 22)); /* nothing of it left of the 4 px bar */
    cydui_scroll_to(&rig.ui, "s", 300);
    frame();
    /* at the bottom the thumb is at y = 10 + 72 = 82..105 */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_SCROLLBAR][0][CYDUI_STATE_NORMAL].accent, px(308, 94));
    TEST_ASSERT_EQUAL_HEX16(0, px(308, 22));
}

/* ---- who gets the touch ---- */

static cydui_response last_resp;

static void frame_with_a_raw_widget(void)
{
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "s", view_h) {
        cydui_rect r = cydui_alloc_rect(&rig.ui, 100, 30); /* x 8..107, y 8..37 at the top */
        cydui_id id = cydui_id_str(&rig.ui, "raw");
        cydui_id_claim(&rig.ui, id);
        last_resp = cydui_interact(&rig.ui, id, r, CYDUI_SENSE_CLICK | CYDUI_SENSE_DRAG);
        cydui_spacer(&rig.ui, content_h);
    }
    coasting = cydui_ui_end(&rig.ui);
}

static void test_a_tap_on_a_child_still_clicks(void)
{
    start();
    frame_with_a_raw_widget();
    frame_with_a_raw_widget();
    rig_down(&rig, 20, 20);
    frame_with_a_raw_widget();
    TEST_ASSERT_TRUE(last_resp.pressed);
    rig_move(&rig, 22, 22); /* a wobble within the slop */
    frame_with_a_raw_widget();
    TEST_ASSERT_TRUE(last_resp.pressed);
    rig_up(&rig, 22, 22);
    frame_with_a_raw_widget();
    TEST_ASSERT_TRUE(last_resp.clicked);
    TEST_ASSERT_EQUAL_INT32(0, offset());
}

static void test_a_drag_that_began_on_a_child_is_taken_over_before_the_child_sees_it(void)
{
    start();
    frame_with_a_raw_widget();
    frame_with_a_raw_widget();
    rig_down(&rig, 20, 20);
    frame_with_a_raw_widget();
    TEST_ASSERT_TRUE(last_resp.pressed);
    rig_move(&rig, 20, 60); /* 40 px away */
    frame_with_a_raw_widget();
    TEST_ASSERT_FALSE_MESSAGE(last_resp.pressed, "the child still showed the press in the frame it was taken");
    TEST_ASSERT_FALSE(last_resp.dragging);
    TEST_ASSERT_EQUAL_INT32(0, offset()); /* dragged down at the top: nowhere to go */
    rig_move(&rig, 20, 20);
    frame_with_a_raw_widget();
    TEST_ASSERT_EQUAL_INT32(40 * 256, offset());
    rig_up(&rig, 20, 20);
    frame_with_a_raw_widget();
    TEST_ASSERT_FALSE_MESSAGE(last_resp.clicked, "a drag clicked the child");
    TEST_ASSERT_FALSE(last_resp.released);
}

static void test_a_slider_inside_keeps_the_touch(void)
{
    int32_t v = 10;

    start();
    warm_up();
    rig_down(&rig, 60, 24); /* on the slider: x 8..167, y 8..39 */
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "s", view_h) {
        cydui_slider(&rig.ui, "##v", &v, 0, 100);
        cydui_spacer(&rig.ui, content_h);
    }
    cydui_ui_end(&rig.ui);
    rig_move(&rig, 100, 2); /* far above, well beyond the slop: an area that took it would scroll */
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "s", view_h) {
        cydui_slider(&rig.ui, "##v", &v, 0, 100);
        cydui_spacer(&rig.ui, content_h);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT32(0, offset());
    TEST_ASSERT_TRUE(rig.ui.active_capture);
    TEST_ASSERT_TRUE(cydui_id_eq(rig.ui.active, cydui_id_str(&rig.ui, "##v")));
}

static void test_a_drag_that_began_outside_the_area_does_not_scroll_it(void)
{
    start();
    warm_up();
    rig_down(&rig, 20, 4); /* above the area (it starts at y = 8), on nothing */
    frame();
    rig_move(&rig, 20, 100);
    frame();
    rig_move(&rig, 20, 8);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, offset());
}

/* ---- coasting ---- */

static const int32_t coast_offsets[] = {19836, 24091, 28133, 31975, 35624, 39093, 42390, 45523, 48500, 51330, 54016, 56567, 58991, 61292, 63479, 65555, 67529, 69404, 71185, 72876, 74481, 76004, 77449, 78821, 80123, 81359, 82534, 83648, 84704, 85707, 86657, 87558, 88414, 89225, 89995, 90724, 91416, 92071, 92693, 93282, 93839, 94367, 94866, 95337, 95783, 96204, 96601, 96977, 97333, 97668, 97983, 98282, 98564, 98830, 99079, 99312, 99533, 99741, 99937, 100121, 100293, 100452, 100603, 100746, 100881, 101007, 101125, 101235, 101337, 101431, 101517, 101594, 101667, 101736, 101801, 101862, 101919, 101972, 102021, 102066, 102106};
#define COAST_FRAMES ((int)(sizeof(coast_offsets) / sizeof(coast_offsets[0])))

static void flick(uint32_t t0)
{
    rig.mem.now = t0;
    content_h = 1000;
    warm_up();
    rig_down(&rig, 100, 90);
    frame();
    rig_move(&rig, 100, 70);
    frame();
    rig_move(&rig, 100, 50);
    frame();
    rig_move(&rig, 100, 30);
    frame();
    TEST_ASSERT_EQUAL_INT32(60 * 256, offset());
    TEST_ASSERT_EQUAL_INT32(1093, speed());
    rig_up(&rig, 100, 30);
    frame();
    TEST_ASSERT_EQUAL_INT32(60 * 256, offset()); /* the release frame has no travel and does not halve the speed */
    TEST_ASSERT_EQUAL_INT32(1093, speed());
    TEST_ASSERT_TRUE(coasting);
}

static void test_a_flick_coasts_along_the_computed_path_and_stops(void)
{
    int i;

    start();
    flick(0);
    for (i = 0; i < COAST_FRAMES; i++) {
        frame();
        TEST_ASSERT_EQUAL_INT32_MESSAGE(coast_offsets[i], offset(), "coasting left the computed path");
        TEST_ASSERT_TRUE_MESSAGE(coasting == (i < COAST_FRAMES - 1), "wrong 'keep drawing' answer");
    }
    TEST_ASSERT_EQUAL_INT32(0, speed());
    frame();
    TEST_ASSERT_EQUAL_INT32(coast_offsets[COAST_FRAMES - 1], offset());
    TEST_ASSERT_FALSE(coasting);
}

static void test_the_same_flick_gives_the_same_path_whenever_it_happens(void)
{
    static const uint32_t starts[] = {0u, 5000u, 0xFFFFFFF0u, 0xFFFFFF00u};
    size_t s;
    int i;

    for (s = 0; s < sizeof(starts) / sizeof(starts[0]); s++) { /* two of them make the clock wrap */
        start();
        flick(starts[s]);
        for (i = 0; i < COAST_FRAMES; i++) {
            frame();
        }
        TEST_ASSERT_EQUAL_INT32(coast_offsets[COAST_FRAMES - 1], offset());
    }
}

static void test_a_finger_that_rests_before_letting_go_does_not_fling(void)
{
    int i;

    start();
    content_h = 1000;
    warm_up();
    rig_down(&rig, 100, 90);
    frame();
    rig_move(&rig, 100, 70);
    frame();
    for (i = 0; i < 7; i++) {
        frame(); /* the finger rests: the speed halves every frame */
    }
    TEST_ASSERT_EQUAL_INT32(4, speed());
    rig_up(&rig, 100, 70);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, speed());
    TEST_ASSERT_EQUAL_INT32(20 * 256, offset());
    frame();
    TEST_ASSERT_EQUAL_INT32(20 * 256, offset());
    TEST_ASSERT_FALSE(coasting);
}

static void test_a_slow_drag_does_not_coast(void)
{
    int i;
    int32_t at_release;

    start();
    content_h = 1000;
    warm_up();
    rig_down(&rig, 100, 90);
    frame();
    for (i = 1; i <= 40; i++) { /* a pixel every 32 ms: about 30 px/s */
        rig_move(&rig, 100, 90 - i);
        frame();
        frame();
    }
    TEST_ASSERT_TRUE(offset() > 20 * 256);
    TEST_ASSERT_TRUE(speed() < CYDUI_SCROLL_FLING_MIN);
    rig_up(&rig, 100, 50);
    frame();
    at_release = offset();
    TEST_ASSERT_EQUAL_INT32(0, speed());
    frame();
    frame();
    TEST_ASSERT_EQUAL_INT32(at_release, offset());
    TEST_ASSERT_FALSE(coasting);
}

static void test_touching_a_coasting_area_stops_it(void)
{
    int32_t before;

    start();
    flick(0);
    frame();
    frame();
    before = offset();
    TEST_ASSERT_TRUE(speed() > 0);
    rig_down(&rig, 100, 60);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, speed());
    TEST_ASSERT_EQUAL_INT32(before, offset());
    frame();
    TEST_ASSERT_EQUAL_INT32(before, offset());
}

static void test_coasting_stops_dead_at_the_end(void)
{
    start();
    content_h = 150; /* only 50 px to go */
    warm_up();
    rig_down(&rig, 100, 90);
    frame();
    rig_move(&rig, 100, 70);
    frame();
    rig_move(&rig, 100, 50);
    frame();
    rig_up(&rig, 100, 50);
    frame();
    {
        int i;
        for (i = 0; i < 40; i++) frame();
    }
    TEST_ASSERT_EQUAL_INT32(50 * 256, offset());
    TEST_ASSERT_EQUAL_INT32(0, speed());
    TEST_ASSERT_FALSE(coasting);
}

static void test_without_a_clock_there_is_no_coasting(void)
{
    start();
    rig.mem.backend.now_ms = NULL;
    content_h = 1000;
    warm_up();
    rig_down(&rig, 100, 90);
    frame();
    rig_move(&rig, 100, 30);
    frame();
    TEST_ASSERT_EQUAL_INT32(60 * 256, offset()); /* the drag itself works */
    rig_up(&rig, 100, 30);
    frame();
    frame();
    TEST_ASSERT_EQUAL_INT32(60 * 256, offset());
    TEST_ASSERT_EQUAL_INT32(0, speed());
    TEST_ASSERT_FALSE(coasting);
}

static void test_a_frozen_clock_neither_coasts_nor_hangs(void)
{
    int i;

    start();
    flick(0);
    for (i = 0; i < 5; i++) { /* the clock does not move: the frames do nothing, and still ask for more */
        cydui_ui_begin(&rig.ui, 0);
        CYDUI_SCROLL(&rig.ui, "s", view_h) { cydui_spacer(&rig.ui, content_h); }
        coasting = cydui_ui_end(&rig.ui);
        TEST_ASSERT_EQUAL_INT32(60 * 256, offset());
    }
}

static void test_a_long_pause_between_frames_counts_as_one_hundred_ms(void)
{
    start();
    flick(0);
    rig_advance(&rig, 60000 - 16); /* frame() adds 16 more */
    frame();
    /* v = 1093, dt capped at 100: 1093 * 100 * 256 / 1000 = 27980 */
    TEST_ASSERT_EQUAL_INT32(60 * 256 + 27980, offset());
}


/* ---- scrolled content is the unscrolled content, moved ---- */

static cydui_color snapshot[RIG_W * RIG_H];

static void rich_frame(int32_t scroll_y, bool jump)
{
    static bool flags[4];
    static int32_t sv = 40;

    rig_advance(&rig, 16);
    if (jump) {
        cydui_scroll_to(&rig.ui, "s", scroll_y);
    }
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "s", 100) {
        cydui_label(&rig.ui, "Title");
        CYDUI_ROW(&rig.ui) {
            cydui_button(&rig.ui, "One");
            cydui_button(&rig.ui, "Two");
        }
        CYDUI_CARD(&rig.ui) {
            cydui_label(&rig.ui, "Inside the card");
            cydui_checkbox(&rig.ui, "Check", &flags[0]);
        }
        cydui_toggle(&rig.ui, "Turn on", &flags[1]);
        cydui_next_width(&rig.ui, 120);
        cydui_slider(&rig.ui, "Vol", &sv, 0, 100);
        cydui_progress(&rig.ui, 50, 100);
        cydui_button(&rig.ui, "End");
    }
    cydui_ui_end(&rig.ui);
}

static void test_scrolled_content_is_the_unscrolled_content_moved_up(void)
{
    static const int offsets[] = {17, 60, 101};
    size_t k;

    for (k = 0; k < sizeof(offsets) / sizeof(offsets[0]); k++) {
        int x, y, shift = offsets[k];

        start();
        rich_frame(0, false);
        rich_frame(0, false);
        rich_frame(0, false);
        memcpy(snapshot, rig.fb, sizeof(snapshot));
        rich_frame(shift, true);
        rich_frame(shift, false);
        TEST_ASSERT_EQUAL_INT32(shift * 256, offset());
        for (y = 8; y < 8 + 100 - shift; y++) {
            for (x = 8; x < 300; x++) { /* left of the scroll bar */
                if (rig.fb[y * RIG_W + x] != snapshot[(y + shift) * RIG_W + x]) {
                    char msg[80];
                    snprintf(msg, sizeof(msg), "offset %d: pixel (%d,%d) differs", shift, x, y);
                    TEST_FAIL_MESSAGE(msg);
                }
            }
        }
    }
}


static void test_an_area_with_no_content_cannot_be_dragged_past_the_offset_cap(void)
{
    cydui_state *st;

    start();
    content_h = 0; /* nothing to measure: the area has no end to stop at, only the cap */
    warm_up();
    st = cydui_state_get(&rig.ui, cydui_id_str(&rig.ui, "s"), false);
    TEST_ASSERT_NOT_NULL(st);
    st->a = (1 << 30) - 256; /* a pixel short of the cap */
    rig_down(&rig, 100, 90);
    frame();
    rig_move(&rig, 100, 40); /* 50 px up: 12800 more */
    frame();
    TEST_ASSERT_EQUAL_INT32(1 << 30, offset());
    TEST_ASSERT_EQUAL_INT32(0, speed());
    rig_move(&rig, 100, 10);
    frame();
    TEST_ASSERT_EQUAL_INT32(1 << 30, offset()); /* and no further */
}

/* ---- scroll_to ---- */

static void test_scroll_to_jumps_and_clamps(void)
{
    start();
    warm_up();
    cydui_scroll_to(&rig.ui, "s", 120);
    frame();
    TEST_ASSERT_EQUAL_INT32(120 * 256, offset());
    cydui_scroll_to(&rig.ui, "s", 99999);
    frame();
    TEST_ASSERT_EQUAL_INT32(300 * 256, offset());
    cydui_scroll_to(&rig.ui, "s", -50);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, offset());
    cydui_scroll_to(&rig.ui, "s", INT_MAX);
    frame();
    TEST_ASSERT_EQUAL_INT32(300 * 256, offset());
    cydui_scroll_to(&rig.ui, "s", INT_MIN);
    frame();
    TEST_ASSERT_EQUAL_INT32(0, offset());
    cydui_scroll_to(&rig.ui, NULL, 5); /* no label: no crash */
}

static void test_scroll_to_stops_a_coast(void)
{
    start();
    flick(0);
    frame();
    cydui_scroll_to(&rig.ui, "s", 10);
    frame();
    TEST_ASSERT_EQUAL_INT32(10 * 256, offset());
    TEST_ASSERT_EQUAL_INT32(0, speed());
}

/* ---- odd use ---- */

static void test_a_scroll_area_inside_another_is_a_plain_column(void)
{
    start();
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    cydui_scroll_begin(&rig.ui, "outer", 150);
    cydui_scroll_begin(&rig.ui, "inner", 60);
    cydui_label(&rig.ui, "x");
    cydui_scroll_end(&rig.ui);
    cydui_scroll_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_BAD_ARG);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
}

static void test_unbalanced_and_too_deep_scroll_areas_are_survived(void)
{
    int i;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_scroll_end(&rig.ui); /* an end without a begin */
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
    cydui_ui_clear_errors(&rig.ui);
    cydui_scroll_begin(&rig.ui, "left open", 50);
    cydui_ui_end(&rig.ui); /* closed for the application, with its clip released */
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    cydui_ui_begin(&rig.ui, 0);
    for (i = 0; i < CYDUI_MAX_LAYOUT_DEPTH + 3; i++) {
        cydui_begin_column(&rig.ui);
    }
    cydui_scroll_begin(&rig.ui, "too deep", 50);
    cydui_scroll_end(&rig.ui);
    for (i = 0; i < CYDUI_MAX_LAYOUT_DEPTH + 3; i++) {
        cydui_end_column(&rig.ui);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
}

static void test_a_scroll_area_without_state_room_still_draws_and_does_not_scroll(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&rig, 0, 0));
    rig_down(&rig, 100, 100);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "s", 100) { cydui_label(&rig.ui, "hello"); cydui_spacer(&rig.ui, 400); }
    cydui_ui_end(&rig.ui);
    rig_move(&rig, 100, 20);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "s", 100) { cydui_label(&rig.ui, "hello"); cydui_spacer(&rig.ui, 400); }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_the_state_entry_is_32_bytes(void)
{
    TEST_ASSERT_EQUAL_UINT32(32, (uint32_t)sizeof(cydui_state));
}

/* ---- lists ---- */

static int32_t first, last;
static int built;

static void list_frame(int32_t count, int item_h, int vh)
{
    int32_t i;

    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    cydui_list_begin(&rig.ui, "s", count, (int16_t)item_h, (int16_t)vh, &first, &last);
    built = 0;
    for (i = first; i < last; i++) {
        cydui_rect r;
        cydui_begin_list_item(&rig.ui, i);
        r = cydui_alloc_rect(&rig.ui, 10, 10);
        (void)r;
        cydui_label(&rig.ui, "same label"); /* equal in every row: ids are mixed with the index */
        cydui_end_list_item(&rig.ui);
        built++;
    }
    cydui_list_end(&rig.ui);
    coasting = cydui_ui_end(&rig.ui);
}

static void test_a_list_builds_only_the_visible_items(void)
{
    start();
    list_frame(1000, 30, 100); /* 100 px / 30 px: items 0..3 (the last one only half in) */
    TEST_ASSERT_EQUAL_INT32(0, first);
    TEST_ASSERT_EQUAL_INT32(4, last);
    TEST_ASSERT_EQUAL_INT(4, built);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    cydui_scroll_to(&rig.ui, "s", 45); /* item 1 is at the top, cut by 15 px */
    list_frame(1000, 30, 100);
    TEST_ASSERT_EQUAL_INT32(1, first);
    TEST_ASSERT_EQUAL_INT32(5, last); /* (45 + 100 + 29) / 30 */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    cydui_scroll_to(&rig.ui, "s", 30 * 1000 - 100); /* the end */
    list_frame(1000, 30, 100);
    TEST_ASSERT_EQUAL_INT32(996, first); /* 29900 / 30 */
    TEST_ASSERT_EQUAL_INT32(1000, last);
}

static void test_list_items_sit_where_their_index_says(void)
{
    cydui_rect r2;

    start();
    list_frame(1000, 30, 100);
    cydui_scroll_to(&rig.ui, "s", 45);
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    cydui_list_begin(&rig.ui, "s", 1000, 30, 100, &first, &last);
    cydui_begin_list_item(&rig.ui, 2);
    r2 = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_end_list_item(&rig.ui);
    cydui_list_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(8, r2.x);
    TEST_ASSERT_EQUAL_INT(8 + 2 * 30 - 45, r2.y); /* 23 */
}

static void test_empty_and_degenerate_lists(void)
{
    start();
    list_frame(0, 30, 100);
    TEST_ASSERT_EQUAL_INT32(0, first);
    TEST_ASSERT_EQUAL_INT32(0, last);
    list_frame(-5, 30, 100);
    TEST_ASSERT_EQUAL_INT32(0, last);
    TEST_ASSERT_EQUAL_INT32(0, cydui_state_get(&rig.ui, cydui_id_str(&rig.ui, "s"), false)->c); /* no content, not a negative one */
    list_frame(10, 0, 100);
    TEST_ASSERT_EQUAL_INT32(0, last);
    list_frame(10, -4, 100);
    TEST_ASSERT_EQUAL_INT32(0, last);
    list_frame(1, 30, 100);
    TEST_ASSERT_EQUAL_INT32(0, first);
    TEST_ASSERT_EQUAL_INT32(1, last);
    list_frame(10, 30, 0);
    TEST_ASSERT_EQUAL_INT32(0, last);
    list_frame(10, 30, -7);
    TEST_ASSERT_EQUAL_INT32(0, last);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_a_huge_list_is_capped_and_still_cheap(void)
{
    start();
    list_frame(INT32_MAX, 30, 100);
    TEST_ASSERT_EQUAL_INT32(0, first);
    TEST_ASSERT_EQUAL_INT32(4, last);
    cydui_scroll_to(&rig.ui, "s", INT_MAX);
    list_frame(INT32_MAX, 30, 100);
    /* the content is capped at 4194304 px: the end is at 4194204, item 139806 */
    TEST_ASSERT_EQUAL_INT32(139806, first);
    TEST_ASSERT_EQUAL_INT32(139811, last); /* (4194204 + 100 + 29) / 30 */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    list_frame(INT32_MAX, 32767, 100); /* the tallest item */
    TEST_ASSERT_TRUE(last - first <= 2);
}

static void test_a_list_drags_and_coasts_like_any_area(void)
{
    start();
    list_frame(1000, 30, 100);
    rig_down(&rig, 100, 90);
    list_frame(1000, 30, 100);
    rig_move(&rig, 100, 50);
    list_frame(1000, 30, 100);
    TEST_ASSERT_EQUAL_INT32(40 * 256, offset());
    TEST_ASSERT_EQUAL_INT32(1, first);
    rig_up(&rig, 100, 50);
    list_frame(1000, 30, 100);
    list_frame(1000, 30, 100);
    TEST_ASSERT_TRUE(offset() > 40 * 256); /* coasting */
}

static void test_list_calls_outside_a_list_are_survived(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_list_item(&rig.ui, 3); /* no list */
    cydui_label(&rig.ui, "x");
    cydui_end_list_item(&rig.ui);
    cydui_list_end(&rig.ui); /* no list */
    cydui_list_begin(&rig.ui, "s", 5, 30, 100, NULL, NULL); /* no outputs wanted */
    cydui_list_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.id_depth);
}

static void test_list_items_hold_their_own_state_and_ids(void)
{
    bool a = false, b = false;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_list_begin(&rig.ui, "s", 10, 40, 100, &first, &last);
    cydui_begin_list_item(&rig.ui, 0);
    cydui_checkbox(&rig.ui, "same", &a);
    cydui_end_list_item(&rig.ui);
    cydui_begin_list_item(&rig.ui, 1);
    cydui_checkbox(&rig.ui, "same", &b);
    cydui_end_list_item(&rig.ui);
    cydui_list_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui) & CYDUI_ERR_ID_COLLISION);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.id_depth);
}


static void test_a_drag_that_began_on_a_widget_outside_the_area_is_not_taken(void)
{
    start();
    warm_up(); /* area y = 8..107; a raw widget below it, at y = 116..145 */
    {
        int i;
        for (i = 0; i < 3; i++) {
            rig_advance(&rig, 16);
            if (i == 0) rig_down(&rig, 20, 130);
            if (i == 1) rig_move(&rig, 20, 60); /* into the area, far from the press */
            if (i == 2) rig_move(&rig, 20, 30);
            cydui_ui_begin(&rig.ui, 0);
            CYDUI_SCROLL(&rig.ui, "s", view_h) { cydui_spacer(&rig.ui, content_h); }
            {
                cydui_rect r = cydui_alloc_rect(&rig.ui, 100, 30);
                cydui_id id = cydui_id_str(&rig.ui, "below");
                cydui_id_claim(&rig.ui, id);
                last_resp = cydui_interact(&rig.ui, id, r, CYDUI_SENSE_CLICK | CYDUI_SENSE_DRAG);
                TEST_ASSERT_EQUAL_INT(116, r.y);
            }
            cydui_ui_end(&rig.ui);
        }
    }
    TEST_ASSERT_EQUAL_INT32(0, offset());
    TEST_ASSERT_TRUE(last_resp.dragging); /* the widget below still has the touch */
}

static void test_the_speed_is_per_second_whatever_the_frame_rate(void)
{
    start();
    content_h = 1000;
    warm_up();
    rig_down(&rig, 100, 90);
    frame();
    rig_advance(&rig, 16); /* 32 ms frames: 20 px / 32 ms = 625 px/s */
    rig_move(&rig, 100, 70);
    frame();
    rig_advance(&rig, 16);
    rig_move(&rig, 100, 50);
    frame();
    rig_advance(&rig, 16);
    rig_move(&rig, 100, 30);
    frame();
    TEST_ASSERT_EQUAL_INT32(546, speed()); /* ((0+625)/2 +625)/2 ... = 312, 468, 546 */
}

static void test_a_flick_downward_coasts_as_the_mirror_image_of_one_upward(void)
{
    int i;
    int32_t start_off = 600 * 256; /* far enough from the top that the coast does not reach it */

    start();
    content_h = 1000;
    warm_up();
    cydui_scroll_to(&rig.ui, "s", 600);
    frame();
    TEST_ASSERT_EQUAL_INT32(start_off, offset());
    rig_down(&rig, 100, 30);
    frame();
    rig_move(&rig, 100, 50);
    frame();
    rig_move(&rig, 100, 70);
    frame();
    rig_move(&rig, 100, 90);
    frame();
    rig_up(&rig, 100, 90);
    frame();
    TEST_ASSERT_EQUAL_INT32(-1093, speed());
    TEST_ASSERT_TRUE(coasting);
    for (i = 0; i < COAST_FRAMES; i++) {
        frame();
        TEST_ASSERT_EQUAL_INT32(start_off - coast_offsets[i], offset()); /* the 60 px of the drag are in the table too */
        TEST_ASSERT_TRUE(coasting == (i < COAST_FRAMES - 1));
    }
}

static void test_the_clock_gap_cap_is_exactly_one_hundred_ms(void)
{
    start();
    flick(0);
    rig_advance(&rig, 101 - 16);
    frame(); /* 101 ms later: counted as 100 */
    TEST_ASSERT_EQUAL_INT32(60 * 256 + 27980, offset());
}

static void test_a_nested_area_keeps_no_state_and_a_plain_one_does(void)
{
    start();
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    cydui_scroll_begin(&rig.ui, "outer", 150);
    cydui_scroll_begin(&rig.ui, "inner", 60);
    cydui_spacer(&rig.ui, 300);
    cydui_scroll_end(&rig.ui);
    cydui_scroll_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_NOT_NULL(cydui_state_get(&rig.ui, cydui_id_str(&rig.ui, "outer"), false));
    TEST_ASSERT_NULL(cydui_state_get(&rig.ui, cydui_id_str(&rig.ui, "inner"), false));
}

static void test_in_a_row_the_area_is_as_wide_as_its_window(void)
{
    cydui_rect after;

    start();
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_ROW(&rig.ui) {
        CYDUI_SCROLL(&rig.ui, "s", 50) { cydui_spacer(&rig.ui, 10); }
        after = cydui_alloc_rect(&rig.ui, 10, 10);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(8 + 304 + 8, after.x); /* the window took the whole row; content is 0 wide */
}

static void test_the_clip_is_the_whole_screen_again_after_an_area(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "s", 50) { cydui_spacer(&rig.ui, 10); }
    TEST_ASSERT_EQUAL_INT(0, rig.ui.ctx.clip.x);
    TEST_ASSERT_EQUAL_INT(RIG_W, rig.ui.ctx.clip.w);
    TEST_ASSERT_EQUAL_INT(RIG_H, rig.ui.ctx.clip.h);
    cydui_ui_end(&rig.ui);
}

static int bar_calls;
static cydui_paint_info bar_info;

static void count_bar(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *pi)
{
    (void)ui;
    (void)t;
    bar_calls++;
    bar_info = *pi;
}

static void test_the_bar_painter_is_called_only_when_there_is_something_to_scroll(void)
{
    start();
    rig.theme.paint[CYDUI_WIDGET_SCROLLBAR] = count_bar;
    content_h = 60; /* shorter than the 100 px window */
    warm_up();
    TEST_ASSERT_EQUAL_INT(0, bar_calls);
    content_h = 100; /* exactly as tall */
    warm_up();
    TEST_ASSERT_EQUAL_INT(0, bar_calls);

    content_h = 400;
    warm_up();
    cydui_scroll_to(&rig.ui, "s", 52);
    bar_calls = 0;
    frame();
    TEST_ASSERT_EQUAL_INT(1, bar_calls);
    TEST_ASSERT_EQUAL_INT(CYDUI_WIDGET_SCROLLBAR, bar_info.kind);
    TEST_ASSERT_EQUAL_INT(CYDUI_STATE_NORMAL, bar_info.state);
    TEST_ASSERT_EQUAL_INT32(52, bar_info.value);
    TEST_ASSERT_EQUAL_INT32(0, bar_info.min);
    TEST_ASSERT_EQUAL_INT32(300, bar_info.max);
    TEST_ASSERT_EQUAL_INT(306, bar_info.rect.x);
    TEST_ASSERT_EQUAL_INT(10, bar_info.rect.y);
    TEST_ASSERT_EQUAL_INT(4, bar_info.rect.w);
    TEST_ASSERT_EQUAL_INT(96, bar_info.rect.h);
    TEST_ASSERT_EQUAL_INT(8, bar_info.aux.x);
    TEST_ASSERT_EQUAL_INT(8, bar_info.aux.y);
    TEST_ASSERT_EQUAL_INT(304, bar_info.aux.w);
    TEST_ASSERT_EQUAL_INT(100, bar_info.aux.h);

    rig_down(&rig, 100, 90);
    frame();
    rig_move(&rig, 100, 60);
    frame();
    TEST_ASSERT_EQUAL_INT(CYDUI_STATE_PRESSED, bar_info.state); /* held while it is being dragged */
}

static void test_the_default_bar_stays_in_its_track_and_has_a_minimum_size(void)
{
    cydui_paint_info pi;
    const cydui_color thumb = rig.theme.table[CYDUI_WIDGET_SCROLLBAR][0][CYDUI_STATE_NORMAL].accent;

    start();
    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_SCROLLBAR;
    pi.rect = (cydui_rect){50, 10, 4, 96};
    pi.aux = (cydui_rect){0, 0, 100, 100};
    pi.min = 0;
    pi.max = 300;
    pi.value = 1000; /* beyond the end: the thumb stays at the bottom of its track */
    cydui_ui_begin(&rig.ui, 0);
    cydui_paint(&rig.ui, &pi);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_HEX16(thumb, px(52, 94));
    TEST_ASSERT_EQUAL_HEX16(0, px(52, 60));
    TEST_ASSERT_EQUAL_HEX16(0, px(52, 106));

    pi.max = 3900; /* 4000 px of content: 2 px by proportion, 16 by the minimum */
    pi.value = 0;
    cydui_ui_begin(&rig.ui, 0);
    cydui_paint(&rig.ui, &pi);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_HEX16(thumb, px(52, 24));
    TEST_ASSERT_EQUAL_HEX16(0, px(52, 30));
}


static void test_the_bar_minimum_is_sixteen_pixels(void)
{
    cydui_paint_info pi;
    const cydui_color thumb = rig.theme.table[CYDUI_WIDGET_SCROLLBAR][0][CYDUI_STATE_NORMAL].accent;

    start();
    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_SCROLLBAR;
    pi.rect = (cydui_rect){50, 10, 4, 96};
    pi.aux = (cydui_rect){0, 0, 100, 100};
    pi.max = 700; /* 800 px of content: 12 px by proportion, 16 by the minimum */
    cydui_ui_begin(&rig.ui, 0);
    cydui_paint(&rig.ui, &pi);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_HEX16(thumb, px(52, 24));
    TEST_ASSERT_EQUAL_HEX16(0, px(52, 28));
}

static void test_the_release_frame_counts_when_the_finger_still_moved_in_it(void)
{
    /* the finger goes up 60 px in 3 frames, then 20 px further and lifts in the same frame */
    start();
    content_h = 1000;
    warm_up();
    rig_down(&rig, 100, 90);
    frame();
    rig_move(&rig, 100, 70);
    frame();
    rig_move(&rig, 100, 50);
    frame();
    rig_move(&rig, 100, 30);
    frame();
    rig_move(&rig, 100, 10);
    rig_up(&rig, 100, 10);
    frame();
    TEST_ASSERT_EQUAL_INT32(80 * 256, offset());
    TEST_ASSERT_EQUAL_INT32(1171, speed()); /* (1093 + 1250) / 2 */
}

static void test_a_touch_outside_the_window_does_not_stop_the_coast(void)
{
    int32_t before;

    start();
    flick(0);
    frame();
    before = offset();
    rig_down(&rig, 100, 200); /* well below the window (y = 8..107) */
    frame();
    TEST_ASSERT_TRUE(speed() > 0);
    TEST_ASSERT_TRUE(offset() > before);
}

static void test_a_nested_area_paints_no_bar(void)
{
    start();
    rig.theme.paint[CYDUI_WIDGET_SCROLLBAR] = count_bar;
    bar_calls = 0;
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    cydui_scroll_begin(&rig.ui, "outer", 150);
    cydui_scroll_begin(&rig.ui, "inner", 60);
    cydui_spacer(&rig.ui, 300); /* taller than the 60 px it was given */
    cydui_scroll_end(&rig.ui);
    cydui_scroll_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(0, bar_calls);
}

static void test_the_window_edges_are_where_a_touch_stops_a_coast(void)
{
    static const int inside_[4][2] = {{8, 50}, {311, 50}, {100, 8}, {100, 107}};
    static const int outside_[4][2] = {{7, 50}, {312, 50}, {100, 7}, {100, 108}};
    int i;

    for (i = 0; i < 4; i++) {
        start();
        flick(0);
        frame();
        rig_down(&rig, inside_[i][0], inside_[i][1]);
        frame();
        TEST_ASSERT_EQUAL_INT32_MESSAGE(0, speed(), "a touch on the edge of the window did not stop it");
    }
    for (i = 0; i < 4; i++) {
        start();
        flick(0);
        frame();
        rig_down(&rig, outside_[i][0], outside_[i][1]);
        frame();
        TEST_ASSERT_TRUE_MESSAGE(speed() > 0, "a touch just outside the window stopped it");
    }
}

static void release_with_speed(int32_t v)
{
    cydui_state *st;

    start();
    content_h = 1000;
    warm_up();
    rig_down(&rig, 100, 90);
    frame();
    rig_move(&rig, 100, 70);
    frame();
    st = cydui_state_get(&rig.ui, cydui_id_str(&rig.ui, "s"), false);
    TEST_ASSERT_NOT_NULL(st);
    st->b = v;
    rig_up(&rig, 100, 70);
    frame();
}

static void test_a_flick_of_exactly_the_minimum_speed_coasts_and_one_below_does_not(void)
{
    release_with_speed(CYDUI_SCROLL_FLING_MIN);
    TEST_ASSERT_EQUAL_INT32(CYDUI_SCROLL_FLING_MIN, speed());
    release_with_speed(CYDUI_SCROLL_FLING_MIN - 1);
    TEST_ASSERT_EQUAL_INT32(0, speed());
    release_with_speed(-CYDUI_SCROLL_FLING_MIN);
    TEST_ASSERT_EQUAL_INT32(-CYDUI_SCROLL_FLING_MIN, speed());
    release_with_speed(-(CYDUI_SCROLL_FLING_MIN - 1));
    TEST_ASSERT_EQUAL_INT32(0, speed());
}

static void test_the_height_of_an_area_is_what_it_was_given(void)
{
    start();
    rig.theme.paint[CYDUI_WIDGET_SCROLLBAR] = count_bar;
    warm_up();
    TEST_ASSERT_EQUAL_INT(100, bar_info.aux.h); /* the argument */

    bar_calls = 0;
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_height(&rig.ui, 50); /* next_height wins over the argument */
    CYDUI_SCROLL(&rig.ui, "s", 100) { cydui_spacer(&rig.ui, 400); }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(50, bar_info.aux.h);

    bar_calls = 0;
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_height(&rig.ui, 0); /* a height of nothing is a height */
    CYDUI_SCROLL(&rig.ui, "s", 100) { cydui_spacer(&rig.ui, 400); }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(0, bar_calls);

    bar_calls = 0;
    {
        int i;
        for (i = 0; i < 3; i++) { /* and fill takes what the container has */
            rig_advance(&rig, 16);
            cydui_ui_begin(&rig.ui, 0);
            cydui_next_height(&rig.ui, 150);
            cydui_begin_column(&rig.ui);
            cydui_next_fill(&rig.ui);
            CYDUI_SCROLL(&rig.ui, "s", 50) { cydui_spacer(&rig.ui, 400); }
            cydui_end_column(&rig.ui);
            cydui_ui_end(&rig.ui);
        }
    }
    TEST_ASSERT_EQUAL_INT(150, bar_info.aux.h);
}

static void test_the_bar_needs_a_window_of_at_least_five_by_nine(void)
{
    int w, h;

    start();
    rig.theme.paint[CYDUI_WIDGET_SCROLLBAR] = count_bar;
    for (h = 3; h <= 6; h++) { /* the track is the window less 2 px each side */
        for (w = 7; w <= 10; w++) {
            int i;
            bar_calls = 0;
            for (i = 0; i < 3; i++) {
                rig_advance(&rig, 16);
                cydui_ui_begin(&rig.ui, 0);
                cydui_next_width(&rig.ui, w);
                CYDUI_SCROLL(&rig.ui, "s", h) { cydui_spacer(&rig.ui, 400); }
                cydui_ui_end(&rig.ui);
            }
            bar_calls = 0;
            rig_advance(&rig, 16);
            cydui_ui_begin(&rig.ui, 0);
            cydui_next_width(&rig.ui, w);
            CYDUI_SCROLL(&rig.ui, "s", h) { cydui_spacer(&rig.ui, 400); }
            cydui_ui_end(&rig.ui);
            TEST_ASSERT_EQUAL_INT_MESSAGE((h >= 5 && w >= 9) ? 1 : 0, bar_calls, "bar drawn in a window too small for it");
        }
    }
}

static void test_a_change_of_content_or_a_jump_asks_for_another_frame(void)
{
    start();
    warm_up();
    frame();
    TEST_ASSERT_FALSE(coasting);
    content_h = 250;
    frame();
    TEST_ASSERT_TRUE(coasting); /* the clamp of this frame used the old height */
    frame();
    TEST_ASSERT_FALSE(coasting);
    cydui_scroll_to(&rig.ui, "s", 10);
    frame();
    TEST_ASSERT_TRUE(coasting);
    frame();
    TEST_ASSERT_FALSE(coasting);
}

static void test_a_skipped_scroll_area_ends_without_touching_the_one_that_is_open(void)
{
    int d;

    start();
    rig.theme.paint[CYDUI_WIDGET_SCROLLBAR] = count_bar;
    warm_up();
    rig_advance(&rig, 16);
    cydui_ui_begin(&rig.ui, 0);
    for (d = 0; d < CYDUI_MAX_LAYOUT_DEPTH - 1; d++) {
        cydui_begin_column(&rig.ui);
    }
    cydui_scroll_begin(&rig.ui, "s", 50); /* the last level there is */
    cydui_spacer(&rig.ui, 400);
    cydui_scroll_begin(&rig.ui, "too deep", 20); /* skipped */
    bar_calls = 0;
    cydui_scroll_end(&rig.ui);                   /* its end: nothing happens to the open one */
    TEST_ASSERT_EQUAL_INT(0, bar_calls);
    TEST_ASSERT_EQUAL_UINT8(CYDUI_MAX_LAYOUT_DEPTH, rig.ui.depth);
    cydui_scroll_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(1, bar_calls); /* the bar of the one that was open, once */
    for (d = 0; d < CYDUI_MAX_LAYOUT_DEPTH - 1; d++) {
        cydui_end_column(&rig.ui);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
}

static void test_an_item_in_a_list_with_no_item_height_is_a_misuse_and_is_survived(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_list_begin(&rig.ui, "s", 5, 0, 100, NULL, NULL);
    cydui_begin_list_item(&rig.ui, 0);
    cydui_label(&rig.ui, "x");
    cydui_end_list_item(&rig.ui);
    cydui_list_end(&rig.ui);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    cydui_ui_end(&rig.ui);

    cydui_ui_clear_errors(&rig.ui);
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_list_item(&rig.ui, 0); /* no list at all */
    cydui_end_list_item(&rig.ui);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    cydui_ui_end(&rig.ui);
}

static void test_rows_with_fill_in_different_items_do_not_share_their_numbers(void)
{
    int f, i;
    int32_t first_, last_;

    start();
    for (f = 0; f < 8; f++) {
        rig_advance(&rig, 16);
        cydui_ui_begin(&rig.ui, 0);
        cydui_list_begin(&rig.ui, "s", 10, 40, 100, &first_, &last_);
        for (i = first_; i < last_; i++) {
            cydui_begin_list_item(&rig.ui, i);
            CYDUI_ROW(&rig.ui) {
                cydui_alloc_rect(&rig.ui, 10 + i * 20, 10);
                cydui_next_fill(&rig.ui);
                cydui_alloc_rect(&rig.ui, 10, 10);
            }
            cydui_end_list_item(&rig.ui);
        }
        cydui_list_end(&rig.ui);
        coasting = cydui_ui_end(&rig.ui);
    }
    TEST_ASSERT_FALSE_MESSAGE(coasting, "the layout of the rows never settled");
}

static void test_the_list_cap_is_exactly_four_million_pixels(void)
{
    start();
    list_frame(4194305, 1, 100);
    cydui_scroll_to(&rig.ui, "s", INT_MAX);
    list_frame(4194305, 1, 100);
    TEST_ASSERT_EQUAL_INT32(4194204, first);
    TEST_ASSERT_EQUAL_INT32(4194304, last);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_dragging_moves_the_content_by_the_travel_beyond_the_slop);
    RUN_TEST(test_the_offset_stops_at_both_ends);
    RUN_TEST(test_the_bottom_is_content_minus_view);
    RUN_TEST(test_content_shorter_than_the_view_does_not_scroll_or_show_a_bar);
    RUN_TEST(test_the_scroll_bar_follows_the_offset);
    RUN_TEST(test_a_tap_on_a_child_still_clicks);
    RUN_TEST(test_a_drag_that_began_on_a_child_is_taken_over_before_the_child_sees_it);
    RUN_TEST(test_a_slider_inside_keeps_the_touch);
    RUN_TEST(test_a_drag_that_began_outside_the_area_does_not_scroll_it);
    RUN_TEST(test_a_flick_coasts_along_the_computed_path_and_stops);
    RUN_TEST(test_the_same_flick_gives_the_same_path_whenever_it_happens);
    RUN_TEST(test_a_finger_that_rests_before_letting_go_does_not_fling);
    RUN_TEST(test_a_slow_drag_does_not_coast);
    RUN_TEST(test_touching_a_coasting_area_stops_it);
    RUN_TEST(test_coasting_stops_dead_at_the_end);
    RUN_TEST(test_without_a_clock_there_is_no_coasting);
    RUN_TEST(test_a_frozen_clock_neither_coasts_nor_hangs);
    RUN_TEST(test_a_long_pause_between_frames_counts_as_one_hundred_ms);
    RUN_TEST(test_scrolled_content_is_the_unscrolled_content_moved_up);
    RUN_TEST(test_an_area_with_no_content_cannot_be_dragged_past_the_offset_cap);
    RUN_TEST(test_scroll_to_jumps_and_clamps);
    RUN_TEST(test_scroll_to_stops_a_coast);
    RUN_TEST(test_a_scroll_area_inside_another_is_a_plain_column);
    RUN_TEST(test_unbalanced_and_too_deep_scroll_areas_are_survived);
    RUN_TEST(test_a_scroll_area_without_state_room_still_draws_and_does_not_scroll);
    RUN_TEST(test_the_state_entry_is_32_bytes);
    RUN_TEST(test_a_list_builds_only_the_visible_items);
    RUN_TEST(test_list_items_sit_where_their_index_says);
    RUN_TEST(test_empty_and_degenerate_lists);
    RUN_TEST(test_a_huge_list_is_capped_and_still_cheap);
    RUN_TEST(test_a_list_drags_and_coasts_like_any_area);
    RUN_TEST(test_list_calls_outside_a_list_are_survived);
    RUN_TEST(test_list_items_hold_their_own_state_and_ids);
    RUN_TEST(test_a_drag_that_began_on_a_widget_outside_the_area_is_not_taken);
    RUN_TEST(test_the_speed_is_per_second_whatever_the_frame_rate);
    RUN_TEST(test_a_flick_downward_coasts_as_the_mirror_image_of_one_upward);
    RUN_TEST(test_the_clock_gap_cap_is_exactly_one_hundred_ms);
    RUN_TEST(test_a_nested_area_keeps_no_state_and_a_plain_one_does);
    RUN_TEST(test_in_a_row_the_area_is_as_wide_as_its_window);
    RUN_TEST(test_the_clip_is_the_whole_screen_again_after_an_area);
    RUN_TEST(test_the_bar_painter_is_called_only_when_there_is_something_to_scroll);
    RUN_TEST(test_the_default_bar_stays_in_its_track_and_has_a_minimum_size);
    RUN_TEST(test_the_list_cap_is_exactly_four_million_pixels);
    RUN_TEST(test_the_bar_minimum_is_sixteen_pixels);
    RUN_TEST(test_the_release_frame_counts_when_the_finger_still_moved_in_it);
    RUN_TEST(test_a_touch_outside_the_window_does_not_stop_the_coast);
    RUN_TEST(test_a_nested_area_paints_no_bar);
    RUN_TEST(test_the_window_edges_are_where_a_touch_stops_a_coast);
    RUN_TEST(test_a_flick_of_exactly_the_minimum_speed_coasts_and_one_below_does_not);
    RUN_TEST(test_the_height_of_an_area_is_what_it_was_given);
    RUN_TEST(test_the_bar_needs_a_window_of_at_least_five_by_nine);
    RUN_TEST(test_a_change_of_content_or_a_jump_asks_for_another_frame);
    RUN_TEST(test_a_skipped_scroll_area_ends_without_touching_the_one_that_is_open);
    RUN_TEST(test_an_item_in_a_list_with_no_item_height_is_a_misuse_and_is_survived);
    RUN_TEST(test_rows_with_fill_in_different_items_do_not_share_their_numbers);
    return UNITY_END();
}
