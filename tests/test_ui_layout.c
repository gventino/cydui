/*
 * Layout (FR-23, FR-24, CT-02, R-05). Every position is worked out by hand and pinned.
 *
 * The screen is 320x240; the root container has a margin of 8 (the style's spacing), so it
 * starts at (8,8) and has 304 x 224 to give. The default spacing between children is 8.
 */

#include <stdio.h>
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

static void begin(void)
{
    TEST_ASSERT_TRUE(cydui_ui_begin(&rig.ui, 0));
}

static bool end(void)
{
    return cydui_ui_end(&rig.ui);
}

static cydui_rect P(int w, int h)
{
    return cydui_alloc_rect(&rig.ui, w, h);
}

#define ASSERT_RECT(ex, ey, ew, eh, r)                                                            \
    do {                                                                                          \
        char msg_[96];                                                                            \
        snprintf(msg_, sizeof(msg_), "rect is (%d,%d,%d,%d), expected (%d,%d,%d,%d)", (r).x,     \
                 (r).y, (r).w, (r).h, (ex), (ey), (ew), (eh));                                    \
        TEST_ASSERT_TRUE_MESSAGE((r).x == (ex) && (r).y == (ey) && (r).w == (ew) && (r).h == (eh), \
                                 msg_);                                                           \
    } while (0)

/* ---- the screen itself is a column ---- */

static void test_root_stacks_children_top_to_bottom(void)
{
    cydui_rect a, b, c;

    start();
    begin();
    a = P(50, 20);
    b = P(70, 10);
    c = P(30, 30);
    end();
    ASSERT_RECT(8, 8, 50, 20, a);
    ASSERT_RECT(8, 36, 70, 10, b);  /* 8 + 20 + 8 */
    ASSERT_RECT(8, 54, 30, 30, c);  /* 36 + 10 + 8 */
}

static void test_positions_are_the_same_every_frame(void)
{
    cydui_rect a1, a2, b1, b2;

    start();
    begin();
    a1 = P(50, 20);
    b1 = P(50, 20);
    end();
    begin();
    a2 = P(50, 20);
    b2 = P(50, 20);
    end();
    ASSERT_RECT(a1.x, a1.y, a1.w, a1.h, a2);
    ASSERT_RECT(b1.x, b1.y, b1.w, b1.h, b2);
}

/* ---- row ---- */

static void test_row_places_children_left_to_right_and_is_as_tall_as_the_tallest(void)
{
    cydui_rect a, b, c, below;

    start();
    begin();
    cydui_begin_row(&rig.ui);
    a = P(40, 20);
    b = P(40, 30);
    c = P(40, 10);
    cydui_end_row(&rig.ui);
    below = P(10, 10);
    end();
    ASSERT_RECT(8, 8, 40, 20, a);
    ASSERT_RECT(56, 8, 40, 30, b);   /* 8 + 40 + 8 */
    ASSERT_RECT(104, 8, 40, 10, c);  /* tops are aligned */
    ASSERT_RECT(8, 46, 10, 10, below); /* the row is 30 tall: 8 + 30 + 8 */
}

static void test_set_spacing_applies_to_the_following_children_of_that_container(void)
{
    cydui_rect a, b, c, after;

    start();
    begin();
    cydui_begin_row(&rig.ui);
    cydui_set_spacing(&rig.ui, 2);
    a = P(40, 20);
    b = P(40, 20);
    cydui_set_spacing(&rig.ui, 20);
    c = P(40, 20);
    cydui_end_row(&rig.ui);
    after = P(10, 10);
    end();
    ASSERT_RECT(8, 8, 40, 20, a);
    ASSERT_RECT(50, 8, 40, 20, b);   /* 8 + 40 + 2 */
    ASSERT_RECT(110, 8, 40, 20, c);  /* 50 + 40 + 20: the gap after b was set when b was placed */
    ASSERT_RECT(8, 36, 10, 10, after); /* the root's spacing is untouched */
}

static void test_spacer_adds_a_gap_along_the_main_axis(void)
{
    cydui_rect a, b, c, d;

    start();
    begin();
    cydui_begin_row(&rig.ui);
    a = P(20, 20);
    cydui_spacer(&rig.ui, 30);
    b = P(20, 20);
    cydui_end_row(&rig.ui);
    c = P(20, 20);
    cydui_spacer(&rig.ui, 30); /* in a column the gap is vertical */
    d = P(20, 20);
    end();
    ASSERT_RECT(8, 8, 20, 20, a);
    ASSERT_RECT(74, 8, 20, 20, b);   /* a ends at 28, gap 8, spacer 36..66, gap 8 */
    ASSERT_RECT(8, 36, 20, 20, c);   /* the row is 20 tall: 8 + 20 + 8 */
    ASSERT_RECT(8, 102, 20, 20, d);  /* c ends at 56, gap 8, spacer 64..94, gap 8 */
}

/* ---- column ---- */

static void test_column_inside_a_row_and_row_inside_a_column(void)
{
    cydui_rect a, b, c, d, e;

    start();
    begin();
    cydui_begin_row(&rig.ui);
    a = P(30, 10);
    cydui_begin_column(&rig.ui);
    b = P(20, 10);
    c = P(25, 10);
    cydui_end_column(&rig.ui);
    d = P(30, 10);
    cydui_end_row(&rig.ui);
    e = P(5, 5);
    end();
    ASSERT_RECT(8, 8, 30, 10, a);
    ASSERT_RECT(46, 8, 20, 10, b);   /* the column starts where the next child would: 8+30+8 */
    ASSERT_RECT(46, 26, 25, 10, c);  /* 8 + 10 + 8 */
    ASSERT_RECT(79, 8, 30, 10, d);   /* the column is 25 wide: 46 + 25 + 8 */
    ASSERT_RECT(8, 44, 5, 5, e);     /* the row is 28 tall (the column): 8 + 28 + 8 */
}

/* ---- grid ---- */

static void test_grid_has_equal_columns_and_rows_as_tall_as_their_tallest(void)
{
    cydui_rect c[7], after;
    int i;

    start();
    begin();
    cydui_begin_grid(&rig.ui, 3); /* cells: (304 - 2 * 8) / 3 = 96 wide */
    for (i = 0; i < 7; i++) {
        c[i] = P(30, i == 1 ? 40 : 20); /* the second cell is taller */
    }
    cydui_end_grid(&rig.ui);
    after = P(10, 10);
    end();
    ASSERT_RECT(8, 8, 30, 20, c[0]);
    ASSERT_RECT(112, 8, 30, 40, c[1]);  /* 8 + 96 + 8 */
    ASSERT_RECT(216, 8, 30, 20, c[2]);
    ASSERT_RECT(8, 56, 30, 20, c[3]);   /* the first row is 40 tall: 8 + 40 + 8 */
    ASSERT_RECT(112, 56, 30, 20, c[4]);
    ASSERT_RECT(216, 56, 30, 20, c[5]);
    ASSERT_RECT(8, 84, 30, 20, c[6]);   /* 56 + 20 + 8 */
    ASSERT_RECT(8, 112, 10, 10, after); /* the last row is 20 tall: 84 + 20 + 8 */
}

static void test_grid_fill_takes_the_whole_cell(void)
{
    cydui_rect a, b;

    start();
    begin();
    cydui_begin_grid(&rig.ui, 2); /* (304 - 8) / 2 = 148 */
    cydui_next_fill(&rig.ui);
    a = P(10, 20);
    cydui_next_fill(&rig.ui);
    b = P(10, 20);
    cydui_end_grid(&rig.ui);
    end();
    ASSERT_RECT(8, 8, 148, 20, a);
    ASSERT_RECT(164, 8, 148, 20, b);
}

static void test_grid_column_count_is_made_safe(void)
{
    cydui_rect a, b;

    start();
    begin();
    cydui_begin_grid(&rig.ui, 0); /* treated as one column */
    a = P(10, 10);
    b = P(10, 10);
    cydui_end_grid(&rig.ui);
    ASSERT_RECT(8, 8, 10, 10, a);
    ASSERT_RECT(8, 26, 10, 10, b);

    cydui_begin_grid(&rig.ui, -5);
    a = P(10, 10);
    cydui_end_grid(&rig.ui);
    ASSERT_RECT(8, 44, 10, 10, a);

    cydui_begin_grid(&rig.ui, 100000); /* clamped, no division by zero or an absurd cell */
    TEST_ASSERT_EQUAL_UINT16(64, rig.ui.regions[rig.ui.depth].cols);
    a = P(10, 10);
    cydui_end_grid(&rig.ui);
    TEST_ASSERT_EQUAL_INT(8, a.x);
    cydui_begin_grid(&rig.ui, 3);
    TEST_ASSERT_EQUAL_UINT16(3, rig.ui.regions[rig.ui.depth].cols);
    cydui_end_grid(&rig.ui);
    end();
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

/* ---- padding ---- */

static void test_padding_insets_the_children_and_grows_the_container(void)
{
    cydui_rect a, b, after;

    start();
    begin();
    cydui_begin_padding(&rig.ui, 10);
    a = P(50, 20);
    b = P(40, 20);
    cydui_end_padding(&rig.ui);
    after = P(10, 10);
    end();
    ASSERT_RECT(18, 18, 50, 20, a);
    ASSERT_RECT(18, 46, 40, 20, b);
    ASSERT_RECT(8, 84, 10, 10, after); /* the container is 48 + 2 * 10 = 68 tall: 8 + 68 + 8 */
}

static void test_a_padded_container_is_wider_by_its_padding_too(void)
{
    cydui_rect inner, next;

    start();
    begin();
    cydui_begin_row(&rig.ui);
    cydui_begin_padding(&rig.ui, 10);
    inner = P(30, 10);
    cydui_end_padding(&rig.ui);
    next = P(5, 5);
    cydui_end_row(&rig.ui);
    end();
    ASSERT_RECT(18, 18, 30, 10, inner);
    ASSERT_RECT(8 + (30 + 20) + 8, 8, 5, 5, next); /* the padded container is 30 + 2 * 10 wide */
}

static void test_a_container_can_be_given_a_fixed_size(void)
{
    cydui_rect inner, next, below;

    start();
    begin();
    cydui_begin_row(&rig.ui);
    cydui_next_width(&rig.ui, 100);
    cydui_next_height(&rig.ui, 60);
    cydui_begin_column(&rig.ui);
    inner = P(10, 10); /* much smaller than the container */
    cydui_end_column(&rig.ui);
    next = P(5, 5);
    cydui_end_row(&rig.ui);
    below = P(5, 5);
    end();
    ASSERT_RECT(8, 8, 10, 10, inner);
    ASSERT_RECT(8 + 100 + 8, 8, 5, 5, next);  /* the container is 100 wide, not 10 */
    ASSERT_RECT(8, 8 + 60 + 8, 5, 5, below);  /* and 60 tall: it sets the height of the row */
}

static void test_each_container_keeps_its_own_fill_state(void)
{
    cydui_rect f1, f2;
    bool more = true;
    int i;

    start();
    for (i = 0; i < 4; i++) { /* two sibling rows, with different fixed neighbours */
        begin();
        cydui_begin_row(&rig.ui);
        P(40, 10);
        cydui_next_fill(&rig.ui);
        f1 = P(1, 10);
        cydui_end_row(&rig.ui);
        cydui_begin_row(&rig.ui);
        P(100, 10);
        cydui_next_fill(&rig.ui);
        f2 = P(1, 10);
        cydui_end_row(&rig.ui);
        more = end();
    }
    TEST_ASSERT_EQUAL_INT(304 - (40 + 8), f1.w);
    TEST_ASSERT_EQUAL_INT(304 - (100 + 8), f2.w);
    TEST_ASSERT_FALSE_MESSAGE(more, "the two rows keep overwriting each other's state and never settle");
}

/* ---- modifiers ---- */

static void test_modifiers_apply_to_one_widget_only(void)
{
    cydui_rect a, b, c, d;

    start();
    begin();
    cydui_next_width(&rig.ui, 100);
    a = P(10, 10);
    b = P(10, 10);
    cydui_next_height(&rig.ui, 33);
    c = P(10, 10);
    cydui_next_width(&rig.ui, 5);
    cydui_next_height(&rig.ui, 6);
    d = P(10, 10);
    end();
    TEST_ASSERT_EQUAL_INT(100, a.w);
    TEST_ASSERT_EQUAL_INT(10, a.h);
    TEST_ASSERT_EQUAL_INT(10, b.w); /* forgotten after one use */
    TEST_ASSERT_EQUAL_INT(33, c.h);
    TEST_ASSERT_EQUAL_INT(10, c.w);
    TEST_ASSERT_EQUAL_INT(5, d.w);
    TEST_ASSERT_EQUAL_INT(6, d.h);
}

static void test_modifiers_are_cleared_at_the_start_of_a_frame(void)
{
    cydui_rect a;

    start();
    begin();
    cydui_next_width(&rig.ui, 77); /* set and never used */
    cydui_next_fill(&rig.ui);
    cydui_next_disabled(&rig.ui);
    end();
    begin();
    a = P(10, 10);
    end();
    ASSERT_RECT(8, 8, 10, 10, a);
}

static void test_take_mods_reads_and_clears(void)
{
    cydui_mods m;

    start();
    begin();
    cydui_next_width(&rig.ui, 40);
    cydui_next_disabled(&rig.ui);
    m = cydui_take_mods(&rig.ui);
    TEST_ASSERT_EQUAL_INT(40, m.w);
    TEST_ASSERT_EQUAL_INT(-1, m.h);
    TEST_ASSERT_TRUE(m.disabled);
    TEST_ASSERT_FALSE(m.fill);
    m = cydui_take_mods(&rig.ui);
    TEST_ASSERT_EQUAL_INT(-1, m.w);
    TEST_ASSERT_FALSE(m.disabled);
}

static void test_stretch_takes_the_whole_cross_axis(void)
{
    cydui_rect a, b;

    start();
    begin();
    cydui_next_stretch(&rig.ui);
    a = P(10, 20);     /* in a column: the full width */
    cydui_begin_row(&rig.ui);
    cydui_next_stretch(&rig.ui);
    b = P(10, 20);     /* in a row: the full height of the room it was given */
    cydui_end_row(&rig.ui);
    end();
    ASSERT_RECT(8, 8, 304, 20, a);
    TEST_ASSERT_EQUAL_INT(10, b.w);
    TEST_ASSERT_EQUAL_INT(224 - 28, b.h); /* what is left below a: 224 - (20 + 8) */
}

static void test_sizes_are_never_negative(void)
{
    cydui_rect a;

    start();
    begin();
    a = P(-5, -9);
    end();
    TEST_ASSERT_EQUAL_INT(0, a.w);
    TEST_ASSERT_EQUAL_INT(0, a.h);
    begin();
    cydui_next_width(&rig.ui, -100);
    cydui_next_height(&rig.ui, 100000);
    a = P(10, 10);
    end();
    TEST_ASSERT_EQUAL_INT(0, a.w);
    TEST_ASSERT_EQUAL_INT(32767, a.h);
}

/* ---- fill: one frame of delay (FR-24) ---- */

static void test_fill_shares_the_space_left_and_settles_in_the_second_frame(void)
{
    cydui_rect a, f, c;
    bool more;

    start();
    begin(); /* frame 1: nobody knows how much the others take yet */
    cydui_begin_row(&rig.ui);
    a = P(40, 20);
    cydui_next_fill(&rig.ui);
    f = P(10, 20);
    c = P(40, 20);
    cydui_end_row(&rig.ui);
    more = end();
    ASSERT_RECT(8, 8, 40, 20, a);
    TEST_ASSERT_EQUAL_INT_MESSAGE(256, f.w, "first frame: takes what is left");
    TEST_ASSERT_TRUE_MESSAGE(more, "a layout that is still settling must ask for another frame");

    begin(); /* frame 2: 304 - (40 + 8 + 8 + 40) = 208 */
    cydui_begin_row(&rig.ui);
    a = P(40, 20);
    cydui_next_fill(&rig.ui);
    f = P(10, 20);
    c = P(40, 20);
    cydui_end_row(&rig.ui);
    more = end();
    ASSERT_RECT(8, 8, 40, 20, a);
    ASSERT_RECT(56, 8, 208, 20, f);
    ASSERT_RECT(272, 8, 40, 20, c);  /* the row ends exactly at the right margin: 272 + 40 = 312 */
    TEST_ASSERT_FALSE_MESSAGE(more, "settled: no reason for another frame");

    begin(); /* and it stays put */
    cydui_begin_row(&rig.ui);
    P(40, 20);
    cydui_next_fill(&rig.ui);
    f = P(10, 20);
    P(40, 20);
    cydui_end_row(&rig.ui);
    more = end();
    TEST_ASSERT_EQUAL_INT(208, f.w);
    TEST_ASSERT_FALSE(more);
}

static void test_the_fill_share_follows_a_change_of_the_others(void)
{
    cydui_rect f;
    bool more;
    int i;

    start();
    for (i = 0; i < 3; i++) { /* settle with a 40 px sibling */
        begin();
        cydui_begin_row(&rig.ui);
        P(40, 20);
        cydui_next_fill(&rig.ui);
        f = P(10, 20);
        cydui_end_row(&rig.ui);
        end();
    }
    TEST_ASSERT_EQUAL_INT(256, f.w); /* 304 - (40 + 8) */

    begin(); /* the sibling grows to 100: this frame still uses the old numbers ... */
    cydui_begin_row(&rig.ui);
    P(100, 20);
    cydui_next_fill(&rig.ui);
    f = P(10, 20);
    cydui_end_row(&rig.ui);
    more = end();
    TEST_ASSERT_EQUAL_INT(256, f.w);
    TEST_ASSERT_TRUE_MESSAGE(more, "the change was not noticed");

    begin(); /* ... and the next one has caught up: 304 - (100 + 8) */
    cydui_begin_row(&rig.ui);
    P(100, 20);
    cydui_next_fill(&rig.ui);
    f = P(10, 20);
    cydui_end_row(&rig.ui);
    more = end();
    TEST_ASSERT_EQUAL_INT(196, f.w);
    TEST_ASSERT_FALSE(more);
}

static void test_several_fills_split_the_space_and_the_remainder_goes_to_the_first(void)
{
    cydui_rect f[3];
    int i, k;

    start();
    for (i = 0; i < 3; i++) {
        begin();
        cydui_next_width(&rig.ui, 101); /* a row 101 wide: 101 - 2 gaps of 8 = 85 to share */
        cydui_begin_row(&rig.ui);
        for (k = 0; k < 3; k++) {
            cydui_next_fill(&rig.ui);
            f[k] = P(1, 10);
        }
        cydui_end_row(&rig.ui);
        end();
    }
    TEST_ASSERT_EQUAL_INT(29, f[0].w); /* 85 / 3 = 28, remainder 1 */
    TEST_ASSERT_EQUAL_INT(28, f[1].w);
    TEST_ASSERT_EQUAL_INT(28, f[2].w);
    TEST_ASSERT_EQUAL_INT(29 + 28 + 28 + 16, f[2].x + f[2].w - f[0].x); /* they fill the row */
}

static void test_fill_in_a_column_shares_the_height(void)
{
    cydui_rect top, mid, bot;
    int i;

    start();
    for (i = 0; i < 3; i++) {
        begin();
        cydui_begin_column(&rig.ui);
        top = P(10, 20);
        cydui_next_fill(&rig.ui);
        mid = P(10, 5);
        bot = P(10, 20);
        cydui_end_column(&rig.ui);
        end();
    }
    TEST_ASSERT_EQUAL_INT(8, top.y);
    TEST_ASSERT_EQUAL_INT(224 - (20 + 8 + 8 + 20), mid.h); /* the screen is 224 tall inside the margin */
    TEST_ASSERT_EQUAL_INT(8 + 224 - 20, bot.y);             /* the last child ends at the bottom margin */
}

static void test_a_spacer_that_fills_pushes_what_follows_to_the_far_end(void)
{
    cydui_rect left, right;
    int i;

    start();
    for (i = 0; i < 3; i++) {
        begin();
        cydui_begin_row(&rig.ui);
        left = P(60, 20);
        cydui_spacer_fill(&rig.ui);
        right = P(50, 20);
        cydui_end_row(&rig.ui);
        end();
    }
    TEST_ASSERT_EQUAL_INT(8, left.x);
    TEST_ASSERT_EQUAL_INT(8 + 304 - 50, right.x); /* flush with the right margin */
}

static void test_a_container_can_fill_its_parent(void)
{
    cydui_rect a, inner;
    int i;

    start();
    for (i = 0; i < 3; i++) {
        begin();
        cydui_begin_row(&rig.ui);
        a = P(40, 20);
        cydui_next_fill(&rig.ui);
        cydui_begin_column(&rig.ui);
        inner = P(10, 10);
        cydui_end_column(&rig.ui);
        cydui_end_row(&rig.ui);
        end();
    }
    ASSERT_RECT(8, 8, 40, 20, a);
    TEST_ASSERT_EQUAL_INT(56, inner.x);
    /* ... and after the column, the row would continue at 8+40+8+256+8 */
}

static void test_fill_with_nothing_to_share_collapses_without_trouble(void)
{
    cydui_rect f;
    int i;

    start();
    for (i = 0; i < 3; i++) {
        begin();
        cydui_next_width(&rig.ui, 30);
        cydui_begin_row(&rig.ui);
        P(100, 10); /* already wider than the row */
        cydui_next_fill(&rig.ui);
        f = P(1, 10);
        cydui_end_row(&rig.ui);
        end();
    }
    TEST_ASSERT_EQUAL_INT(0, f.w);
}

/* ---- depth, balance (CT-02) ---- */

static void test_eight_nested_containers_are_fine_the_ninth_is_ignored(void)
{
    cydui_rect inner, after;
    int i;

    start();
    begin();
    for (i = 0; i < CYDUI_MAX_LAYOUT_DEPTH; i++) {
        cydui_begin_column(&rig.ui);
    }
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    cydui_begin_column(&rig.ui); /* too deep */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_LAYOUT, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_EQUAL_UINT8(CYDUI_MAX_LAYOUT_DEPTH, rig.ui.depth);
    inner = P(10, 10); /* flows into the eighth */
    cydui_end_column(&rig.ui); /* the end of the ignored one: must not close the eighth */
    TEST_ASSERT_EQUAL_UINT8(CYDUI_MAX_LAYOUT_DEPTH, rig.ui.depth);
    for (i = 0; i < CYDUI_MAX_LAYOUT_DEPTH; i++) {
        cydui_end_column(&rig.ui);
    }
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    after = P(10, 10);
    end();
    ASSERT_RECT(8, 8, 10, 10, inner);
    TEST_ASSERT_EQUAL_INT(8, after.x);
}

static void test_a_wall_of_too_deep_containers_cannot_break_anything(void)
{
    int i;

    start();
    begin();
    for (i = 0; i < 100; i++) {
        cydui_begin_row(&rig.ui);
    }
    TEST_ASSERT_EQUAL_UINT8(CYDUI_MAX_LAYOUT_DEPTH, rig.ui.depth);
    TEST_ASSERT_EQUAL_UINT8(100 - CYDUI_MAX_LAYOUT_DEPTH, rig.ui.skipped);
    for (i = 0; i < 100; i++) {
        cydui_end_row(&rig.ui);
    }
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.skipped);
    end();
}

static void test_an_end_without_a_begin_is_ignored_and_flagged(void)
{
    cydui_rect a;

    start();
    begin();
    cydui_end_row(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_LAYOUT, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    cydui_ui_clear_errors(&rig.ui);

    cydui_begin_row(&rig.ui);
    cydui_end_column(&rig.ui); /* the wrong kind: ignored, the row stays open */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_LAYOUT, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_EQUAL_UINT8(1, rig.ui.depth);
    a = P(10, 10);
    cydui_end_row(&rig.ui);
    end();
    ASSERT_RECT(8, 8, 10, 10, a);
}

static void test_containers_left_open_are_closed_at_the_end_of_the_frame(void)
{
    cydui_rect a, b;

    start();
    begin();
    cydui_begin_row(&rig.ui);
    cydui_begin_column(&rig.ui);
    P(10, 10);
    end(); /* never closed */
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
    cydui_ui_clear_errors(&rig.ui);

    begin(); /* the next frame starts from a clean slate */
    a = P(10, 10);
    b = P(10, 10);
    end();
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    ASSERT_RECT(8, 8, 10, 10, a);
    ASSERT_RECT(8, 26, 10, 10, b);
}

static void test_scope_macros_open_and_close_containers(void)
{
    cydui_rect a, b, c;
    int i = 0;

    start();
    begin();
    CYDUI_ROW(&rig.ui) {
        a = P(40, 20);
        b = P(40, 20);
        i++;
    }
    c = P(10, 10);
    CYDUI_COLUMN(&rig.ui) {
        CYDUI_GRID(&rig.ui, 2) {
            P(5, 5);
        }
        CYDUI_PADDING(&rig.ui, 3) {
            P(5, 5);
        }
    }
    end();
    TEST_ASSERT_EQUAL_INT(1, i); /* the body ran once */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    ASSERT_RECT(8, 8, 40, 20, a);
    ASSERT_RECT(56, 8, 40, 20, b);
    ASSERT_RECT(8, 36, 10, 10, c);
}

static void test_a_break_inside_a_scope_macro_is_caught_at_the_end_of_the_frame(void)
{
    start();
    begin();
    CYDUI_ROW(&rig.ui) {
        P(10, 10);
        break; /* skips the end call */
    }
    TEST_ASSERT_EQUAL_UINT8(1, rig.ui.depth);
    end();
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT);
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.depth);
}

/* ---- room, empty containers ---- */

static void test_layout_avail_reports_the_room_left(void)
{
    cydui_rect r;

    start();
    begin();
    r = cydui_layout_avail(&rig.ui);
    ASSERT_RECT(8, 8, 304, 224, r);
    P(10, 20);
    r = cydui_layout_avail(&rig.ui);
    ASSERT_RECT(8, 36, 304, 196, r); /* below the first child */

    cydui_begin_row(&rig.ui);
    P(50, 10);
    r = cydui_layout_avail(&rig.ui);
    ASSERT_RECT(66, 36, 246, 196, r); /* right of the first child: 8 + 50 + 8 */
    cydui_end_row(&rig.ui);
    end();
}

static void test_an_empty_container_takes_no_room_but_the_gap(void)
{
    cydui_rect a, b;

    start();
    begin();
    a = P(10, 10);
    cydui_begin_row(&rig.ui);
    cydui_end_row(&rig.ui); /* nothing inside */
    b = P(10, 10);
    end();
    ASSERT_RECT(8, 8, 10, 10, a);
    TEST_ASSERT_EQUAL_INT(8 + 10 + 8 + 0 + 8, b.y); /* it counts as a child of size zero: one extra gap */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_root_stacks_children_top_to_bottom);
    RUN_TEST(test_positions_are_the_same_every_frame);
    RUN_TEST(test_row_places_children_left_to_right_and_is_as_tall_as_the_tallest);
    RUN_TEST(test_set_spacing_applies_to_the_following_children_of_that_container);
    RUN_TEST(test_spacer_adds_a_gap_along_the_main_axis);
    RUN_TEST(test_column_inside_a_row_and_row_inside_a_column);
    RUN_TEST(test_grid_has_equal_columns_and_rows_as_tall_as_their_tallest);
    RUN_TEST(test_grid_fill_takes_the_whole_cell);
    RUN_TEST(test_grid_column_count_is_made_safe);
    RUN_TEST(test_padding_insets_the_children_and_grows_the_container);
    RUN_TEST(test_a_padded_container_is_wider_by_its_padding_too);
    RUN_TEST(test_a_container_can_be_given_a_fixed_size);
    RUN_TEST(test_each_container_keeps_its_own_fill_state);
    RUN_TEST(test_modifiers_apply_to_one_widget_only);
    RUN_TEST(test_modifiers_are_cleared_at_the_start_of_a_frame);
    RUN_TEST(test_take_mods_reads_and_clears);
    RUN_TEST(test_stretch_takes_the_whole_cross_axis);
    RUN_TEST(test_sizes_are_never_negative);
    RUN_TEST(test_fill_shares_the_space_left_and_settles_in_the_second_frame);
    RUN_TEST(test_the_fill_share_follows_a_change_of_the_others);
    RUN_TEST(test_several_fills_split_the_space_and_the_remainder_goes_to_the_first);
    RUN_TEST(test_fill_in_a_column_shares_the_height);
    RUN_TEST(test_a_spacer_that_fills_pushes_what_follows_to_the_far_end);
    RUN_TEST(test_a_container_can_fill_its_parent);
    RUN_TEST(test_fill_with_nothing_to_share_collapses_without_trouble);
    RUN_TEST(test_eight_nested_containers_are_fine_the_ninth_is_ignored);
    RUN_TEST(test_a_wall_of_too_deep_containers_cannot_break_anything);
    RUN_TEST(test_an_end_without_a_begin_is_ignored_and_flagged);
    RUN_TEST(test_containers_left_open_are_closed_at_the_end_of_the_frame);
    RUN_TEST(test_scope_macros_open_and_close_containers);
    RUN_TEST(test_a_break_inside_a_scope_macro_is_caught_at_the_end_of_the_frame);
    RUN_TEST(test_layout_avail_reports_the_room_left);
    RUN_TEST(test_an_empty_container_takes_no_room_but_the_gap);
    return UNITY_END();
}
