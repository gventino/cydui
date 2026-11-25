/*
 * What it takes to write a widget of one's own (FR-36), proved with examples/theme-gallery/rating.c,
 * which is built from the public headers alone, and the small public functions it needed. Default
 * theme, 12 px font, 32 px touch targets, margin 8: with no label the stars of a rating at the top
 * of the screen are targets 32 px wide from x = 8, so star i has its center at (8 + 32 i + 16, 24).
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cydui_themes.h"
#include "rating.h"
#include "cydui_sans_24.h"
#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;
static int value;
static bool changed;

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    value = 0;
}

static void frame(void)
{
    cydui_ui_begin(&rig.ui, 0);
    changed = rating(&rig.ui, "##r", &value);
    cydui_ui_end(&rig.ui);
}

static int star_x(int i) { return 8 + 32 * i + 16; }

static void tap(int x, int y)
{
    rig_down(&rig, x, y);
    rig_up(&rig, x, y);
    frame();
}

static cydui_color px(int x, int y)
{
    return rig.fb[y * RIG_W + x];
}

/* ---- behavior ---- */

static void test_a_tap_on_a_star_lights_it_and_the_ones_before_it(void)
{
    int i;

    start();
    for (i = 0; i < 5; i++) {
        tap(star_x(i), 24);
        TEST_ASSERT_EQUAL_INT(i + 1, value);
        TEST_ASSERT_TRUE(changed);
    }
}

static void test_a_tap_on_the_last_lit_star_clears_it(void)
{
    start();
    value = 3;
    tap(star_x(2), 24);
    TEST_ASSERT_EQUAL_INT(0, value);
    TEST_ASSERT_TRUE(changed);
    tap(star_x(2), 24);
    TEST_ASSERT_EQUAL_INT(3, value);
    tap(star_x(0), 24); /* an earlier star is not "the last lit": it becomes the rating */
    TEST_ASSERT_EQUAL_INT(1, value);
    tap(star_x(0), 24);
    TEST_ASSERT_EQUAL_INT(0, value);
}

static void test_it_reports_a_change_only_when_there_is_one(void)
{
    start();
    frame();
    TEST_ASSERT_FALSE(changed);
    value = 2;
    frame();
    TEST_ASSERT_FALSE(changed);
    tap(300, 24); /* beside the widget */
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL_INT(2, value);
    tap(star_x(1), 60); /* below it */
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL_INT(2, value);
}

static void test_the_edges_of_the_stars_area(void)
{
    start();
    tap(8, 24); /* the first pixel */
    TEST_ASSERT_EQUAL_INT(1, value);
    value = 0;
    tap(8 + 159, 24); /* the last one: 5 x 32 = 160 px */
    TEST_ASSERT_EQUAL_INT(5, value);
    value = 0;
    tap(8 + 160, 24);
    TEST_ASSERT_EQUAL_INT(0, value);
    tap(7, 24);
    TEST_ASSERT_EQUAL_INT(0, value);
    tap(star_x(0), 8 + 32); /* one pixel below */
    TEST_ASSERT_EQUAL_INT(0, value);
    tap(8 + 31, 24); /* the last pixel of the first star... */
    TEST_ASSERT_EQUAL_INT(1, value);
    value = 0;
    tap(8 + 32, 24); /* ...and the first of the second */
    TEST_ASSERT_EQUAL_INT(2, value);
}

static void test_a_drag_or_a_release_elsewhere_does_not_count(void)
{
    start();
    rig_down(&rig, star_x(0), 24);
    rig_move(&rig, star_x(3), 24); /* far beyond the tap slop */
    rig_up(&rig, star_x(3), 24);
    frame();
    TEST_ASSERT_EQUAL_INT(0, value);
    TEST_ASSERT_FALSE(changed);
    rig_down(&rig, star_x(0), 24);
    rig_up(&rig, 300, 100); /* let go outside */
    frame();
    TEST_ASSERT_EQUAL_INT(0, value);
}

static void test_a_value_out_of_range_is_clamped(void)
{
    start();
    value = -7;
    frame();
    TEST_ASSERT_EQUAL_INT(0, value);
    value = 99;
    frame();
    TEST_ASSERT_EQUAL_INT(5, value);
    value = INT_MIN;
    frame();
    TEST_ASSERT_EQUAL_INT(0, value);
    value = INT_MAX;
    frame();
    TEST_ASSERT_EQUAL_INT(5, value);
    cydui_ui_begin(&rig.ui, 0);
    rating(&rig.ui, "##n", NULL); /* no variable: nothing to write to */
    cydui_ui_end(&rig.ui);
}

static void test_a_disabled_rating_does_not_react_and_looks_dimmed(void)
{
    const cydui_style *st = &rig.theme.style;
    cydui_color on;

    start();
    value = 2;
    frame();
    on = px(star_x(0), 24);
    TEST_ASSERT_EQUAL_HEX16(st->accent, on);
    rig_down(&rig, star_x(4), 24);
    rig_up(&rig, star_x(4), 24);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    changed = rating(&rig.ui, "##r", &value);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE(changed);
    TEST_ASSERT_EQUAL_INT(2, value);
    TEST_ASSERT_EQUAL_HEX16(cydui_blend(st->bg, st->accent, 110), px(star_x(0), 24));
    TEST_ASSERT_EQUAL_HEX16(cydui_blend(st->bg, st->border, 110), px(star_x(4), 24));
}

/* ---- the theme ---- */

static void check_colors(const cydui_theme *t)
{
    int i;

    value = 2;
    frame();
    for (i = 0; i < 5; i++) {
        cydui_color want = i < 2 ? t->style.accent : t->style.border;
        char msg[48];
        snprintf(msg, sizeof(msg), "star %d in theme %s", i, t->name);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(want, px(star_x(i), 24), msg); /* the center is fully covered */
    }
}

static void test_the_stars_take_their_colors_from_the_theme(void)
{
    static cydui_theme clean, dark;

    start();
    check_colors(&rig.theme);
    cydui_theme_init_clean(&clean);
    cydui_theme_init_clean_dark(&dark);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &clean));
    check_colors(&clean);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &dark));
    check_colors(&dark);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &rig.theme)); /* and back */
    check_colors(&rig.theme);
}

/* ---- identity and measure ---- */

static void test_only_the_visible_part_of_the_label_counts(void)
{
    cydui_text_extent e;
    int x0;

    start();
    e = cydui_text_size(rig.theme.style.font, "Note", 4, 0);
    x0 = 8 + e.w + 8; /* the label, a gap of 8, then the first target */
    cydui_ui_begin(&rig.ui, 0);
    rating(&rig.ui, "Note##1", &value);
    cydui_ui_end(&rig.ui);
    rig_down(&rig, x0 + 16, 24);
    rig_up(&rig, x0 + 16, 24);
    cydui_ui_begin(&rig.ui, 0);
    changed = rating(&rig.ui, "Note##1", &value);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_INT(1, value);
    rig_down(&rig, x0 + 160 + 16, 24); /* where a sixth target would be: nothing there */
    rig_up(&rig, x0 + 160 + 16, 24);
    cydui_ui_begin(&rig.ui, 0);
    changed = rating(&rig.ui, "Note##1", &value);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE(changed);
}

static void test_equal_labels_collide_and_suffixes_tell_them_apart(void)
{
    int a = 1, b = 2;

    start();
    cydui_ui_begin(&rig.ui, 0);
    rating(&rig.ui, "Note", &a);
    rating(&rig.ui, "Note", &b);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_ID_COLLISION);

    cydui_ui_clear_errors(&rig.ui);
    cydui_ui_begin(&rig.ui, 0);
    rating(&rig.ui, "Note##a", &a);
    rating(&rig.ui, "Note##b", &b);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui) & CYDUI_ERR_ID_COLLISION);
}

static void test_the_frame_limit_covers_widgets_of_the_application_too(void)
{
    static int vals[CYDUI_MAX_WIDGETS + 88];
    int i;

    start();
    for (i = 0; i < CYDUI_MAX_WIDGETS + 88; i++) {
        vals[i] = 9; /* out of range: a rating that runs clamps it to 5 */
    }
    cydui_ui_begin(&rig.ui, 0);
    for (i = 0; i < CYDUI_MAX_WIDGETS + 88; i++) {
        cydui_push_id_int(&rig.ui, i);
        rating(&rig.ui, "##r", &vals[i]);
        cydui_pop_id(&rig.ui);
    }
    TEST_ASSERT_EQUAL_UINT16(CYDUI_MAX_WIDGETS, rig.ui.widgets);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_TOO_MANY_WIDGETS);
    TEST_ASSERT_EQUAL_INT(5, vals[CYDUI_MAX_WIDGETS - 1]); /* the last one that counted ran */
    TEST_ASSERT_EQUAL_INT(9, vals[CYDUI_MAX_WIDGETS]);     /* the first one past the limit did nothing */
    TEST_ASSERT_EQUAL_INT(9, vals[CYDUI_MAX_WIDGETS + 87]);
    cydui_ui_end(&rig.ui);
}

static void test_a_label_taller_than_the_stars_makes_the_widget_taller(void)
{
    cydui_text_extent e;
    cydui_rect next;

    start();
    rig.theme.style.font = &cydui_sans_24;
    rig.theme.style.min_target = 0; /* the stars then give 20 px, less than a 24 px line */
    e = cydui_text_size(&cydui_sans_24, "Note", 4, 0);
    TEST_ASSERT_TRUE(e.h > 20);
    cydui_ui_begin(&rig.ui, 0);
    rating(&rig.ui, "Note", &value);
    next = cydui_alloc_rect(&rig.ui, 10, 10);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(8 + e.h + 8, next.y);
}

/* ---- inside the containers ---- */

static void test_it_works_in_a_scroll_area(void)
{
    int i;

    start();
    for (i = 0; i < 3; i++) {
        rig_advance(&rig, 16);
        if (i == 2) {
            cydui_scroll_to(&rig.ui, "area", 20);
        }
        cydui_ui_begin(&rig.ui, 0);
        CYDUI_SCROLL(&rig.ui, "area", 100) {
            rating(&rig.ui, "##r", &value);
            cydui_spacer(&rig.ui, 300);
        }
        cydui_ui_end(&rig.ui);
    }
    /* scrolled by 20: the rating is at y = -12..19, the window shows y = 8..19 of it */
    rig_down(&rig, star_x(3), 12);
    rig_up(&rig, star_x(3), 12);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "area", 100) {
        rating(&rig.ui, "##r", &value);
        cydui_spacer(&rig.ui, 300);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(4, value);
    rig_down(&rig, star_x(1), 6); /* above the window: the part of the widget there cannot be touched */
    rig_up(&rig, star_x(1), 6);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_SCROLL(&rig.ui, "area", 100) {
        rating(&rig.ui, "##r", &value);
        cydui_spacer(&rig.ui, 300);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(4, value);
}

static void test_it_works_in_a_card(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_CARD(&rig.ui) { rating(&rig.ui, "##r", &value); }
    cydui_ui_end(&rig.ui);
    rig_down(&rig, 18 + 32 + 16, 34); /* the card pads it by 10: stars from x = 18, y = 18 */
    rig_up(&rig, 18 + 32 + 16, 34);
    cydui_ui_begin(&rig.ui, 0);
    CYDUI_CARD(&rig.ui) { changed = rating(&rig.ui, "##r", &value); }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_INT(2, value);
}

static void test_it_is_placed_like_any_widget(void)
{
    int i;

    start();
    for (i = 0; i < 3; i++) { /* a 80 px tall block next to it, the rating centered on the row */
        cydui_ui_begin(&rig.ui, 0);
        cydui_begin_row(&rig.ui);
        cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
        cydui_alloc_rect(&rig.ui, 10, 80);
        rating(&rig.ui, "##r", &value);
        cydui_end_row(&rig.ui);
        cydui_ui_end(&rig.ui);
    }
    /* stars from x = 8 + 10 + 8 = 26, y = 8 + (80 - 32) / 2 = 32 .. 63 */
    rig_down(&rig, 26 + 16, 48);
    rig_up(&rig, 26 + 16, 48);
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_row(&rig.ui);
    cydui_set_cross_align(&rig.ui, CYDUI_PLACE_CENTER);
    cydui_alloc_rect(&rig.ui, 10, 80);
    changed = rating(&rig.ui, "##r", &value);
    cydui_end_row(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(changed);
    TEST_ASSERT_EQUAL_INT(1, value);
}

/* ---- the small public functions ---- */

static void test_label_len_stops_at_the_suffix_and_at_the_limit(void)
{
    char *exact = (char *)malloc(CYDUI_MAX_STR); /* no terminator: nothing past the limit may be read */
    char *hash_last = (char *)malloc(CYDUI_MAX_STR);
    char *suffix_edge = (char *)malloc(CYDUI_MAX_STR);

    TEST_ASSERT_NOT_NULL(exact);
    TEST_ASSERT_NOT_NULL(hash_last);
    TEST_ASSERT_NOT_NULL(suffix_edge);
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)cydui_label_len(NULL));
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)cydui_label_len(""));
    TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)cydui_label_len("##a"));
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)cydui_label_len("ab##"));
    TEST_ASSERT_EQUAL_UINT32(4, (uint32_t)cydui_label_len("Note##1"));
    TEST_ASSERT_EQUAL_UINT32(3, (uint32_t)cydui_label_len("a#b"));
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)cydui_label_len("#"));
    TEST_ASSERT_EQUAL_UINT32(2, (uint32_t)cydui_label_len("a#"));
    TEST_ASSERT_EQUAL_UINT32(1, (uint32_t)cydui_label_len("a###"));

    memset(exact, 'x', CYDUI_MAX_STR);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_MAX_STR, (uint32_t)cydui_label_len(exact));
    memset(hash_last, 'x', CYDUI_MAX_STR);
    hash_last[CYDUI_MAX_STR - 1] = '#'; /* a lone '#' as the last allowed byte: the next one is not looked at */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_MAX_STR, (uint32_t)cydui_label_len(hash_last));
    memset(suffix_edge, 'x', CYDUI_MAX_STR);
    suffix_edge[CYDUI_MAX_STR - 2] = '#';
    suffix_edge[CYDUI_MAX_STR - 1] = '#'; /* a whole "##" that ends exactly at the limit */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_MAX_STR - 2, (uint32_t)cydui_label_len(suffix_edge));
    free(exact);
    free(hash_last);
    free(suffix_edge);
}

static void test_state_of_gives_disabled_priority_over_pressed(void)
{
    TEST_ASSERT_EQUAL_INT(CYDUI_STATE_NORMAL, cydui_state_of(false, false));
    TEST_ASSERT_EQUAL_INT(CYDUI_STATE_PRESSED, cydui_state_of(false, true));
    TEST_ASSERT_EQUAL_INT(CYDUI_STATE_DISABLED, cydui_state_of(true, false));
    TEST_ASSERT_EQUAL_INT(CYDUI_STATE_DISABLED, cydui_state_of(true, true));
}

static cydui_response ra, rb;

static void two_raw_widgets(void)
{
    cydui_id ia, ib;

    cydui_ui_begin(&rig.ui, 0);
    ia = cydui_id_str(&rig.ui, "a");
    ib = cydui_id_str(&rig.ui, "b");
    cydui_id_claim(&rig.ui, ia);
    cydui_id_claim(&rig.ui, ib);
    ra = cydui_interact(&rig.ui, ia, (cydui_rect){8, 8, 100, 30}, CYDUI_SENSE_CLICK);
    rb = cydui_interact(&rig.ui, ib, (cydui_rect){8, 60, 100, 30}, CYDUI_SENSE_CLICK);
    cydui_ui_end(&rig.ui);
}

static void test_the_response_says_where_the_pointer_is_to_the_owner_only(void)
{
    start();
    two_raw_widgets();
    TEST_ASSERT_TRUE(ra.x == 0 && ra.y == 0 && rb.x == 0 && rb.y == 0);
    rig_down(&rig, 20, 20);
    two_raw_widgets();
    TEST_ASSERT_EQUAL_INT(20, ra.x);
    TEST_ASSERT_EQUAL_INT(20, ra.y);
    TEST_ASSERT_TRUE(rb.x == 0 && rb.y == 0);
    rig_move(&rig, 25, 22);
    two_raw_widgets();
    TEST_ASSERT_EQUAL_INT(25, ra.x);
    TEST_ASSERT_EQUAL_INT(22, ra.y);
    rig_up(&rig, 27, 23);
    two_raw_widgets();
    TEST_ASSERT_EQUAL_INT(27, ra.x); /* in the frame of the release: where it was let go */
    TEST_ASSERT_EQUAL_INT(23, ra.y);
    TEST_ASSERT_TRUE(ra.released);
    two_raw_widgets();
    TEST_ASSERT_TRUE(ra.x == 0 && ra.y == 0); /* afterwards nobody owns it */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_tap_on_a_star_lights_it_and_the_ones_before_it);
    RUN_TEST(test_a_tap_on_the_last_lit_star_clears_it);
    RUN_TEST(test_it_reports_a_change_only_when_there_is_one);
    RUN_TEST(test_the_edges_of_the_stars_area);
    RUN_TEST(test_a_drag_or_a_release_elsewhere_does_not_count);
    RUN_TEST(test_a_value_out_of_range_is_clamped);
    RUN_TEST(test_a_disabled_rating_does_not_react_and_looks_dimmed);
    RUN_TEST(test_the_stars_take_their_colors_from_the_theme);
    RUN_TEST(test_only_the_visible_part_of_the_label_counts);
    RUN_TEST(test_equal_labels_collide_and_suffixes_tell_them_apart);
    RUN_TEST(test_the_frame_limit_covers_widgets_of_the_application_too);
    RUN_TEST(test_a_label_taller_than_the_stars_makes_the_widget_taller);
    RUN_TEST(test_it_works_in_a_scroll_area);
    RUN_TEST(test_it_works_in_a_card);
    RUN_TEST(test_it_is_placed_like_any_widget);
    RUN_TEST(test_label_len_stops_at_the_suffix_and_at_the_limit);
    RUN_TEST(test_state_of_gives_disabled_priority_over_pressed);
    RUN_TEST(test_the_response_says_where_the_pointer_is_to_the_owner_only);
    return UNITY_END();
}
