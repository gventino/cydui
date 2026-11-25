/*
 * Widgets (FR-30, FR-20): behaviour through synthetic touches, drawing, the per-frame limit, the
 * label rules and collisions (CT-07), determinism (NFR-30).
 *
 * The first widget of a frame sits at (8,8); with the 12 px font and the 32 px touch target a
 * button is 32 tall, so (15,20) is inside the first widget of any kind.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;

#define TAP_X 15
#define TAP_Y 20

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
}

static uint64_t screen_hash(void)
{
    return cydui_mem_hash(&rig.mem);
}

/* ---- buttons ---- */

static bool one_button_frame(void)
{
    bool clicked;
    cydui_ui_begin(&rig.ui, 0);
    clicked = cydui_button(&rig.ui, "Add");
    cydui_ui_end(&rig.ui);
    return clicked;
}

static void test_a_button_returns_true_once_for_a_tap(void)
{
    int clicks = 0, i;

    start();
    TEST_ASSERT_FALSE(one_button_frame());
    rig_down(&rig, TAP_X, TAP_Y);
    TEST_ASSERT_FALSE(one_button_frame()); /* held: not yet */
    rig_up(&rig, TAP_X, TAP_Y);
    for (i = 0; i < 4; i++) {
        clicks += one_button_frame();
    }
    TEST_ASSERT_EQUAL_INT(1, clicks);
}

static void test_a_button_tapped_within_one_frame_clicks(void)
{
    start();
    rig_down(&rig, TAP_X, TAP_Y);
    rig_up(&rig, TAP_X, TAP_Y);
    TEST_ASSERT_TRUE(one_button_frame());
}

static void test_releasing_elsewhere_does_not_click(void)
{
    start();
    rig_down(&rig, TAP_X, TAP_Y);
    one_button_frame();
    rig_move(&rig, 200, 200);
    one_button_frame();
    rig_up(&rig, 200, 200);
    TEST_ASSERT_FALSE(one_button_frame());
}

static bool disabled_button_frame(void)
{
    bool clicked;
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    clicked = cydui_button(&rig.ui, "Add");
    cydui_ui_end(&rig.ui);
    return clicked;
}

static void test_a_disabled_button_never_clicks_and_looks_different(void)
{
    uint64_t enabled, disabled;
    int i;

    start();
    one_button_frame();
    enabled = screen_hash();
    for (i = 0; i < 3; i++) {
        rig_down(&rig, TAP_X, TAP_Y);
        rig_up(&rig, TAP_X, TAP_Y);
        TEST_ASSERT_FALSE(disabled_button_frame());
    }
    disabled = screen_hash();
    TEST_ASSERT_TRUE_MESSAGE(enabled != disabled, "a disabled button looks like an enabled one");
}

static void test_a_pressed_button_looks_pressed(void)
{
    uint64_t rest, held;

    start();
    one_button_frame();
    rest = screen_hash();
    rig_down(&rig, TAP_X, TAP_Y);
    one_button_frame();
    held = screen_hash();
    TEST_ASSERT_TRUE_MESSAGE(rest != held, "no visible feedback while pressing");
    rig_up(&rig, 250, 200); /* let go elsewhere: it goes back to normal */
    one_button_frame();
    one_button_frame();
    TEST_ASSERT_TRUE(screen_hash() == rest);
}

/* ---- check boxes and switches ---- */

static bool checkbox_frame(bool *v)
{
    bool changed;
    cydui_ui_begin(&rig.ui, 0);
    changed = cydui_checkbox(&rig.ui, "Wi-Fi", v);
    cydui_ui_end(&rig.ui);
    return changed;
}

static bool toggle_frame(bool *v)
{
    bool changed;
    cydui_ui_begin(&rig.ui, 0);
    changed = cydui_toggle(&rig.ui, "Wi-Fi", v);
    cydui_ui_end(&rig.ui);
    return changed;
}

static void test_a_checkbox_flips_its_value_once_per_tap(void)
{
    bool v = false;
    int i;

    start();
    for (i = 0; i < 4; i++) {
        bool expect = !v;
        rig_down(&rig, TAP_X, TAP_Y);
        rig_up(&rig, TAP_X, TAP_Y);
        TEST_ASSERT_TRUE(checkbox_frame(&v));
        TEST_ASSERT_EQUAL_INT(expect, v);
        TEST_ASSERT_FALSE_MESSAGE(checkbox_frame(&v), "reported the change a second time");
        TEST_ASSERT_EQUAL_INT(expect, v);
    }
}

static void test_a_toggle_flips_its_value_once_per_tap(void)
{
    bool v = true;

    start();
    rig_down(&rig, TAP_X, TAP_Y);
    rig_up(&rig, TAP_X, TAP_Y);
    TEST_ASSERT_TRUE(toggle_frame(&v));
    TEST_ASSERT_FALSE(v);
    TEST_ASSERT_FALSE(toggle_frame(&v));
    rig_down(&rig, TAP_X, TAP_Y);
    rig_up(&rig, TAP_X, TAP_Y);
    TEST_ASSERT_TRUE(toggle_frame(&v));
    TEST_ASSERT_TRUE(v);
}

static void test_checked_and_unchecked_look_different(void)
{
    bool v = false;
    uint64_t off, on, tog_off, tog_on;

    start();
    checkbox_frame(&v);
    off = screen_hash();
    v = true;
    checkbox_frame(&v);
    on = screen_hash();
    TEST_ASSERT_TRUE(off != on);

    v = false;
    toggle_frame(&v);
    tog_off = screen_hash();
    v = true;
    toggle_frame(&v);
    tog_on = screen_hash();
    TEST_ASSERT_TRUE(tog_off != tog_on);
    TEST_ASSERT_TRUE(off != tog_off);
}

/* Holding a control down changes nothing; letting go does, once. */
static void test_a_held_control_flips_only_when_released(void)
{
    bool v = false;
    int i;

    start();
    rig_down(&rig, TAP_X, TAP_Y);
    for (i = 0; i < 4; i++) {
        TEST_ASSERT_FALSE(checkbox_frame(&v));
        TEST_ASSERT_FALSE_MESSAGE(v, "the value changed while the control was only being held");
    }
    rig_up(&rig, TAP_X, TAP_Y);
    TEST_ASSERT_TRUE(checkbox_frame(&v));
    TEST_ASSERT_TRUE(v);
    TEST_ASSERT_FALSE(checkbox_frame(&v));
    TEST_ASSERT_TRUE(v);

    start();
    v = false;
    rig_down(&rig, TAP_X, TAP_Y);
    for (i = 0; i < 4; i++) {
        TEST_ASSERT_FALSE(toggle_frame(&v));
    }
    TEST_ASSERT_FALSE(v);
    rig_up(&rig, TAP_X, TAP_Y);
    TEST_ASSERT_TRUE(toggle_frame(&v));
    TEST_ASSERT_TRUE(v);
}

/* The switch: a 44 x 24 track at (8,12) with an 18 px knob 3 px inside it, on the left when off. */
static void test_the_toggle_knob_sits_at_the_end_that_matches_the_value(void)
{
    bool v = false;
    const cydui_color track_off = rig_init(&rig) ? 0 : rig.theme.style.border;

    toggle_frame(&v); /* off */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent_text, rig.fb[24 * RIG_W + 20]); /* knob on the left */
    TEST_ASSERT_EQUAL_HEX16(track_off, rig.fb[24 * RIG_W + 40]);             /* bare track on the right */

    v = true;
    toggle_frame(&v); /* on */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent, rig.fb[24 * RIG_W + 20]);      /* bare track on the left */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent_text, rig.fb[24 * RIG_W + 40]); /* knob on the right */
}

static void test_a_disabled_label_is_dimmer(void)
{
    uint64_t normal, dimmed;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_label(&rig.ui, "Hello");
    cydui_ui_end(&rig.ui);
    normal = screen_hash();
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    cydui_label(&rig.ui, "Hello");
    cydui_ui_end(&rig.ui);
    dimmed = screen_hash();
    TEST_ASSERT_TRUE(normal != dimmed);
}

static void test_pressed_controls_look_pressed(void)
{
    bool v = false;
    uint64_t rest, held;

    start();
    checkbox_frame(&v);
    rest = screen_hash();
    rig_down(&rig, TAP_X, TAP_Y);
    checkbox_frame(&v);
    held = screen_hash();
    TEST_ASSERT_TRUE_MESSAGE(rest != held, "checkbox: no feedback while pressing");

    start();
    v = false;
    toggle_frame(&v);
    rest = screen_hash();
    rig_down(&rig, TAP_X, TAP_Y);
    toggle_frame(&v);
    held = screen_hash();
    TEST_ASSERT_TRUE_MESSAGE(rest != held, "toggle: no feedback while pressing");
}

static void test_disabled_controls_do_not_change_their_value(void)
{
    bool v = false;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    cydui_checkbox(&rig.ui, "x", &v);
    cydui_ui_end(&rig.ui);
    rig_down(&rig, TAP_X, TAP_Y);
    rig_up(&rig, TAP_X, TAP_Y);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    TEST_ASSERT_FALSE(cydui_checkbox(&rig.ui, "x", &v));
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE(v);
}

static void test_a_null_value_does_not_crash(void)
{
    start();
    rig_down(&rig, TAP_X, TAP_Y);
    rig_up(&rig, TAP_X, TAP_Y);
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_TRUE(cydui_checkbox(&rig.ui, "x", NULL)); /* reports the tap, has nothing to flip */
    cydui_toggle(&rig.ui, "y", NULL);
    cydui_ui_end(&rig.ui);
}

/* ---- labels, sizes ---- */

/* Labels that fill the whole CYDUI_MAX_STR limit with no NUL after them: nothing may read past it. */
static void test_labels_at_the_length_limit_without_a_nul_are_safe(void)
{
    int kind;

    start();
    for (kind = 0; kind < 2; kind++) {
        char *exact = (char *)malloc(CYDUI_MAX_STR); /* exactly the limit: ASan sees any extra byte */
        bool v = false;

        TEST_ASSERT_NOT_NULL(exact);
        memset(exact, 'x', CYDUI_MAX_STR);
        if (kind) { /* x#x#x#...#: a lone '#' as the very last byte, with no "##" before it */
            int k;
            for (k = 1; k < CYDUI_MAX_STR; k += 2) {
                exact[k] = '#';
            }
            TEST_ASSERT_EQUAL_INT('#', exact[CYDUI_MAX_STR - 1]);
        }
        cydui_ui_begin(&rig.ui, 0);
        cydui_label(&rig.ui, exact);
        cydui_label_wrapped(&rig.ui, exact);
        cydui_push_id_int(&rig.ui, 1);
        cydui_button(&rig.ui, exact);
        cydui_pop_id(&rig.ui);
        cydui_push_id_int(&rig.ui, 2);
        cydui_checkbox(&rig.ui, exact, &v);
        cydui_pop_id(&rig.ui);
        cydui_push_id_int(&rig.ui, 3);
        cydui_toggle(&rig.ui, exact, &v);
        cydui_pop_id(&rig.ui);
        cydui_ui_end(&rig.ui);
        free(exact);
    }
}

static void test_widgets_are_at_least_a_touch_target_tall(void)
{
    cydui_rect after;
    bool v = false;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_button(&rig.ui, "OK");
    cydui_checkbox(&rig.ui, "x", &v);
    cydui_toggle(&rig.ui, "x", &v);
    after = cydui_layout_avail(&rig.ui);
    cydui_ui_end(&rig.ui);
    /* three widgets of 32 and three gaps of 8 below the margin of 8 */
    TEST_ASSERT_EQUAL_INT(8 + 32 + 8 + 32 + 8 + 32 + 8, after.y);
}

static void test_the_text_after_two_hashes_is_not_shown(void)
{
    uint64_t plain, suffixed, other_suffix;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_label(&rig.ui, "Done");
    cydui_ui_end(&rig.ui);
    plain = screen_hash();
    cydui_ui_begin(&rig.ui, 0);
    cydui_label(&rig.ui, "Done##3");
    cydui_ui_end(&rig.ui);
    suffixed = screen_hash();
    cydui_ui_begin(&rig.ui, 0);
    cydui_label(&rig.ui, "Done##completely different");
    cydui_ui_end(&rig.ui);
    other_suffix = screen_hash();
    TEST_ASSERT_TRUE(plain == suffixed);
    TEST_ASSERT_TRUE(plain == other_suffix);

    cydui_ui_begin(&rig.ui, 0); /* a button too, and one that is only a suffix */
    cydui_button(&rig.ui, "##only");
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_a_long_label_does_not_leave_its_widget(void)
{
    int x, y, stray = 0;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_width(&rig.ui, 40);
    cydui_button(&rig.ui, "A label far too long to fit in forty pixels");
    cydui_ui_end(&rig.ui);
    for (y = 0; y < RIG_H; y++) {
        for (x = 8 + 40; x < RIG_W; x++) {
            stray += rig.fb[y * RIG_W + x] != 0;
        }
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, stray, "text spilled out of the button");
}

static void test_a_wrapped_label_wraps_at_the_room_it_has(void)
{
    cydui_rect after_wide, after_narrow;
    static const char text[] = "A text that needs several lines when the box is narrow";

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_label_wrapped(&rig.ui, text);
    after_wide = cydui_layout_avail(&rig.ui);
    cydui_ui_end(&rig.ui);

    cydui_ui_begin(&rig.ui, 0);
    cydui_next_width(&rig.ui, 90);
    cydui_begin_column(&rig.ui);
    cydui_label_wrapped(&rig.ui, text);
    cydui_end_column(&rig.ui);
    after_narrow = cydui_layout_avail(&rig.ui);
    cydui_ui_end(&rig.ui);

    TEST_ASSERT_TRUE_MESSAGE(after_narrow.y > after_wide.y + 14, "the narrow box did not wrap");
}

static void test_null_texts_are_empty(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_label(&rig.ui, NULL);
    cydui_label_wrapped(&rig.ui, NULL);
    cydui_push_id_int(&rig.ui, 1); /* empty labels are all the same id: tell them apart as an app would */
    cydui_button(&rig.ui, NULL);
    cydui_pop_id(&rig.ui);
    cydui_push_id_int(&rig.ui, 2);
    cydui_checkbox(&rig.ui, NULL, NULL);
    cydui_pop_id(&rig.ui);
    cydui_push_id_int(&rig.ui, 3);
    cydui_toggle(&rig.ui, NULL, NULL);
    cydui_pop_id(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

/* ---- ids and lists (CT-07) ---- */

static void test_two_buttons_with_the_same_label_collide_in_debug_builds(void)
{
    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_button(&rig.ui, "Done");
    cydui_button(&rig.ui, "Done");
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_ID_COLLISION);
}

static void test_hash_suffixes_and_push_id_keep_a_list_apart(void)
{
    bool a[3] = {false, false, false};
    bool b[3] = {false, false, false};
    int i;
    char label[24];

    start();
    cydui_ui_begin(&rig.ui, 0);
    for (i = 0; i < 3; i++) { /* the "##" way */
        snprintf(label, sizeof(label), "Done##%d", i);
        cydui_checkbox(&rig.ui, label, &a[i]);
    }
    for (i = 0; i < 3; i++) { /* the push_id way */
        cydui_push_id_int(&rig.ui, i);
        cydui_checkbox(&rig.ui, "Done", &b[i]);
        cydui_pop_id(&rig.ui);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

/* Tapping the middle row of a list must change that row only, and keep doing so. */
static void list_frame(bool *v)
{
    int i;
    cydui_ui_begin(&rig.ui, 0);
    for (i = 0; i < 3; i++) {
        cydui_push_id_int(&rig.ui, i);
        cydui_checkbox(&rig.ui, "Done", &v[i]);
        cydui_pop_id(&rig.ui);
    }
    cydui_ui_end(&rig.ui);
}

static void test_tapping_one_row_of_a_list_changes_only_that_row(void)
{
    bool v[3] = {false, false, false};

    start();
    list_frame(v);
    rig_down(&rig, TAP_X, 8 + 40 + 16); /* the second row: 8 + (32 + 8) .. 8 + 2 * 32 + 8 */
    rig_up(&rig, TAP_X, 8 + 40 + 16);
    list_frame(v);
    TEST_ASSERT_FALSE(v[0]);
    TEST_ASSERT_TRUE(v[1]);
    TEST_ASSERT_FALSE(v[2]);
    list_frame(v);
    TEST_ASSERT_TRUE(v[1]);
}

/* ---- the limit per frame (AB-07) ---- */

static void test_widgets_beyond_the_limit_are_ignored_and_flagged(void)
{
    int i, drawn_before;
    char label[16];

    start();
    cydui_ui_begin(&rig.ui, 0);
    for (i = 0; i < CYDUI_MAX_WIDGETS; i++) {
        cydui_label(&rig.ui, "x");
    }
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui) & CYDUI_ERR_TOO_MANY_WIDGETS); /* exactly the limit is fine */
    cydui_label(&rig.ui, "one too many");
    TEST_ASSERT_TRUE_MESSAGE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_TOO_MANY_WIDGETS,
                             "the first widget past the limit was accepted");
    cydui_ui_clear_errors(&rig.ui);
    drawn_before = (int)cydui_cmd_count(&rig.ui.ctx);
    for (i = 0; i < 5000; i++) {
        snprintf(label, sizeof(label), "w%d", i);
        cydui_label(&rig.ui, label);
        cydui_button(&rig.ui, label);
        cydui_next_width(&rig.ui, 5); /* modifiers set on an ignored call must not leak */
    }
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_TOO_MANY_WIDGETS);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(CYDUI_MAX_WIDGETS, rig.ui.widgets, "counted a widget beyond the limit");
    TEST_ASSERT_EQUAL_INT_MESSAGE(drawn_before, (int)cydui_cmd_count(&rig.ui.ctx),
                                  "ignored widgets still recorded drawing commands");
    cydui_ui_end(&rig.ui);

    cydui_ui_clear_errors(&rig.ui); /* and the next frame starts over */
    cydui_ui_begin(&rig.ui, 0);
    cydui_label(&rig.ui, "again");
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui) & CYDUI_ERR_TOO_MANY_WIDGETS);
}

/* ---- fill with real widgets ---- */

static void fill_frame(void)
{
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_row(&rig.ui);
    cydui_label(&rig.ui, "left");
    cydui_spacer_fill(&rig.ui);
    cydui_button(&rig.ui, "right");
    cydui_end_row(&rig.ui);
    cydui_ui_end(&rig.ui);
}

static void test_a_row_with_a_fill_spacer_pushes_a_button_to_the_right_edge(void)
{
    int i, x, y, rightmost = 0;

    start();
    for (i = 0; i < 4; i++) {
        fill_frame();
    }
    for (y = 8; y < 40; y++) { /* the button is the only thing drawn in the accent colour */
        for (x = 0; x < RIG_W; x++) {
            if (rig.fb[y * RIG_W + x] == rig.theme.style.accent && x > rightmost) {
                rightmost = x;
            }
        }
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(8 + 304 - 1, rightmost, "the button is not flush with the right margin");
}

/* ---- determinism (NFR-30) ---- */

static uint64_t session(uirig *r)
{
    bool a = false, b = true;
    int frame, clicks = 0;

    for (frame = 0; frame < 12; frame++) {
        if (frame == 2) rig_down(r, TAP_X, TAP_Y);
        if (frame == 3) rig_up(r, TAP_X, TAP_Y);
        if (frame == 5) { rig_down(r, 30, 60); rig_up(r, 30, 60); }
        if (frame == 7) { rig_down(r, 30, 100); rig_move(r, 80, 100); rig_up(r, 80, 100); }
        cydui_ui_begin(&r->ui, 0);
        clicks += cydui_button(&r->ui, "Add");
        cydui_checkbox(&r->ui, "Wi-Fi", &a);
        cydui_toggle(&r->ui, "Dark", &b);
        CYDUI_ROW(&r->ui) {
            cydui_label(&r->ui, "clicks");
            cydui_spacer_fill(&r->ui);
            cydui_button(&r->ui, "End");
        }
        cydui_ui_end(&r->ui);
        r->mem.now += 16;
    }
    return cydui_mem_hash(&r->mem) ^ (uint64_t)clicks ^ ((uint64_t)a << 8) ^ ((uint64_t)b << 9);
}

static void test_the_same_touches_give_the_same_screen_every_time(void)
{
    static uirig other;
    uint64_t first, second;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&other));
    first = session(&rig);
    second = session(&other);
    TEST_ASSERT_TRUE(first == second);
    TEST_ASSERT_TRUE(first != 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_button_returns_true_once_for_a_tap);
    RUN_TEST(test_a_button_tapped_within_one_frame_clicks);
    RUN_TEST(test_releasing_elsewhere_does_not_click);
    RUN_TEST(test_a_disabled_button_never_clicks_and_looks_different);
    RUN_TEST(test_a_pressed_button_looks_pressed);
    RUN_TEST(test_a_checkbox_flips_its_value_once_per_tap);
    RUN_TEST(test_a_toggle_flips_its_value_once_per_tap);
    RUN_TEST(test_checked_and_unchecked_look_different);
    RUN_TEST(test_a_held_control_flips_only_when_released);
    RUN_TEST(test_the_toggle_knob_sits_at_the_end_that_matches_the_value);
    RUN_TEST(test_a_disabled_label_is_dimmer);
    RUN_TEST(test_pressed_controls_look_pressed);
    RUN_TEST(test_disabled_controls_do_not_change_their_value);
    RUN_TEST(test_a_null_value_does_not_crash);
    RUN_TEST(test_labels_at_the_length_limit_without_a_nul_are_safe);
    RUN_TEST(test_widgets_are_at_least_a_touch_target_tall);
    RUN_TEST(test_the_text_after_two_hashes_is_not_shown);
    RUN_TEST(test_a_long_label_does_not_leave_its_widget);
    RUN_TEST(test_a_wrapped_label_wraps_at_the_room_it_has);
    RUN_TEST(test_null_texts_are_empty);
    RUN_TEST(test_two_buttons_with_the_same_label_collide_in_debug_builds);
    RUN_TEST(test_hash_suffixes_and_push_id_keep_a_list_apart);
    RUN_TEST(test_tapping_one_row_of_a_list_changes_only_that_row);
    RUN_TEST(test_widgets_beyond_the_limit_are_ignored_and_flagged);
    RUN_TEST(test_a_row_with_a_fill_spacer_pushes_a_button_to_the_right_edge);
    RUN_TEST(test_the_same_touches_give_the_same_screen_every_time);
    return UNITY_END();
}
