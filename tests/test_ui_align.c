/*
 * Alignment and cards (FR-25, FR-52). Everything is worked out by hand with the default theme:
 * margin and gaps of 8, card padding (style.pad_x) of 10, a 320x240 screen, so the root column
 * starts at (8,8) and is 304 x 224. Children are plain rectangles from cydui_alloc_rect, so no font
 * metric gets in the way.
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;
static bool more; /* what cydui_ui_end said */

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
}

static void assert_rect(int x, int y, int w, int h, cydui_rect r)
{
    char msg[80];
    snprintf(msg, sizeof(msg), "got (%d,%d,%d,%d) want (%d,%d,%d,%d)", r.x, r.y, r.w, r.h, x, y, w, h);
    TEST_ASSERT_TRUE_MESSAGE(r.x == x && r.y == y && r.w == w && r.h == h, msg);
}

/* ---- cross alignment ---- */

static cydui_rect a, b;
static cydui_place cross_place = CYDUI_PLACE_CENTER;
static int b_height = 40;

static void row_frame(void)
{
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_row(&rig.ui);
    cydui_set_cross_align(&rig.ui, cross_place);
    a = cydui_alloc_rect(&rig.ui, 10, 20);
    b = cydui_alloc_rect(&rig.ui, 10, b_height);
    cydui_end_row(&rig.ui);
    more = cydui_ui_end(&rig.ui);
}

static void test_a_row_centers_its_children_on_the_tallest_one_after_a_frame(void)
{
    int i;

    start();
    cross_place = CYDUI_PLACE_CENTER;
    b_height = 40;
    row_frame(); /* nothing is known yet: everybody at the top */
    assert_rect(8, 8, 10, 20, a);
    assert_rect(26, 8, 10, 40, b);
    TEST_ASSERT_TRUE_MESSAGE(more, "the first frame must ask for another");
    row_frame();
    assert_rect(8, 18, 10, 20, a); /* 8 + (40 - 20) / 2 */
    assert_rect(26, 8, 10, 40, b);
    TEST_ASSERT_FALSE(more);
    for (i = 0; i < 10; i++) { /* the measure does not feed on the shift */
        row_frame();
        assert_rect(8, 18, 10, 20, a);
        assert_rect(26, 8, 10, 40, b);
        TEST_ASSERT_FALSE(more);
    }
}

static void test_end_puts_children_at_the_bottom_of_the_row(void)
{
    start();
    cross_place = CYDUI_PLACE_END;
    b_height = 40;
    row_frame();
    row_frame();
    assert_rect(8, 28, 10, 20, a); /* 8 + (40 - 20) */
    assert_rect(26, 8, 10, 40, b);
    TEST_ASSERT_FALSE(more);
}

static void test_start_changes_nothing_and_asks_for_no_frames(void)
{
    start();
    cross_place = CYDUI_PLACE_START;
    row_frame();
    assert_rect(8, 8, 10, 20, a);
    TEST_ASSERT_FALSE(more);
}

static void test_a_change_of_content_is_followed_one_frame_later(void)
{
    start();
    cross_place = CYDUI_PLACE_CENTER;
    b_height = 40;
    row_frame();
    row_frame();
    b_height = 30; /* the tallest got shorter */
    row_frame();
    assert_rect(8, 18, 10, 20, a); /* still placed by the old measure */
    TEST_ASSERT_TRUE(more);
    row_frame();
    assert_rect(8, 13, 10, 20, a); /* 8 + (30 - 20) / 2 */
    TEST_ASSERT_FALSE(more);
}

static void test_the_root_column_centers_across_the_whole_width_at_once(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
    a = cydui_alloc_rect(&rig.ui, 100, 10);
    b = cydui_alloc_rect(&rig.ui, 50, 10);
    more = cydui_ui_end(&rig.ui);
    assert_rect(8 + 102, 8, 100, 10, a); /* (304 - 100) / 2: the root's width is known in the first frame */
    assert_rect(8 + 127, 26, 50, 10, b); /* 8 + 10 + the gap of 8 */
}

static void test_a_column_with_a_width_centers_in_it(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_width(&rig.ui, 200);
    cydui_begin_column(&rig.ui);
    cydui_set_cross_align(&rig.ui, CYDUI_PLACE_END);
    a = cydui_alloc_rect(&rig.ui, 60, 10);
    cydui_end_column(&rig.ui);
    cydui_ui_end(&rig.ui);
    assert_rect(8 + 140, 8, 60, 10, a); /* at the right end of its 200 px, at once */
}

/* ---- main alignment ---- */

static cydui_place main_place = CYDUI_PLACE_CENTER;
static bool give_width = true;

static void main_row_frame(void)
{
    cydui_ui_begin(&rig.ui, 0);
    if (give_width) {
        cydui_next_width(&rig.ui, 200);
    }
    cydui_begin_row(&rig.ui);
    cydui_set_main_align(&rig.ui, main_place);
    a = cydui_alloc_rect(&rig.ui, 20, 10);
    b = cydui_alloc_rect(&rig.ui, 20, 10);
    cydui_end_row(&rig.ui);
    more = cydui_ui_end(&rig.ui);
}

static void test_a_group_is_centered_in_the_width_the_container_was_given(void)
{
    start();
    main_place = CYDUI_PLACE_CENTER;
    give_width = true;
    main_row_frame();
    assert_rect(8, 8, 20, 10, a); /* the first frame does not know the size of the group */
    main_row_frame();
    assert_rect(8 + 76, 8, 20, 10, a); /* (200 - 48) / 2 = 76; the group is 20 + 8 + 20 = 48 wide */
    assert_rect(8 + 104, 8, 20, 10, b);
    main_row_frame();
    TEST_ASSERT_FALSE(more);
}

static void test_a_group_goes_to_the_end(void)
{
    start();
    main_place = CYDUI_PLACE_END;
    give_width = true;
    main_row_frame();
    main_row_frame();
    assert_rect(8 + 152, 8, 20, 10, a); /* 200 - 48 */
    assert_rect(8 + 180, 8, 20, 10, b);
}

static void test_without_a_width_the_group_has_no_room_to_move(void)
{
    start();
    main_place = CYDUI_PLACE_CENTER;
    give_width = false;
    main_row_frame();
    main_row_frame();
    assert_rect(8, 8, 20, 10, a);
    assert_rect(28 + 8, 8, 20, 10, b);
}

static void test_the_root_column_centers_its_group_vertically(void)
{
    int i;

    start();
    for (i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_set_main_align(&rig.ui, CYDUI_PLACE_CENTER);
        a = cydui_alloc_rect(&rig.ui, 100, 10);
        b = cydui_alloc_rect(&rig.ui, 100, 10);
        more = cydui_ui_end(&rig.ui);
        if (i == 0) {
            assert_rect(8, 8, 100, 10, a); /* not known yet */
        }
    }
    assert_rect(8, 8 + 98, 100, 10, a); /* the group is 28 tall in 224: (224 - 28) / 2 = 98 */
    assert_rect(8, 8 + 116, 100, 10, b);
    TEST_ASSERT_FALSE(more);
}

static void test_fill_inside_a_placed_container_leaves_nothing_to_move(void)
{
    int i;

    start();
    for (i = 0; i < 4; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_next_width(&rig.ui, 200);
        cydui_begin_row(&rig.ui);
        cydui_set_main_align(&rig.ui, CYDUI_PLACE_CENTER);
        a = cydui_alloc_rect(&rig.ui, 20, 10);
        cydui_next_fill(&rig.ui);
        b = cydui_alloc_rect(&rig.ui, 20, 10);
        cydui_end_row(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    assert_rect(8, 8, 20, 10, a);
    assert_rect(8 + 28, 8, 172, 10, b); /* 200 - 20 - 8 */
    TEST_ASSERT_FALSE(more);
}

static void test_a_grid_ignores_alignment(void)
{
    int i;

    start();
    for (i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_begin_grid(&rig.ui, 2);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_set_main_align(&rig.ui, CYDUI_PLACE_END);
        a = cydui_alloc_rect(&rig.ui, 10, 10);
        b = cydui_alloc_rect(&rig.ui, 10, 30);
        cydui_end_grid(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    assert_rect(8, 8, 10, 10, a);
    assert_rect(8 + 148 + 8, 8, 10, 30, b); /* cells of (304 - 8) / 2 = 148 */
    TEST_ASSERT_FALSE(more);
}

static void test_a_bad_place_value_means_start(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_set_cross_align(&rig.ui, (cydui_place)77);
    cydui_set_main_align(&rig.ui, (cydui_place)-1);
    a = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_ui_end(&rig.ui);
    assert_rect(8, 8, 10, 10, a);
}

/* ---- containers that are placed themselves ---- */

static void test_a_row_inside_a_centered_column_is_centered_as_a_whole(void)
{
    int i;
    cydui_rect c1, c2;

    start();
    for (i = 0; i < 4; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_begin_row(&rig.ui);
        c1 = cydui_alloc_rect(&rig.ui, 40, 10);
        c2 = cydui_alloc_rect(&rig.ui, 40, 10);
        cydui_end_row(&rig.ui);
        more = cydui_ui_end(&rig.ui);
        if (i == 0) {
            assert_rect(8, 8, 40, 10, c1); /* the row's size is unknown: it stays where it is */
            TEST_ASSERT_TRUE(more);
        }
    }
    assert_rect(8 + 108, 8, 40, 10, c1); /* the row is 88 wide: (304 - 88) / 2 = 108 */
    assert_rect(8 + 156, 8, 40, 10, c2);
    TEST_ASSERT_FALSE(more);
}

static void test_two_levels_settle_in_at_most_one_frame_per_level(void)
{
    int i;
    cydui_rect leaf;

    start();
    for (i = 0; i < 4; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_begin_column(&rig.ui);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_begin_row(&rig.ui);
        leaf = cydui_alloc_rect(&rig.ui, 60, 10);
        cydui_end_row(&rig.ui);
        cydui_end_column(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    assert_rect(8 + 122, 8, 60, 10, leaf); /* the column is 60 wide, centered in 304: 8 + 122 */
    TEST_ASSERT_FALSE(more);
}


static void test_a_container_with_only_one_size_given_waits_for_its_measure(void)
{
    int i;
    cydui_rect c1;

    start();
    for (i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_next_width(&rig.ui, 100); /* the height is not given: it is the content's */
        cydui_begin_row(&rig.ui);
        c1 = cydui_alloc_rect(&rig.ui, 40, 10);
        cydui_end_row(&rig.ui);
        more = cydui_ui_end(&rig.ui);
        if (i == 0) {
            assert_rect(8, 8, 40, 10, c1); /* the first frame leaves it where it is */
        }
    }
    assert_rect(8 + 102, 8, 40, 10, c1); /* (304 - 100) / 2 */
    TEST_ASSERT_FALSE(more);
}

static void test_a_change_of_the_cross_size_alone_asks_for_another_frame(void)
{
    int i;

    start();
    for (i = 0; i < 3; i++) { /* a column: its width is the cross size and its height does not change */
        cydui_ui_begin(&rig.ui, 0);
        cydui_next_width(&rig.ui, 200);
        cydui_begin_column(&rig.ui);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_END);
        a = cydui_alloc_rect(&rig.ui, 20, 10);
        cydui_end_column(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    TEST_ASSERT_FALSE(more);
    assert_rect(8 + 180, 8, 20, 10, a);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_width(&rig.ui, 200);
    cydui_begin_column(&rig.ui);
    cydui_set_cross_align(&rig.ui, CYDUI_PLACE_END);
    a = cydui_alloc_rect(&rig.ui, 30, 10); /* wider, same height */
    cydui_end_column(&rig.ui);
    more = cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(more);
    assert_rect(8 + 170, 8, 30, 10, a); /* the container has a width, so the child lands right at once... */
    TEST_ASSERT_TRUE(more); /* ...but whoever depends on the measure has to hear that it changed */
}

/* ---- negative positions ---- */

static void test_children_scrolled_above_the_top_keep_their_negative_positions(void)
{
    cydui_rect r;
    int i;

    start();
    for (i = 0; i < 3; i++) {
        rig_advance(&rig, 16);
        cydui_ui_begin(&rig.ui, 0);
        CYDUI_SCROLL(&rig.ui, "s", 50) { cydui_spacer(&rig.ui, 400); }
        cydui_ui_end(&rig.ui);
    }
    cydui_scroll_to(&rig.ui, "s", 100);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "s", 50) {
        r = cydui_alloc_rect(&rig.ui, 10, 10);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_spacer(&rig.ui, 400);
    }
    cydui_ui_end(&rig.ui);
    assert_rect(8, 8 - 100, 10, 10, r);
}


static void test_several_children_scrolled_above_the_top_follow_each_other(void)
{
    cydui_rect r1, r2, r3, c1, c2;
    int i;

    start();
    for (i = 0; i < 4; i++) {
        rig_advance(&rig, 16);
        if (i == 3) {
            cydui_scroll_to(&rig.ui, "s", 100);
        }
        cydui_ui_begin(&rig.ui, 0);
        CYDUI_SCROLL(&rig.ui, "s", 50) {
            r1 = cydui_alloc_rect(&rig.ui, 10, 10);
            r2 = cydui_alloc_rect(&rig.ui, 10, 10);
            cydui_begin_row(&rig.ui);
            c1 = cydui_alloc_rect(&rig.ui, 10, 10);
            c2 = cydui_alloc_rect(&rig.ui, 10, 10);
            cydui_end_row(&rig.ui);
            r3 = cydui_alloc_rect(&rig.ui, 10, 10);
            cydui_spacer(&rig.ui, 400);
        }
        cydui_ui_end(&rig.ui);
    }
    assert_rect(8, 8 - 100, 10, 10, r1);
    assert_rect(8, 8 - 100 + 18, 10, 10, r2); /* a gap of 8 after each 10 px child */
    assert_rect(8, 8 - 100 + 36, 10, 10, c1); /* the row, still above the top */
    assert_rect(26, 8 - 100 + 36, 10, 10, c2);
    assert_rect(8, 8 - 100 + 54, 10, 10, r3);
}

/* ---- cards ---- */

static cydui_rect panel_rects[4];
static cydui_rect last_panel;
static int panel_calls;

static void record_panel(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *pi)
{
    (void)t;
    last_panel = pi->rect;
    if (panel_calls < 4) {
        panel_rects[panel_calls] = pi->rect;
    }
    panel_calls++;
    cydui_draw_rect(&ui->ctx, pi->rect, CYDUI_RGB(1, 2, 3), 255); /* #1 of the panel */
    cydui_draw_rect(&ui->ctx, pi->rect, CYDUI_RGB(4, 5, 6), 255); /* #2 */
}

static void start_with_recording_panel(void)
{
    start();
    rig.theme.paint[CYDUI_WIDGET_PANEL] = record_panel;
    panel_calls = 0;
}

static void test_a_card_is_as_big_as_its_content_plus_padding_in_the_first_frame(void)
{
    cydui_rect child, next;

    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_card(&rig.ui);
    child = cydui_alloc_rect(&rig.ui, 100, 20);
    cydui_end_card(&rig.ui);
    next = cydui_alloc_rect(&rig.ui, 10, 10);
    more = cydui_ui_end(&rig.ui);
    assert_rect(18, 18, 100, 20, child);          /* 8 + 10 padding */
    TEST_ASSERT_EQUAL_INT(1, panel_calls);
    assert_rect(8, 8, 120, 40, panel_rects[0]);   /* 100 + 20, 20 + 20 */
    assert_rect(8, 8 + 40 + 8, 10, 10, next);     /* the card took 120 x 40 in its parent */
    TEST_ASSERT_FALSE(more);                      /* nothing needed another frame */
}

static void test_the_panel_is_behind_the_children_and_the_order_is_kept(void)
{
    const cydui_ctx *c;

    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    cydui_draw_rect(&rig.ui.ctx, (cydui_rect){0, 0, 5, 5}, CYDUI_RGB(9, 0, 0), 255); /* before the card */
    cydui_begin_card(&rig.ui);
    cydui_draw_rect(&rig.ui.ctx, (cydui_rect){20, 20, 5, 5}, CYDUI_RGB(0, 9, 0), 255); /* children */
    cydui_draw_rect(&rig.ui.ctx, (cydui_rect){20, 30, 5, 5}, CYDUI_RGB(0, 0, 9), 255);
    cydui_alloc_rect(&rig.ui, 50, 50);
    cydui_end_card(&rig.ui);
    cydui_draw_rect(&rig.ui.ctx, (cydui_rect){60, 60, 5, 5}, CYDUI_RGB(9, 9, 0), 255); /* after */
    cydui_ui_end(&rig.ui);
    c = &rig.ui.ctx;
    TEST_ASSERT_EQUAL_UINT32(6, cydui_cmd_count(c));
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(9, 0, 0), c->cmds[0].u.rect.color);
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(1, 2, 3), c->cmds[1].u.rect.color); /* panel, in its own order */
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(4, 5, 6), c->cmds[2].u.rect.color);
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(0, 9, 0), c->cmds[3].u.rect.color); /* then the children, in theirs */
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(0, 0, 9), c->cmds[4].u.rect.color);
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(9, 9, 0), c->cmds[5].u.rect.color);
}

static void test_an_empty_card_is_just_its_padding(void)
{
    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_CARD(&rig.ui) { }
    cydui_ui_end(&rig.ui);
    assert_rect(8, 8, 20, 20, panel_rects[0]);
    TEST_ASSERT_EQUAL_UINT32(2, cydui_cmd_count(&rig.ui.ctx));
}

static void test_cards_nest_and_each_one_wraps_what_is_inside(void)
{
    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_CARD(&rig.ui) {
        cydui_alloc_rect(&rig.ui, 30, 10);
        CYDUI_CARD(&rig.ui) {
            cydui_alloc_rect(&rig.ui, 50, 20);
        }
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(2, panel_calls);
    assert_rect(8 + 10, 8 + 10 + 10 + 8, 70, 40, panel_rects[0]); /* the inner one ends first */
    assert_rect(8, 8, 90, 10 + 8 + 40 + 20, panel_rects[1]);
    {
        const cydui_ctx *c = &rig.ui.ctx;
        /* outer panel, then the inner one, then nothing else (the children drew no commands) */
        TEST_ASSERT_EQUAL_UINT32(4, cydui_cmd_count(c));
        TEST_ASSERT_EQUAL_INT(8, c->cmds[0].rect.x); /* the outer panel is first */
        TEST_ASSERT_EQUAL_INT(18, c->cmds[2].rect.x);
    }
}

static void test_a_card_can_be_given_a_size(void)
{
    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_width(&rig.ui, 200);
    cydui_next_height(&rig.ui, 80);
    CYDUI_CARD(&rig.ui) { cydui_alloc_rect(&rig.ui, 10, 10); }
    cydui_ui_end(&rig.ui);
    assert_rect(8, 8, 200, 80, panel_rects[0]);
}

static void test_a_card_can_place_its_children_too(void)
{
    cydui_rect child;
    int i;

    start_with_recording_panel();
    for (i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_next_width(&rig.ui, 200);
        cydui_begin_card(&rig.ui);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        child = cydui_alloc_rect(&rig.ui, 100, 20);
        cydui_end_card(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    assert_rect(8 + 10 + 40, 18, 100, 20, child); /* (200 - 20 - 100) / 2 = 40 into the padded area */
    TEST_ASSERT_FALSE(more);
}

static void test_a_card_in_a_scrolled_area_is_painted_where_it_is(void)
{
    int i;

    start_with_recording_panel();
    for (i = 0; i < 4; i++) {
        rig_advance(&rig, 16);
        if (i == 2) {
            cydui_scroll_to(&rig.ui, "s", 30);
        }
        cydui_ui_begin(&rig.ui, 0);
        CYDUI_SCROLL(&rig.ui, "s", 60) {
            CYDUI_CARD(&rig.ui) { cydui_alloc_rect(&rig.ui, 100, 100); }
        }
        cydui_ui_end(&rig.ui);
    }
    assert_rect(8, 8 - 30, 120, 120, last_panel); /* the last frame: scrolled by 30 */
}

static void test_a_full_command_list_loses_the_panel_and_keeps_the_rest_in_order(void)
{
    int i;

    start();
    cydui_ui_begin(&rig.ui, 0);
    for (i = 0; i < 509; i++) {
        cydui_draw_rect(&rig.ui.ctx, (cydui_rect){0, (int16_t)(i % 200), 4, 4}, CYDUI_RGB(1, 1, 1), 255);
    }
    cydui_ui_clear_errors(&rig.ui);
    cydui_begin_card(&rig.ui);
    cydui_draw_rect(&rig.ui.ctx, (cydui_rect){20, 20, 5, 5}, CYDUI_RGB(0, 9, 0), 255);
    cydui_draw_rect(&rig.ui.ctx, (cydui_rect){20, 30, 5, 5}, CYDUI_RGB(0, 0, 9), 255);
    cydui_draw_rect(&rig.ui.ctx, (cydui_rect){20, 40, 5, 5}, CYDUI_RGB(9, 9, 9), 255); /* 512th: fits */
    cydui_end_card(&rig.ui); /* the panel needs 2 more: they do not fit */
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_CMD_OVERFLOW);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(512, cydui_cmd_count(&rig.ui.ctx));
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(0, 9, 0), rig.ui.ctx.cmds[509].u.rect.color); /* children untouched */
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(0, 0, 9), rig.ui.ctx.cmds[510].u.rect.color);
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(9, 9, 9), rig.ui.ctx.cmds[511].u.rect.color);
}

static void test_a_panel_that_only_partly_fits_is_still_ordered(void)
{
    int i;

    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    for (i = 0; i < 508; i++) {
        cydui_draw_rect(&rig.ui.ctx, (cydui_rect){0, (int16_t)(i % 200), 4, 4}, CYDUI_RGB(1, 1, 1), 255);
    }
    cydui_begin_card(&rig.ui);
    cydui_draw_rect(&rig.ui.ctx, (cydui_rect){20, 20, 5, 5}, CYDUI_RGB(0, 9, 0), 255); /* 509th */
    cydui_draw_rect(&rig.ui.ctx, (cydui_rect){20, 30, 5, 5}, CYDUI_RGB(0, 0, 9), 255); /* 510th */
    cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_end_card(&rig.ui); /* two panel commands: 511th and 512th, exactly the room left */
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(512, cydui_cmd_count(&rig.ui.ctx));
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(1, 2, 3), rig.ui.ctx.cmds[508].u.rect.color);
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(4, 5, 6), rig.ui.ctx.cmds[509].u.rect.color);
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(0, 9, 0), rig.ui.ctx.cmds[510].u.rect.color);
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(0, 0, 9), rig.ui.ctx.cmds[511].u.rect.color);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_card_calls_that_do_not_match_are_survived(void)
{
    int i;

    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    cydui_end_card(&rig.ui); /* without a begin */
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
    cydui_ui_clear_errors(&rig.ui);

    cydui_begin_column(&rig.ui);
    cydui_end_card(&rig.ui); /* a plain column closed as a card */
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    TEST_ASSERT_EQUAL_INT(0, panel_calls);

    cydui_begin_card(&rig.ui);
    cydui_end_column(&rig.ui); /* a card closed as a column: no panel */
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);

    for (i = 0; i < CYDUI_MAX_LAYOUT_DEPTH + 2; i++) { /* too deep */
        cydui_begin_card(&rig.ui);
    }
    for (i = 0; i < CYDUI_MAX_LAYOUT_DEPTH + 2; i++) {
        cydui_end_card(&rig.ui);
    }
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    cydui_ui_end(&rig.ui);
}

static void test_a_card_the_application_forgot_to_close_is_still_painted(void)
{
    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_card(&rig.ui);
    cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(1, panel_calls);
    assert_rect(8, 8, 30, 30, panel_rects[0]);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
}

static void test_a_card_with_a_height_places_its_group_inside_the_padding(void)
{
    cydui_rect child;
    int i;

    start_with_recording_panel();
    for (i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_next_height(&rig.ui, 100);
        cydui_begin_card(&rig.ui);
        cydui_set_main_align(&rig.ui, CYDUI_PLACE_CENTER);
        child = cydui_alloc_rect(&rig.ui, 10, 20);
        cydui_end_card(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    /* room inside the padding: 100 - 2 x 10 = 80; the group is 20 tall: (80 - 20) / 2 = 30 below y = 8 + 10 */
    assert_rect(18, 18 + 30, 10, 20, child);
    TEST_ASSERT_FALSE(more);
}

static void test_a_centered_column_centers_a_card_by_its_padded_size(void)
{
    cydui_rect child;
    int i;

    start_with_recording_panel();
    for (i = 0; i < 4; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_begin_card(&rig.ui);
        child = cydui_alloc_rect(&rig.ui, 100, 20);
        cydui_end_card(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    /* the card is 100 + 2 x 10 = 120 wide: (304 - 120) / 2 = 92 from x = 8, and the child 10 further in */
    assert_rect(8 + 92, 8, 120, 40, last_panel);
    assert_rect(8 + 92 + 10, 8 + 10, 100, 20, child);
    TEST_ASSERT_FALSE(more);
}

static void test_a_container_that_is_moved_takes_its_children_with_it(void)
{
    cydui_rect c1, c2;
    int i;

    start();
    for (i = 0; i < 4; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_begin_row(&rig.ui);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_alloc_rect(&rig.ui, 10, 80);
        cydui_begin_column(&rig.ui);
        c1 = cydui_alloc_rect(&rig.ui, 10, 20);
        c2 = cydui_alloc_rect(&rig.ui, 10, 20);
        cydui_end_column(&rig.ui);
        cydui_end_row(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    /* the column is 10 x 48 in a row 80 tall: (80 - 48) / 2 = 16 down; its children follow, a gap of 8 apart */
    assert_rect(8 + 10 + 8, 8 + 16, 10, 20, c1);
    assert_rect(8 + 10 + 8, 8 + 16 + 28, 10, 20, c2);
    TEST_ASSERT_FALSE(more);
}

static void test_a_padded_container_with_a_height_gives_its_fill_child_the_room_inside_the_padding(void)
{
    cydui_rect r;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_height(&rig.ui, 100);
    cydui_begin_padding(&rig.ui, 10);
    cydui_next_fill(&rig.ui);
    r = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_end_padding(&rig.ui);
    cydui_ui_end(&rig.ui);
    assert_rect(18, 18, 10, 80, r); /* 100 less 10 above and 10 below: at once, in the first frame */
}

static void test_a_container_that_asked_to_fill_counts_among_the_ones_that_share(void)
{
    cydui_rect second;
    int i;

    start();
    for (i = 0; i < 4; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_next_width(&rig.ui, 200);
        cydui_begin_row(&rig.ui);
        cydui_next_fill(&rig.ui);
        cydui_begin_column(&rig.ui);
        cydui_alloc_rect(&rig.ui, 10, 10);
        cydui_end_column(&rig.ui);
        cydui_next_fill(&rig.ui);
        second = cydui_alloc_rect(&rig.ui, 10, 10);
        cydui_end_row(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    /* 200 less the gap of 8, shared by two: 96 each; the second starts after the first and the gap */
    assert_rect(8 + 96 + 8, 8, 96, 10, second);
    TEST_ASSERT_FALSE(more);
}

static void test_a_width_or_height_of_zero_is_a_size_like_any_other(void)
{
    cydui_rect next, below;

    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_row(&rig.ui);
    cydui_next_width(&rig.ui, 0);
    cydui_begin_row(&rig.ui);
    cydui_alloc_rect(&rig.ui, 50, 10);
    cydui_end_row(&rig.ui);
    next = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_end_row(&rig.ui);
    cydui_next_height(&rig.ui, 0);
    cydui_begin_column(&rig.ui);
    cydui_alloc_rect(&rig.ui, 10, 50);
    cydui_end_column(&rig.ui);
    below = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_next_width(&rig.ui, 0);
    CYDUI_CARD(&rig.ui) { cydui_alloc_rect(&rig.ui, 30, 10); }
    cydui_next_height(&rig.ui, 0);
    CYDUI_CARD(&rig.ui) { cydui_alloc_rect(&rig.ui, 30, 10); }
    cydui_ui_end(&rig.ui);
    assert_rect(8 + 0 + 8, 8, 10, 10, next); /* the row that was given 0 wide took 0, whatever its child */
    TEST_ASSERT_EQUAL_INT(8 + 10 + 8 + 0 + 8, below.y); /* the row is 10 tall, a gap, the column given 0 took 0, a gap */
    TEST_ASSERT_EQUAL_INT(0, panel_rects[0].w);
    TEST_ASSERT_EQUAL_INT(0, panel_rects[1].h);
}

static void test_a_container_given_no_width_has_no_room_inside(void)
{
    cydui_rect inside, tall;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_width(&rig.ui, 0);
    cydui_begin_column(&rig.ui);
    cydui_next_stretch(&rig.ui);
    inside = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_end_column(&rig.ui);
    cydui_next_height(&rig.ui, 0);
    cydui_begin_row(&rig.ui);
    cydui_next_fill(&rig.ui);
    tall = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_end_row(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(0, inside.w); /* stretched to the room of a column that has none */
    TEST_ASSERT_EQUAL_INT(304, tall.w); /* a row of height 0 still has all its width */
}

static void test_a_card_that_could_not_open_ends_without_closing_the_one_that_is_open(void)
{
    int d;

    start_with_recording_panel();
    cydui_ui_begin(&rig.ui, 0);
    for (d = 0; d < CYDUI_MAX_LAYOUT_DEPTH - 1; d++) {
        cydui_begin_column(&rig.ui);
    }
    cydui_begin_card(&rig.ui); /* the last level there is */
    cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_begin_card(&rig.ui); /* too deep: skipped */
    cydui_ui_clear_errors(&rig.ui);
    cydui_end_card(&rig.ui);   /* the end of the one that was skipped */
    TEST_ASSERT_EQUAL_INT(0, panel_calls);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_EQUAL_UINT8(CYDUI_MAX_LAYOUT_DEPTH, rig.ui.depth);
    cydui_end_card(&rig.ui);
    TEST_ASSERT_EQUAL_INT(1, panel_calls);

    for (d = 0; d < CYDUI_MAX_LAYOUT_DEPTH - 1; d++) { /* a plain column under a skipped card: no complaint */
        cydui_end_column(&rig.ui);
    }
    for (d = 0; d < CYDUI_MAX_LAYOUT_DEPTH; d++) {
        cydui_begin_column(&rig.ui);
    }
    cydui_begin_card(&rig.ui); /* skipped, under a plain column */
    cydui_ui_clear_errors(&rig.ui);
    cydui_end_card(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    for (d = 0; d < CYDUI_MAX_LAYOUT_DEPTH; d++) {
        cydui_end_column(&rig.ui);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
}

static void test_a_list_item_at_the_deepest_level_cannot_open_and_is_survived(void)
{
    int d;

    start();
    cydui_ui_begin(&rig.ui, 0);
    for (d = 0; d < CYDUI_MAX_LAYOUT_DEPTH - 1; d++) {
        cydui_begin_column(&rig.ui);
    }
    cydui_list_begin(&rig.ui, "s", 5, 30, 100, NULL, NULL); /* the last level there is */
    TEST_ASSERT_EQUAL_UINT8(CYDUI_MAX_LAYOUT_DEPTH, rig.ui.depth);
    cydui_ui_clear_errors(&rig.ui);
    cydui_begin_list_item(&rig.ui, 0); /* no room for another level */
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
    TEST_ASSERT_EQUAL_UINT8(CYDUI_MAX_LAYOUT_DEPTH, rig.ui.depth);
    cydui_label(&rig.ui, "x");
    cydui_end_list_item(&rig.ui);
    TEST_ASSERT_EQUAL_UINT8(CYDUI_MAX_LAYOUT_DEPTH, rig.ui.depth);
    cydui_list_end(&rig.ui);
    for (d = 0; d < CYDUI_MAX_LAYOUT_DEPTH - 1; d++) {
        cydui_end_column(&rig.ui);
    }
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    cydui_ui_end(&rig.ui);
}

static void test_the_row_places_a_card_by_its_padded_height(void)
{
    cydui_rect child;
    int i;

    start();
    for (i = 0; i < 4; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_begin_row(&rig.ui);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_alloc_rect(&rig.ui, 10, 80);
        cydui_begin_card(&rig.ui);
        child = cydui_alloc_rect(&rig.ui, 10, 20);
        cydui_end_card(&rig.ui);
        cydui_end_row(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    /* the card is 20 + 2 x 10 = 40 tall in a row 80 tall: 20 down; the child is 10 further in */
    assert_rect(8 + 10 + 8 + 10, 8 + 20 + 10, 10, 20, child);
    TEST_ASSERT_FALSE(more);
}

static void test_a_list_item_is_a_column_of_the_size_of_its_row(void)
{
    cydui_rect r1, r2;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_list_begin(&rig.ui, "s", 5, 40, 100, NULL, NULL);
    cydui_begin_list_item(&rig.ui, 1); /* y = 8 + 40 */
    cydui_next_stretch(&rig.ui);
    r1 = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_next_fill(&rig.ui);
    r2 = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_end_list_item(&rig.ui);
    cydui_list_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    assert_rect(8, 48, 304, 10, r1);        /* stretched: the whole width of the row of the list */
    assert_rect(8, 48 + 10 + 8, 10, 22, r2); /* after a gap of 8; fill: what is left of its 40 px */
}

static void test_a_container_that_fills_takes_the_room_along_the_column(void)
{
    cydui_rect r2;
    int i;

    start();
    for (i = 0; i < 4; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_next_height(&rig.ui, 200);
        cydui_begin_column(&rig.ui);
        cydui_next_fill(&rig.ui);
        cydui_begin_column(&rig.ui);
        cydui_alloc_rect(&rig.ui, 10, 10);
        cydui_end_column(&rig.ui);
        r2 = cydui_alloc_rect(&rig.ui, 10, 20);
        cydui_end_column(&rig.ui);
        more = cydui_ui_end(&rig.ui);
    }
    assert_rect(8, 8 + 172 + 8, 10, 20, r2); /* 200 less the 20 and the gap of 8 that are not for filling */
    TEST_ASSERT_FALSE(more);
}

static void test_a_container_that_stretches_takes_the_room_across(void)
{
    cydui_rect inner, wide;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_height(&rig.ui, 50);
    cydui_begin_row(&rig.ui);
    cydui_next_stretch(&rig.ui); /* as tall as the row */
    cydui_begin_column(&rig.ui);
    cydui_next_fill(&rig.ui);
    inner = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_end_column(&rig.ui);
    cydui_end_row(&rig.ui);
    cydui_next_stretch(&rig.ui); /* as wide as the column that holds it */
    cydui_begin_row(&rig.ui);
    cydui_next_fill(&rig.ui);
    wide = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_end_row(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(50, inner.h);
    TEST_ASSERT_EQUAL_INT(304, wide.w);
}

static void test_a_grid_has_between_one_and_sixty_four_columns(void)
{
    cydui_rect r[66];
    int i;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_grid(&rig.ui, 1000);
    for (i = 0; i < 66; i++) {
        r[i] = cydui_alloc_rect(&rig.ui, 1, 1);
    }
    cydui_end_grid(&rig.ui);
    TEST_ASSERT_EQUAL_INT(r[0].y, r[63].y); /* 64 in the first row */
    TEST_ASSERT_TRUE(r[64].y > r[0].y);     /* the 65th starts the second */
    cydui_begin_grid(&rig.ui, 0);
    r[0] = cydui_alloc_rect(&rig.ui, 1, 1);
    r[1] = cydui_alloc_rect(&rig.ui, 1, 1);
    cydui_end_grid(&rig.ui);
    TEST_ASSERT_TRUE(r[1].y > r[0].y); /* none or fewer than one is one */
    cydui_begin_grid(&rig.ui, -3);
    r[0] = cydui_alloc_rect(&rig.ui, 1, 1);
    r[1] = cydui_alloc_rect(&rig.ui, 1, 1);
    cydui_end_grid(&rig.ui);
    TEST_ASSERT_TRUE(r[1].y > r[0].y);
    cydui_ui_end(&rig.ui);
}

static void test_the_default_panel_is_drawn_in_the_theme_colors(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_CARD(&rig.ui) { cydui_alloc_rect(&rig.ui, 100, 40); }
    cydui_ui_end(&rig.ui);
    /* the middle of the card shows the panel color of the theme, not the background */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.table[CYDUI_WIDGET_PANEL][0][CYDUI_STATE_NORMAL].fill, rig.fb[(8 + 30) * RIG_W + 8 + 60]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_row_centers_its_children_on_the_tallest_one_after_a_frame);
    RUN_TEST(test_end_puts_children_at_the_bottom_of_the_row);
    RUN_TEST(test_start_changes_nothing_and_asks_for_no_frames);
    RUN_TEST(test_a_change_of_content_is_followed_one_frame_later);
    RUN_TEST(test_the_root_column_centers_across_the_whole_width_at_once);
    RUN_TEST(test_a_column_with_a_width_centers_in_it);
    RUN_TEST(test_a_group_is_centered_in_the_width_the_container_was_given);
    RUN_TEST(test_a_group_goes_to_the_end);
    RUN_TEST(test_without_a_width_the_group_has_no_room_to_move);
    RUN_TEST(test_the_root_column_centers_its_group_vertically);
    RUN_TEST(test_fill_inside_a_placed_container_leaves_nothing_to_move);
    RUN_TEST(test_a_grid_ignores_alignment);
    RUN_TEST(test_a_bad_place_value_means_start);
    RUN_TEST(test_a_row_inside_a_centered_column_is_centered_as_a_whole);
    RUN_TEST(test_two_levels_settle_in_at_most_one_frame_per_level);
    RUN_TEST(test_a_container_with_only_one_size_given_waits_for_its_measure);
    RUN_TEST(test_a_change_of_the_cross_size_alone_asks_for_another_frame);
    RUN_TEST(test_children_scrolled_above_the_top_keep_their_negative_positions);
    RUN_TEST(test_several_children_scrolled_above_the_top_follow_each_other);
    RUN_TEST(test_a_card_is_as_big_as_its_content_plus_padding_in_the_first_frame);
    RUN_TEST(test_the_panel_is_behind_the_children_and_the_order_is_kept);
    RUN_TEST(test_an_empty_card_is_just_its_padding);
    RUN_TEST(test_cards_nest_and_each_one_wraps_what_is_inside);
    RUN_TEST(test_a_card_can_be_given_a_size);
    RUN_TEST(test_a_card_can_place_its_children_too);
    RUN_TEST(test_a_card_in_a_scrolled_area_is_painted_where_it_is);
    RUN_TEST(test_a_full_command_list_loses_the_panel_and_keeps_the_rest_in_order);
    RUN_TEST(test_a_panel_that_only_partly_fits_is_still_ordered);
    RUN_TEST(test_card_calls_that_do_not_match_are_survived);
    RUN_TEST(test_a_card_the_application_forgot_to_close_is_still_painted);
    RUN_TEST(test_the_default_panel_is_drawn_in_the_theme_colors);
    RUN_TEST(test_a_card_with_a_height_places_its_group_inside_the_padding);
    RUN_TEST(test_a_centered_column_centers_a_card_by_its_padded_size);
    RUN_TEST(test_a_container_that_is_moved_takes_its_children_with_it);
    RUN_TEST(test_a_padded_container_with_a_height_gives_its_fill_child_the_room_inside_the_padding);
    RUN_TEST(test_a_container_that_asked_to_fill_counts_among_the_ones_that_share);
    RUN_TEST(test_a_width_or_height_of_zero_is_a_size_like_any_other);
    RUN_TEST(test_a_card_that_could_not_open_ends_without_closing_the_one_that_is_open);
    RUN_TEST(test_a_list_item_at_the_deepest_level_cannot_open_and_is_survived);
    RUN_TEST(test_the_row_places_a_card_by_its_padded_height);
    RUN_TEST(test_a_list_item_is_a_column_of_the_size_of_its_row);
    RUN_TEST(test_a_container_that_fills_takes_the_room_along_the_column);
    RUN_TEST(test_a_container_that_stretches_takes_the_room_across);
    RUN_TEST(test_a_grid_has_between_one_and_sixty_four_columns);
    RUN_TEST(test_a_container_given_no_width_has_no_room_inside);
    return UNITY_END();
}
