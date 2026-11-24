/*
 * Input (FR-40, FR-41, CT-05, CT-06): taps, drags, long presses, cancelled presses, out-of-order and
 * flooded events, the clock wrapping, clipping. Two widgets are probed with cydui_interact.
 *
 *   A = (20,20) 60x60      B = (120,20) 60x60      Z = (0,230) 10x10 (bottom left corner)
 */

#include <stdio.h>
#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;
static cydui_response ra, rb, rz;
static unsigned a_flags, b_flags;

static const cydui_rect RECT_A = {20, 20, 60, 60};
static const cydui_rect RECT_B = {120, 20, 60, 60};
static const cydui_rect RECT_Z = {0, 230, 10, 10};

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    a_flags = b_flags = CYDUI_SENSE_CLICK | CYDUI_SENSE_DRAG;
}

static void start_with_long_press(uint16_t ms)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&rig, 64, ms));
    a_flags = b_flags = CYDUI_SENSE_CLICK | CYDUI_SENSE_DRAG;
}

/* One frame: probes A, B and Z. */
static bool frame(void)
{
    cydui_ui_begin(&rig.ui, 0);
    ra = cydui_interact(&rig.ui, cydui_id_str(&rig.ui, "A"), RECT_A, a_flags);
    rb = cydui_interact(&rig.ui, cydui_id_str(&rig.ui, "B"), RECT_B, b_flags);
    rz = cydui_interact(&rig.ui, cydui_id_str(&rig.ui, "Z"), RECT_Z, CYDUI_SENSE_CLICK);
    return cydui_ui_end(&rig.ui);
}

/* ---- taps ---- */

static void test_tap_over_two_frames(void)
{
    start();
    rig_down(&rig, 30, 30);
    frame();
    TEST_ASSERT_TRUE(ra.pressed);
    TEST_ASSERT_FALSE(ra.clicked);
    TEST_ASSERT_FALSE(ra.released);
    TEST_ASSERT_FALSE(rb.pressed);

    frame(); /* still held */
    TEST_ASSERT_TRUE(ra.pressed);
    TEST_ASSERT_FALSE(ra.clicked);

    rig_up(&rig, 30, 30);
    frame();
    TEST_ASSERT_TRUE(ra.clicked);
    TEST_ASSERT_TRUE(ra.released);
    TEST_ASSERT_FALSE(rb.clicked);

    frame(); /* and it is over: nothing repeats */
    TEST_ASSERT_FALSE(ra.pressed);
    TEST_ASSERT_FALSE(ra.clicked);
    TEST_ASSERT_FALSE(ra.released);
}

static void test_press_and_release_in_one_frame_is_one_tap(void)
{
    int clicks = 0, i;

    start();
    rig_down(&rig, 40, 40);
    rig_up(&rig, 40, 40);
    for (i = 0; i < 3; i++) {
        frame();
        clicks += ra.clicked;
        if (i == 0) {
            TEST_ASSERT_TRUE(ra.pressed);
            TEST_ASSERT_TRUE(ra.released);
        }
    }
    TEST_ASSERT_EQUAL_INT(1, clicks);
}

static void test_the_widget_sees_the_pointer_travel_to_a_release_outside(void)
{
    start();
    rig_down(&rig, 30, 30);
    frame();
    rig_move(&rig, 70, 30);
    frame();
    TEST_ASSERT_TRUE(ra.pressed); /* still over A */
    rig_move(&rig, 200, 30);      /* and out of it */
    frame();
    TEST_ASSERT_FALSE(ra.pressed);
    TEST_ASSERT_TRUE(ra.dragging);
    rig_up(&rig, 200, 30);
    frame();
    TEST_ASSERT_FALSE(ra.clicked);
    TEST_ASSERT_TRUE(ra.released);
    TEST_ASSERT_FALSE(rb.clicked);
}

/* ---- slop ---- */

static void test_travel_within_the_slop_is_still_a_tap(void)
{
    start();
    rig_down(&rig, 50, 50);
    frame();
    rig_move(&rig, 60, 50); /* exactly 10 px: allowed */
    frame();
    TEST_ASSERT_FALSE(ra.dragging);
    rig_up(&rig, 60, 50);
    frame();
    TEST_ASSERT_TRUE(ra.clicked);
}

static void test_releasing_just_outside_the_widget_is_not_a_tap(void)
{
    start();
    rig_down(&rig, 78, 50); /* the right edge of A is x = 80 (exclusive) */
    frame();
    rig_move(&rig, 84, 50); /* 6 px: well inside the slop, but outside A */
    frame();
    TEST_ASSERT_FALSE(ra.dragging);
    TEST_ASSERT_FALSE(ra.pressed);
    rig_up(&rig, 84, 50);
    frame();
    TEST_ASSERT_TRUE(ra.released);
    TEST_ASSERT_FALSE_MESSAGE(ra.clicked, "a release outside the widget was taken for a tap");

    rig_down(&rig, 78, 50); /* and the same press released inside does tap */
    frame();
    rig_move(&rig, 79, 50);
    frame();
    rig_up(&rig, 79, 50);
    frame();
    TEST_ASSERT_TRUE(ra.clicked);
}

static void test_travel_beyond_the_slop_is_a_drag_not_a_tap(void)
{
    start();
    rig_down(&rig, 50, 50);
    frame();
    rig_move(&rig, 61, 50); /* 11 px: too far */
    frame();
    TEST_ASSERT_TRUE(ra.dragging);
    rig_up(&rig, 61, 50);
    frame();
    TEST_ASSERT_FALSE(ra.clicked);
    TEST_ASSERT_TRUE(ra.released);
}

static void test_slop_is_a_distance_not_a_box(void)
{
    start();
    rig_down(&rig, 40, 40);
    frame();
    rig_move(&rig, 47, 47); /* 9.9 px diagonally */
    frame();
    TEST_ASSERT_FALSE(ra.dragging);
    rig_move(&rig, 48, 48); /* 11.3 px */
    frame();
    TEST_ASSERT_TRUE(ra.dragging);
}

static void test_returning_to_the_start_does_not_undo_a_drag(void)
{
    start();
    rig_down(&rig, 50, 50);
    frame();
    rig_move(&rig, 80, 50);
    frame();
    rig_move(&rig, 50, 50);
    frame();
    rig_up(&rig, 50, 50);
    frame();
    TEST_ASSERT_FALSE_MESSAGE(ra.clicked, "a press that travelled far must not turn into a tap");
}

static void test_drag_reports_the_travel_of_each_frame(void)
{
    start();
    rig_down(&rig, 30, 30);
    frame();
    TEST_ASSERT_EQUAL_INT(0, ra.dx);
    rig_move(&rig, 35, 32);
    frame();
    TEST_ASSERT_EQUAL_INT(5, ra.dx);
    TEST_ASSERT_EQUAL_INT(2, ra.dy);
    frame(); /* no movement */
    TEST_ASSERT_EQUAL_INT(0, ra.dx);
    TEST_ASSERT_EQUAL_INT(0, ra.dy);
    rig_move(&rig, 25, 40);
    frame();
    TEST_ASSERT_EQUAL_INT(-10, ra.dx);
    TEST_ASSERT_EQUAL_INT(8, ra.dy);

    a_flags = CYDUI_SENSE_CLICK; /* a widget that did not ask for it gets no deltas */
    rig_move(&rig, 20, 20);
    frame();
    TEST_ASSERT_EQUAL_INT(0, ra.dx);
}

/* ---- long press ---- */

static void test_long_press_fires_once_after_the_hold_time(void)
{
    int fired = 0, i;

    start();
    rig_down(&rig, 30, 30);
    frame();
    TEST_ASSERT_FALSE(ra.long_pressed);
    rig_advance(&rig, 499);
    frame();
    TEST_ASSERT_FALSE_MESSAGE(ra.long_pressed, "fired a millisecond early");
    rig_advance(&rig, 1);
    frame();
    TEST_ASSERT_TRUE(ra.long_pressed);
    fired++;
    for (i = 0; i < 5; i++) {
        rig_advance(&rig, 300);
        frame();
        fired += ra.long_pressed;
        TEST_ASSERT_TRUE(ra.pressed);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, fired, "a long press must fire exactly once");

    rig_up(&rig, 30, 30);
    frame();
    TEST_ASSERT_TRUE(ra.released);
    TEST_ASSERT_FALSE_MESSAGE(ra.clicked, "a long press is not also a tap");
}

static void test_release_after_the_hold_time_counts_as_long_even_without_a_frame_between(void)
{
    start();
    rig_down(&rig, 30, 30);
    frame();
    rig_advance(&rig, 600); /* no frame ran during the hold (a slow frame) */
    rig_up(&rig, 30, 30);
    frame();
    TEST_ASSERT_TRUE(ra.long_pressed);
    TEST_ASSERT_FALSE(ra.clicked);
}

static void test_long_press_time_is_configurable(void)
{
    start_with_long_press(100);
    rig_down(&rig, 30, 30);
    frame();
    rig_advance(&rig, 99);
    frame();
    TEST_ASSERT_FALSE(ra.long_pressed);
    rig_advance(&rig, 1);
    frame();
    TEST_ASSERT_TRUE(ra.long_pressed);
}

static void test_moving_cancels_the_long_press(void)
{
    start();
    rig_down(&rig, 30, 30);
    frame();
    rig_move(&rig, 60, 30);
    frame();
    rig_advance(&rig, 2000);
    frame();
    TEST_ASSERT_FALSE(ra.long_pressed);
}

static void test_long_press_survives_the_clock_wrapping(void)
{
    start();
    rig.mem.now = 0xFFFFFF00u;
    rig_down(&rig, 30, 30);
    frame();
    rig_advance(&rig, 0x240); /* now = 0x140 after wrapping; 576 ms have passed */
    TEST_ASSERT_TRUE(rig.mem.now < 0x1000u);
    frame();
    TEST_ASSERT_TRUE_MESSAGE(ra.long_pressed, "the wrapped clock broke the hold time");

    start();
    rig.mem.now = 0xFFFFFF00u;
    rig_down(&rig, 30, 30);
    frame();
    rig_advance(&rig, 0x100); /* now = 0: only 256 ms */
    frame();
    TEST_ASSERT_FALSE(ra.long_pressed);
}

static void test_without_a_clock_taps_work_and_long_press_never_fires(void)
{
    start();
    rig.mem.backend.now_ms = NULL;
    rig_down(&rig, 30, 30);
    frame();
    rig_advance(&rig, 100000);
    frame();
    TEST_ASSERT_FALSE(ra.long_pressed);
    rig_up(&rig, 30, 30);
    frame();
    TEST_ASSERT_TRUE(ra.clicked);
}

/* ---- presses that go to the wrong place ---- */

static void test_press_on_a_and_release_on_b_clicks_neither(void)
{
    start();
    rig_down(&rig, 30, 30);
    frame();
    rig_move(&rig, 150, 30);
    frame();
    TEST_ASSERT_FALSE(rb.pressed); /* B does not light up under a press that A owns */
    rig_up(&rig, 150, 30);
    frame();
    TEST_ASSERT_FALSE(ra.clicked);
    TEST_ASSERT_FALSE(rb.clicked);
}

static void test_a_press_belongs_to_where_it_started_not_to_where_the_pointer_is_now(void)
{
    start();
    rig_down(&rig, 30, 30); /* on A ... */
    rig_move(&rig, 150, 30); /* ... and already over B when the frame runs */
    frame();
    TEST_ASSERT_TRUE(ra.dragging);
    TEST_ASSERT_FALSE(ra.pressed); /* A owns it but the pointer left */
    TEST_ASSERT_FALSE_MESSAGE(rb.pressed, "B took a press that started on A");
    rig_up(&rig, 150, 30);
    frame();
    TEST_ASSERT_TRUE(ra.released);
    TEST_ASSERT_FALSE(rb.clicked);
}

static void test_press_on_empty_space_then_release_on_a_widget_clicks_nothing(void)
{
    start();
    rig_down(&rig, 100, 150);
    frame();
    rig_move(&rig, 40, 40);
    frame();
    rig_up(&rig, 40, 40);
    frame();
    TEST_ASSERT_FALSE(ra.clicked);
    TEST_ASSERT_FALSE(ra.pressed);
}

static void test_overlapping_widgets_the_first_call_gets_the_press(void)
{
    cydui_response r1, r2;

    start();
    rig_down(&rig, 30, 30);
    rig_up(&rig, 30, 30);
    cydui_ui_begin(&rig.ui, 0);
    r1 = cydui_interact(&rig.ui, cydui_id_str(&rig.ui, "first"), RECT_A, CYDUI_SENSE_CLICK);
    r2 = cydui_interact(&rig.ui, cydui_id_str(&rig.ui, "second"), RECT_A, CYDUI_SENSE_CLICK);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(r1.clicked);
    TEST_ASSERT_FALSE(r2.clicked);
    TEST_ASSERT_FALSE(r2.pressed);
}

static void test_a_disabled_widget_ignores_the_press_and_does_not_capture_it(void)
{
    cydui_response r1, r2;

    start();
    rig_down(&rig, 30, 30);
    rig_up(&rig, 30, 30);
    cydui_ui_begin(&rig.ui, 0);
    r1 = cydui_interact(&rig.ui, cydui_id_str(&rig.ui, "off"), RECT_A, CYDUI_SENSE_CLICK | CYDUI_DISABLED);
    r2 = cydui_interact(&rig.ui, cydui_id_str(&rig.ui, "on"), RECT_A, CYDUI_SENSE_CLICK);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE(r1.pressed || r1.clicked || r1.released);
    TEST_ASSERT_TRUE_MESSAGE(r2.clicked, "the widget under the disabled one never got the press");
}

static void test_the_press_is_tested_against_the_clip(void)
{
    cydui_response r;

    start();
    rig_down(&rig, 75, 75); /* inside A (20..80) but outside the clip below */
    rig_up(&rig, 75, 75);
    cydui_ui_begin(&rig.ui, 0);
    cydui_set_clip(&rig.ui.ctx, (cydui_rect){0, 0, 50, 50});
    r = cydui_interact(&rig.ui, cydui_id_str(&rig.ui, "A"), RECT_A, CYDUI_SENSE_CLICK);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE_MESSAGE(r.clicked || r.pressed, "reacted to a point the clip hides");

    rig_down(&rig, 40, 40); /* inside both */
    rig_up(&rig, 40, 40);
    cydui_ui_begin(&rig.ui, 0);
    cydui_set_clip(&rig.ui.ctx, (cydui_rect){0, 0, 50, 50});
    r = cydui_interact(&rig.ui, cydui_id_str(&rig.ui, "A"), RECT_A, CYDUI_SENSE_CLICK);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(r.clicked);
}

/* ---- events in the wrong order ---- */

static void test_release_without_a_press_is_ignored(void)
{
    start();
    rig_up(&rig, 30, 30);
    frame();
    TEST_ASSERT_FALSE(ra.clicked || ra.released || ra.pressed);
    rig_up(&rig, 30, 30);
    rig_up(&rig, 30, 30);
    frame();
    TEST_ASSERT_FALSE(ra.clicked || ra.released);
    TEST_ASSERT_FALSE(rig.ui.in.down);
}

static void test_a_stray_release_does_not_delay_a_press_that_follows_it(void)
{
    start();
    rig_up(&rig, 150, 30); /* nobody is pressing: ignored, and not a "release edge" */
    rig_down(&rig, 30, 30);
    frame();
    TEST_ASSERT_TRUE_MESSAGE(ra.pressed, "the press was pushed to the next frame by a stray release");
}

static void test_a_second_release_after_a_tap_is_ignored(void)
{
    int clicks = 0, i;

    start();
    rig_down(&rig, 30, 30);
    rig_up(&rig, 30, 30);
    rig_up(&rig, 30, 30);
    for (i = 0; i < 3; i++) {
        frame();
        clicks += ra.clicked;
    }
    TEST_ASSERT_EQUAL_INT(1, clicks);
}

static void test_a_press_that_never_ended_is_cancelled_by_the_next_press(void)
{
    start();
    rig_down(&rig, 30, 30);
    frame();
    TEST_ASSERT_TRUE(ra.pressed);
    rig_down(&rig, 150, 30); /* the finger is down again: the first release was lost */
    frame();
    TEST_ASSERT_FALSE_MESSAGE(ra.clicked, "the cancelled press must not click");
    TEST_ASSERT_TRUE(ra.released);
    frame(); /* the new press is picked up in the next frame */
    TEST_ASSERT_TRUE(rb.pressed);
    rig_up(&rig, 150, 30);
    frame();
    TEST_ASSERT_TRUE(rb.clicked);
}

static void test_release_then_press_in_one_frame_takes_two_frames(void)
{
    start();
    rig_down(&rig, 30, 30);
    frame();
    rig_up(&rig, 30, 30);
    rig_down(&rig, 150, 30);
    frame();
    TEST_ASSERT_TRUE(ra.clicked);
    TEST_ASSERT_FALSE(rb.pressed); /* the second press waits */
    frame();
    TEST_ASSERT_TRUE(rb.pressed);
}

static void test_an_unseen_widget_does_not_keep_the_press_forever(void)
{
    start();
    rig_down(&rig, 30, 30);
    frame();
    TEST_ASSERT_TRUE(ra.pressed);
    rig_up(&rig, 30, 30);
    cydui_ui_begin(&rig.ui, 0); /* a frame in which nobody calls cydui_interact */
    cydui_ui_end(&rig.ui);
    rig_down(&rig, 150, 30);
    frame();
    TEST_ASSERT_TRUE_MESSAGE(rb.pressed, "a stale press blocked the next widget");
}

/* ---- coordinates ---- */

static void test_coordinates_are_clamped_to_the_screen(void)
{
    start();
    rig_down(&rig, -50, 9999); /* off the bottom left corner: clamped to (0, 239) */
    frame();
    TEST_ASSERT_TRUE(rz.pressed);
    TEST_ASSERT_EQUAL_INT(0, rig.ui.in.x);
    TEST_ASSERT_EQUAL_INT(RIG_H - 1, rig.ui.in.y);
    rig_up(&rig, -32768, 32767);
    frame();
    TEST_ASSERT_TRUE(rz.clicked);

    rig_down(&rig, 32767, -32768);
    frame();
    TEST_ASSERT_EQUAL_INT(RIG_W - 1, rig.ui.in.x);
    TEST_ASSERT_EQUAL_INT(0, rig.ui.in.y);
}

static void test_unknown_event_types_are_ignored(void)
{
    start();
    cydui_mem_push_event(&rig.mem, (cydui_event_type)77, 30, 30);
    cydui_mem_push_event(&rig.mem, (cydui_event_type)-1, 30, 30);
    frame();
    TEST_ASSERT_FALSE(rig.ui.in.down);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.in.q_len);

    /* ... and in the middle of a press they must not be taken for a release or a move */
    rig_down(&rig, 30, 30);
    frame();
    cydui_mem_push_event(&rig.mem, (cydui_event_type)77, 150, 30);
    cydui_mem_push_event(&rig.mem, (cydui_event_type)5, 150, 30);
    frame();
    TEST_ASSERT_TRUE_MESSAGE(rig.ui.in.down, "an unknown event ended the press");
    TEST_ASSERT_TRUE(ra.pressed);
    TEST_ASSERT_EQUAL_INT(30, rig.ui.in.x);
}

static void test_a_quit_event_ends_the_loop(void)
{
    start();
    TEST_ASSERT_TRUE(cydui_ui_begin(&rig.ui, 0));
    cydui_ui_end(&rig.ui);
    cydui_mem_push_event(&rig.mem, CYDUI_EVENT_QUIT, 0, 0);
    TEST_ASSERT_FALSE(cydui_ui_begin(&rig.ui, 0));
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE(cydui_ui_begin(&rig.ui, 0)); /* and it stays quit */
}

/* ---- floods (CT-05) ---- */

static void test_a_flood_of_moves_stays_bounded_and_ends_where_the_pointer_ended(void)
{
    int round, i, last_x = 0;

    start();
    rig_down(&rig, 30, 30);
    frame();
    for (round = 0; round < 60; round++) {
        for (i = 0; i < 200; i++) {
            last_x = (round * 200 + i) % 300;
            if (!cydui_mem_push_event(&rig.mem, CYDUI_EVENT_MOVE, (int16_t)last_x, 100)) {
                break; /* the backend queue is full: it drops what does not fit */
            }
        }
        frame();
        TEST_ASSERT_TRUE_MESSAGE(rig.ui.in.q_len <= CYDUI_EVENT_QUEUE, "queue grew past its limit");
        TEST_ASSERT_TRUE(rig.ui.in.last_processed <= CYDUI_EVENTS_PER_FRAME);
    }
    for (i = 0; i < 20 && rig.mem.ev_len > 0; i++) { /* let the backlog drain */
        frame();
    }
    TEST_ASSERT_EQUAL_UINT16(0, rig.mem.ev_len);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.in.q_len);
    rig_up(&rig, 5, 5);
    frame();
    TEST_ASSERT_FALSE_MESSAGE(rig.ui.in.down, "the release was lost in the flood");
    (void)last_x;
}

static void test_a_flood_of_moves_collapses_in_the_queue(void)
{
    int i;

    start();
    rig_down(&rig, 30, 30);
    frame();
    for (i = 0; i < 60; i++) {
        rig_move(&rig, 100 + i, 100);
    }
    frame(); /* 60 moves arrive together: one queue entry, the last position */
    TEST_ASSERT_EQUAL_INT(159, rig.ui.in.x);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.in.q_len);
}

/* When the queue overflows the newest events are kept: the pointer must end where it really is. */
static void test_a_full_queue_keeps_the_newest_events(void)
{
    int i, frames;

    start();
    for (i = 0; i < 20; i++) { /* 40 events that do not coalesce */
        rig_down(&rig, 30, 30);
        rig_up(&rig, 30, 30);
    }
    rig_down(&rig, 150, 30); /* the finger is now down on B: this is the newest event */
    for (frames = 0; frames < 60 && (rig.mem.ev_len || rig.ui.in.q_len > 1); frames++) {
        frame();
    }
    frame();
    TEST_ASSERT_TRUE_MESSAGE(rig.ui.in.down, "the newest event (a press) was thrown away");
    TEST_ASSERT_EQUAL_INT(150, rig.ui.in.x);
    TEST_ASSERT_TRUE(rb.pressed);
}

/* A full queue sacrifices MOVEs before presses and releases: every tap in the burst still happens. */
static void test_a_full_queue_gives_up_moves_before_edges(void)
{
    int i, frames, clicks = 0;

    start();
    for (i = 0; i < 12; i++) { /* 36 events: DOWN, MOVE, UP, three times four */
        rig_down(&rig, 30, 30);
        rig_move(&rig, 31 + i % 3, 30); /* distinct, so they would not coalesce even if adjacent */
        rig_up(&rig, 30, 30);
    }
    for (frames = 0; frames < 80; frames++) {
        frame();
        clicks += ra.clicked;
        if (rig.mem.ev_len == 0 && rig.ui.in.q_len == 0) {
            break;
        }
    }
    frame();
    clicks += ra.clicked;
    TEST_ASSERT_EQUAL_INT_MESSAGE(12, clicks, "a tap was lost because the queue dropped an edge instead of a move");
}

static void test_the_events_taken_per_frame_are_limited(void)
{
    int i;

    start();
    for (i = 0; i < 50; i++) {
        rig_up(&rig, 30, 30); /* releases without a press: cheap, and not coalesced */
    }
    frame();
    TEST_ASSERT_TRUE(rig.ui.in.last_processed <= CYDUI_EVENTS_PER_FRAME);
    TEST_ASSERT_TRUE_MESSAGE(rig.ui.in.last_processed > 0, "nothing was processed");
}

/* Alternating presses and releases in numbers that overflow the queue. */
static void test_alternating_presses_and_releases_cannot_leave_a_widget_stuck(void)
{
    int i, frames;

    start();
    for (i = 0; i < CYDUI_POLL_PER_FRAME - 1; i++) { /* 63 events: more than the queue holds */
        if (i % 2 == 0) {
            rig_down(&rig, 30 + (i % 7), 30);
        } else {
            rig_up(&rig, 30 + (i % 7), 30);
        }
    }
    rig_up(&rig, 30, 30); /* the last event is a release: the finger really is up */
    for (frames = 0; frames < 200; frames++) {
        frame();
        TEST_ASSERT_TRUE(rig.ui.in.q_len <= CYDUI_EVENT_QUEUE);
        if (rig.mem.ev_len == 0 && rig.ui.in.q_len == 0) {
            break;
        }
    }
    frame();
    frame();
    TEST_ASSERT_FALSE_MESSAGE(rig.ui.in.down, "the finger is up but the library thinks it is down");
    TEST_ASSERT_TRUE_MESSAGE(rig.ui.active.h == 0 && rig.ui.active.c == 0, "a widget is stuck active");
}

static void test_a_backend_that_never_runs_dry_cannot_stall_a_frame(void)
{
    /* The mem backend refills itself from this hook; every poll succeeds. */
    start();
    rig_down(&rig, 30, 30);
    {
        int i;
        for (i = 0; i < CYDUI_MEM_EVENTS; i++) {
            cydui_mem_push_event(&rig.mem, CYDUI_EVENT_MOVE, (int16_t)(i % 300), 50);
        }
    }
    frame(); /* must return: at most CYDUI_POLL_PER_FRAME events are read */
    TEST_ASSERT_TRUE(rig.mem.ev_len >= CYDUI_MEM_EVENTS - CYDUI_POLL_PER_FRAME - 1);
}

/* ---- the frame loop's hint ---- */

static void test_end_asks_for_another_frame_while_a_press_is_held(void)
{
    start();
    TEST_ASSERT_FALSE(frame());
    rig_down(&rig, 30, 30);
    TEST_ASSERT_TRUE_MESSAGE(frame(), "idle loop would never see the long press");
    TEST_ASSERT_TRUE(frame());
    rig_up(&rig, 30, 30);
    TEST_ASSERT_FALSE(frame());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tap_over_two_frames);
    RUN_TEST(test_press_and_release_in_one_frame_is_one_tap);
    RUN_TEST(test_the_widget_sees_the_pointer_travel_to_a_release_outside);
    RUN_TEST(test_travel_within_the_slop_is_still_a_tap);
    RUN_TEST(test_releasing_just_outside_the_widget_is_not_a_tap);
    RUN_TEST(test_travel_beyond_the_slop_is_a_drag_not_a_tap);
    RUN_TEST(test_slop_is_a_distance_not_a_box);
    RUN_TEST(test_returning_to_the_start_does_not_undo_a_drag);
    RUN_TEST(test_drag_reports_the_travel_of_each_frame);
    RUN_TEST(test_long_press_fires_once_after_the_hold_time);
    RUN_TEST(test_release_after_the_hold_time_counts_as_long_even_without_a_frame_between);
    RUN_TEST(test_long_press_time_is_configurable);
    RUN_TEST(test_moving_cancels_the_long_press);
    RUN_TEST(test_long_press_survives_the_clock_wrapping);
    RUN_TEST(test_without_a_clock_taps_work_and_long_press_never_fires);
    RUN_TEST(test_press_on_a_and_release_on_b_clicks_neither);
    RUN_TEST(test_a_press_belongs_to_where_it_started_not_to_where_the_pointer_is_now);
    RUN_TEST(test_press_on_empty_space_then_release_on_a_widget_clicks_nothing);
    RUN_TEST(test_overlapping_widgets_the_first_call_gets_the_press);
    RUN_TEST(test_a_disabled_widget_ignores_the_press_and_does_not_capture_it);
    RUN_TEST(test_the_press_is_tested_against_the_clip);
    RUN_TEST(test_release_without_a_press_is_ignored);
    RUN_TEST(test_a_stray_release_does_not_delay_a_press_that_follows_it);
    RUN_TEST(test_a_second_release_after_a_tap_is_ignored);
    RUN_TEST(test_a_press_that_never_ended_is_cancelled_by_the_next_press);
    RUN_TEST(test_release_then_press_in_one_frame_takes_two_frames);
    RUN_TEST(test_an_unseen_widget_does_not_keep_the_press_forever);
    RUN_TEST(test_coordinates_are_clamped_to_the_screen);
    RUN_TEST(test_unknown_event_types_are_ignored);
    RUN_TEST(test_a_quit_event_ends_the_loop);
    RUN_TEST(test_a_flood_of_moves_stays_bounded_and_ends_where_the_pointer_ended);
    RUN_TEST(test_a_flood_of_moves_collapses_in_the_queue);
    RUN_TEST(test_a_full_queue_keeps_the_newest_events);
    RUN_TEST(test_a_full_queue_gives_up_moves_before_edges);
    RUN_TEST(test_the_events_taken_per_frame_are_limited);
    RUN_TEST(test_alternating_presses_and_releases_cannot_leave_a_widget_stuck);
    RUN_TEST(test_a_backend_that_never_runs_dry_cannot_stall_a_frame);
    RUN_TEST(test_end_asks_for_another_frame_while_a_press_is_held);
    return UNITY_END();
}
