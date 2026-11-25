/*
 * The keyboard on the screen (FR-34): it shows while a text field has the focus, its keys type into the field, Shift and
 * the pages work, Backspace repeats while held, and nothing under it reacts. Keys are tapped at their centers, worked out
 * with the same arithmetic as the keyboard: the keyboard is 143 px tall at the bottom of the 320 x 240 screen, rows are
 * 32 px tall 3 px apart, and a key of weight `w` out of a row total `t` is as wide as its share of what is left after
 * the 3 px margins and the 3 px gaps.
 */

#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define KB_TOP 97
#define ROW_Y(r) (KB_TOP + 3 + (r) * 35 + 16) /* the middle of row r */

static uirig rig;
static char buf[64];
static size_t cap;
static unsigned flags_;
static bool under_clicked;
static unsigned result;
static bool tall_button; /* a big button under the keyboard that must not react */
static bool area_mode;

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    memset(buf, 0, sizeof(buf));
    cap = sizeof(buf);
    flags_ = 0;
    under_clicked = false;
    tall_button = false;
    area_mode = false;
}

static void frame(void)
{
    cydui_ui_begin(&rig.ui, 0);
    if (area_mode) result |= cydui_text_area(&rig.ui, "Msg", buf, cap, 0, 2, 0);
    else result |= cydui_text_input(&rig.ui, "Msg", buf, cap, 0, flags_);
    if (tall_button) {
        cydui_next_stretch(&rig.ui);
        cydui_next_height(&rig.ui, 150);
        if (cydui_button(&rig.ui, "Under")) under_clicked = true;
    }
    cydui_ui_end(&rig.ui);
}

static void frames(int n)
{
    int i;
    for (i = 0; i < n; i++) frame();
}

static void tap(int x, int y)
{
    rig_down(&rig, x, y);
    frame();
    rig_up(&rig, x, y);
    frames(3);
}

/* The x of the middle of key `i` of a row of keys with the given weights. */
static int key_x(const int *weights, int n, int i)
{
    int total = 0, cum = 0, k, avail, x0, x1;

    for (k = 0; k < n; k++) total += weights[k];
    avail = 320 - 6 - (n - 1) * 3;
    for (k = 0; k < i; k++) cum += weights[k];
    x0 = 3 + cum * avail / total + i * 3;
    cum += weights[i];
    x1 = 3 + cum * avail / total + i * 3;
    return (x0 + x1) / 2;
}

static const int W10[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
static const int W9[9] = {2, 2, 2, 2, 2, 2, 2, 2, 2};
static const int W_ROW2[9] = {3, 2, 2, 2, 2, 2, 2, 2, 3}; /* Shift, 7 letters, Backspace */
static const int W_ACC2[7] = {3, 2, 2, 2, 2, 2, 3};  /* the accents page: Shift, 5 letters, Backspace */
static const int W_BOTTOM[5] = {3, 3, 10, 4, 3};          /* 123, accents, space, Ok, hide */

static void tap_row0(int i) { tap(key_x(W10, 10, i), ROW_Y(0)); }
static void tap_row1(int i) { tap(key_x(W9, 9, i), ROW_Y(1)); }
static void tap_row2(int i) { tap(key_x(W_ROW2, 9, i), ROW_Y(2)); }
static void tap_acc_row2(int i) { tap(key_x(W_ACC2, 7, i), ROW_Y(2)); }
static void tap_bottom(int i) { tap(key_x(W_BOTTOM, 5, i), ROW_Y(3)); }

static void focus_field(void)
{
    frames(2);
    tap(15, 20);
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
}

static void test_it_shows_with_the_focus_and_not_without(void)
{
    start();
    frames(3);
    TEST_ASSERT_EQUAL_INT(240, cydui_keyboard_top(&rig.ui));
    focus_field();
    TEST_ASSERT_EQUAL_INT(KB_TOP, cydui_keyboard_top(&rig.ui));
    rig_key(&rig, CYDUI_KEY_ESCAPE);
    frames(2);
    TEST_ASSERT_EQUAL_INT(240, cydui_keyboard_top(&rig.ui));
}

static void test_the_keys_type_into_the_field(void)
{
    start();
    focus_field();
    tap_row1(5); /* h */
    tap_row0(2); /* e */
    tap_row1(8); /* l */
    tap_row1(8); /* l */
    tap_row0(8); /* o */
    TEST_ASSERT_EQUAL_STRING("hello", buf);
    tap_bottom(2); /* space */
    TEST_ASSERT_EQUAL_STRING("hello ", buf);
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui)); /* the press on the keyboard kept the focus */
}

static void test_shift_is_for_one_character(void)
{
    start();
    focus_field();
    tap_row2(0); /* Shift */
    tap_row1(0); /* A */
    tap_row1(0); /* a */
    TEST_ASSERT_EQUAL_STRING("Aa", buf);
    tap_row2(0);
    tap_row2(0); /* on and off again */
    tap_row1(0);
    TEST_ASSERT_EQUAL_STRING("Aaa", buf);
}

static void test_the_pages(void)
{
    start();
    focus_field();
    tap_bottom(0); /* 123 */
    tap_row0(4);   /* 5 */
    tap_row2(0);   /* the symbols (#+=) */
    tap_row0(0);   /* [ */
    tap_row2(0);   /* back to the numbers (123) */
    tap_row0(9);   /* 0 */
    tap_bottom(0); /* abc */
    tap_row0(0);   /* q */
    TEST_ASSERT_EQUAL_STRING("5[0q", buf);
}

static void test_changing_the_page_turns_shift_off(void)
{
    start();
    focus_field();
    tap_row2(0);   /* Shift on */
    tap_bottom(0); /* 123 */
    tap_bottom(0); /* abc */
    tap_row0(0);
    TEST_ASSERT_EQUAL_STRING("q", buf); /* not Q */
}

static void test_the_accented_letters_and_shift_on_them(void)
{
    start();
    focus_field();
    tap_bottom(1); /* the accents page */
    tap_row0(0);   /* á */
    tap_acc_row2(0); /* Shift */
    tap_row0(0);   /* Á */
    tap_acc_row2(3); /* ç: the third letter after Shift, which has gone back off */
    TEST_ASSERT_EQUAL_STRING("\xC3\xA1\xC3\x81\xC3\xA7", buf);
    tap_bottom(1); /* the same place is "abc" here: back to the letters */
    tap_row0(0);
    TEST_ASSERT_EQUAL_STRING("\xC3\xA1\xC3\x81\xC3\xA7q", buf);
}

static void test_backspace_works_on_the_press_and_repeats_while_held(void)
{
    int bx = key_x(W_ROW2, 9, 8), i;

    start();
    focus_field();
    strcpy(buf, "abcdefghij");
    tap(300, 20); /* the cursor at the end */
    TEST_ASSERT_EQUAL_STRING("abcdefghij", buf);
    tap(bx, ROW_Y(2));
    TEST_ASSERT_EQUAL_STRING("abcdefghi", buf); /* one per tap */

    rig_down(&rig, bx, ROW_Y(2));
    frames(3); /* the press: one deleted at once */
    TEST_ASSERT_EQUAL_STRING("abcdefgh", buf);
    rig_advance(&rig, 300);
    frames(3);
    TEST_ASSERT_EQUAL_STRING("abcdefgh", buf); /* not yet: the delay is 500 ms */
    rig_advance(&rig, 250); /* 550 ms held: the first repeat */
    frames(3);
    TEST_ASSERT_EQUAL_STRING("abcdefg", buf);
    rig_advance(&rig, 170); /* 720 ms: the repeats are due at 500, 580 and 660: three in all */
    frames(3);
    TEST_ASSERT_EQUAL_STRING("abcde", buf);
    rig_up(&rig, bx, ROW_Y(2));
    frames(3);
    TEST_ASSERT_EQUAL_STRING("abcde", buf); /* the release deletes nothing more */
    for (i = 0; i < 3; i++) rig_advance(&rig, 500);
    frames(3);
    TEST_ASSERT_EQUAL_STRING("abcde", buf);
}

static void test_ok_confirms_a_single_line_field_and_closes_the_keyboard(void)
{
    start();
    focus_field();
    tap_row0(0);
    result = 0;
    tap_bottom(3); /* Ok */
    TEST_ASSERT_TRUE((result & CYDUI_TEXT_CONFIRMED) != 0);
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
    TEST_ASSERT_EQUAL_INT(240, cydui_keyboard_top(&rig.ui));
    TEST_ASSERT_EQUAL_STRING("q", buf);
}

static void test_ok_in_a_text_area_is_a_newline_and_the_hide_key_closes_it(void)
{
    start();
    area_mode = true;
    focus_field();
    tap_row0(0);
    tap_bottom(3);
    tap_row0(0);
    TEST_ASSERT_EQUAL_STRING("q\nq", buf);
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
    tap_bottom(4); /* hide */
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
    TEST_ASSERT_EQUAL_STRING("q\nq", buf);
}

static void test_nothing_under_the_keyboard_reacts_and_the_press_does_not_drop_the_focus(void)
{
    start();
    tall_button = true; /* from y = 48 to 198: its lower part is under the keyboard */
    focus_field();
    tap_row0(0);
    TEST_ASSERT_FALSE(under_clicked);
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
    tap(160, 150); /* the gap between two rows? that is a key or the keyboard's own background */
    TEST_ASSERT_FALSE(under_clicked);
    tap(160, 70); /* above the keyboard: the button does react, and the focus is dropped */
    TEST_ASSERT_TRUE(under_clicked);
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
}

static void test_the_application_can_turn_it_off(void)
{
    start();
    cydui_keyboard_enable(&rig.ui, false);
    focus_field();
    TEST_ASSERT_EQUAL_INT(240, cydui_keyboard_top(&rig.ui));
    cydui_keyboard_enable(&rig.ui, true);
    frames(2);
    TEST_ASSERT_EQUAL_INT(KB_TOP, cydui_keyboard_top(&rig.ui));
}

static void test_it_works_over_a_modal_popup(void)
{
    bool open = true;
    int i;

    start();
    for (i = 0; i < 4; i++) {
        cydui_ui_begin(&rig.ui, 0);
        if (cydui_popup_begin(&rig.ui, "P", &open, 220, 80, CYDUI_POPUP_MODAL)) {
            cydui_text_input(&rig.ui, "Msg", buf, cap, 0, 0);
            cydui_popup_end(&rig.ui);
        }
        cydui_ui_end(&rig.ui);
        if (i == 1) {
            rig_down(&rig, 80, 100);
            rig_up(&rig, 80, 100);
        }
    }
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
    rig_down(&rig, key_x(W10, 10, 0), ROW_Y(0));
    rig_up(&rig, key_x(W10, 10, 0), ROW_Y(0));
    for (i = 0; i < 5; i++) {
        cydui_ui_begin(&rig.ui, 0);
        if (cydui_popup_begin(&rig.ui, "P", &open, 220, 80, CYDUI_POPUP_MODAL)) {
            cydui_text_input(&rig.ui, "Msg", buf, cap, 0, 0);
            cydui_popup_end(&rig.ui);
        }
        cydui_ui_end(&rig.ui);
    }
    TEST_ASSERT_EQUAL_STRING("q", buf);
    TEST_ASSERT_TRUE(open);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_a_field_with_a_limit_refuses_what_the_keys_type(void)
{
    start();
    cap = 3; /* two characters */
    focus_field();
    tap_row0(0);
    tap_row0(1);
    tap_row0(2);
    TEST_ASSERT_EQUAL_STRING("qw", buf);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_it_shows_with_the_focus_and_not_without);
    RUN_TEST(test_the_keys_type_into_the_field);
    RUN_TEST(test_shift_is_for_one_character);
    RUN_TEST(test_the_pages);
    RUN_TEST(test_changing_the_page_turns_shift_off);
    RUN_TEST(test_the_accented_letters_and_shift_on_them);
    RUN_TEST(test_backspace_works_on_the_press_and_repeats_while_held);
    RUN_TEST(test_ok_confirms_a_single_line_field_and_closes_the_keyboard);
    RUN_TEST(test_ok_in_a_text_area_is_a_newline_and_the_hide_key_closes_it);
    RUN_TEST(test_nothing_under_the_keyboard_reacts_and_the_press_does_not_drop_the_focus);
    RUN_TEST(test_the_application_can_turn_it_off);
    RUN_TEST(test_it_works_over_a_modal_popup);
    RUN_TEST(test_a_field_with_a_limit_refuses_what_the_keys_type);
    return UNITY_END();
}
