/*
 * The single line text field (FR-34): focus, typing, the editing keys, taps that place the cursor, the limits, the
 * password mode, scrolling and the dropping of the focus. Through synthetic touches and keys on the rig.
 *
 * The first widget of a frame sits at (8,8); with the 12 px font and the 32 px touch target a field is 32 tall and as
 * wide as the screen less the margins, so (15,20) is inside it; its text starts at x = 16.
 */

#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;
static char buf[32];
static size_t cap, max_chars;
static unsigned flags;
static const char *label;
static bool more;

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    memset(buf, 0, sizeof(buf));
    cap = sizeof(buf);
    max_chars = 0;
    flags = 0;
    label = "Name";
}

/* One frame with the field and, below it, a button that is always the second widget. */
static unsigned frame(void)
{
    unsigned r;

    cydui_ui_begin(&rig.ui, 0);
    r = cydui_text_input(&rig.ui, label, buf, cap, max_chars, flags);
    cydui_button(&rig.ui, "Other");
    more = cydui_ui_end(&rig.ui);
    return r;
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
    frame();
    frame();
}

static void focus_the_field(void)
{
    frames(2);
    tap(15, 20);
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
}

static void type(const char *s)
{
    for (; *s; s++) rig_char(&rig, (uint32_t)(unsigned char)*s);
    frame();
}

static uint64_t hash(void)
{
    return cydui_mem_hash(&rig.mem);
}

static cydui_state *field_state(void)
{
    return cydui_state_get(&rig.ui, cydui_id_str(&rig.ui, label), false);
}

static void test_a_tap_gives_the_focus_and_typing_edits(void)
{
    start();
    frames(2);
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
    type("x"); /* no focus: the key goes nowhere */
    TEST_ASSERT_EQUAL_STRING("", buf);
    tap(15, 20);
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
    rig_char(&rig, 'a');
    rig_char(&rig, 'b');
    TEST_ASSERT_EQUAL_UINT(CYDUI_TEXT_CHANGED, frame());
    TEST_ASSERT_EQUAL_STRING("ab", buf);
    TEST_ASSERT_EQUAL_UINT(0, frame()); /* nothing changed since */
}

static void test_keys_typed_before_the_focus_are_not_kept_for_later(void)
{
    start();
    frames(2);
    type("zzz");
    tap(15, 20);
    frames(2);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

static void test_the_editing_keys(void)
{
    start();
    focus_the_field();
    type("abcd");
    rig_key(&rig, CYDUI_KEY_LEFT);
    rig_key(&rig, CYDUI_KEY_LEFT);
    rig_char(&rig, 'X');
    frame();
    TEST_ASSERT_EQUAL_STRING("abXcd", buf);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    frame();
    TEST_ASSERT_EQUAL_STRING("abcd", buf);
    rig_key(&rig, CYDUI_KEY_DELETE);
    frame();
    TEST_ASSERT_EQUAL_STRING("abd", buf);
    rig_key(&rig, CYDUI_KEY_HOME);
    rig_key(&rig, CYDUI_KEY_DELETE);
    frame();
    TEST_ASSERT_EQUAL_STRING("bd", buf);
    rig_key(&rig, CYDUI_KEY_END);
    rig_key(&rig, CYDUI_KEY_RIGHT); /* already at the end */
    rig_char(&rig, '!');
    frame();
    TEST_ASSERT_EQUAL_STRING("bd!", buf);
    rig_key(&rig, CYDUI_KEY_UP); /* for the text area: nothing here */
    rig_key(&rig, CYDUI_KEY_DOWN);
    TEST_ASSERT_EQUAL_UINT(0, frame());
    TEST_ASSERT_EQUAL_STRING("bd!", buf);
}

static void test_several_keys_in_one_frame_apply_in_order(void)
{
    start();
    focus_the_field();
    rig_char(&rig, 'a');
    rig_char(&rig, 'b');
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    rig_char(&rig, 'c');
    frame();
    TEST_ASSERT_EQUAL_STRING("ac", buf);
}

static void test_enter_confirms_and_gives_up_the_focus_and_escape_only_the_focus(void)
{
    start();
    focus_the_field();
    type("ok");
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_char(&rig, 'n'); /* after the Enter: not for this field */
    TEST_ASSERT_EQUAL_UINT(CYDUI_TEXT_CONFIRMED, frame());
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
    TEST_ASSERT_EQUAL_STRING("ok", buf);

    tap(15, 20);
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
    rig_key(&rig, CYDUI_KEY_ESCAPE);
    TEST_ASSERT_EQUAL_UINT(0, frame());
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
}

static void test_a_press_elsewhere_drops_the_focus_but_a_press_on_the_field_keeps_it(void)
{
    start();
    focus_the_field();
    tap(15, 20); /* on the field again */
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
    tap(300, 200); /* empty space */
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
    tap(15, 20);
    tap(15, 56); /* the button below */
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
}

static void test_the_focus_is_dropped_when_the_field_is_not_called(void)
{
    start();
    focus_the_field();
    cydui_ui_begin(&rig.ui, 0);
    cydui_button(&rig.ui, "Other");
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
}

static void test_focus_set_and_clear(void)
{
    start();
    frames(2);
    cydui_ui_begin(&rig.ui, 0);
    cydui_focus_set(&rig.ui, "Name");
    cydui_text_input(&rig.ui, label, buf, cap, 0, 0);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
    type("q");
    TEST_ASSERT_EQUAL_STRING("q", buf);
    cydui_focus_clear(&rig.ui);
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
}

static void test_a_tap_puts_the_cursor_between_the_characters(void)
{
    const cydui_font *f;
    int w5;

    start();
    strcpy(buf, "hello world");
    frames(2);
    f = rig.theme.style.font;
    w5 = cydui_text_size(f, "hello", 5, 0).w;
    tap(16 + w5 + 1, 20); /* just after "hello", nearer to it than to the next boundary */
    type("!");
    TEST_ASSERT_EQUAL_STRING("hello! world", buf);
    tap(16 - 6, 20); /* left of everything */
    type(">");
    TEST_ASSERT_EQUAL_STRING(">hello! world", buf);
    tap(300, 20); /* beyond the text */
    type("<");
    TEST_ASSERT_EQUAL_STRING(">hello! world<", buf);
}

static void test_a_space_moves_the_cursor_like_any_other_character(void)
{
    uint64_t without, with_space;

    start();
    focus_the_field();
    type("ab");
    frames(2);
    without = hash();
    type(" "); /* the width of a text does not count trailing spaces; the cursor has to */
    frames(2);
    with_space = hash();
    TEST_ASSERT_TRUE(without != with_space);
    TEST_ASSERT_EQUAL_STRING("ab ", buf);
}

static void test_multibyte_characters(void)
{
    start();
    focus_the_field();
    rig_char(&rig, 0xE1);
    rig_char(&rig, 0x20AC);
    rig_char(&rig, 0x1F600); /* the font has no glyph for it: the fallback is drawn, the text is kept */
    frame();
    TEST_ASSERT_EQUAL_STRING("\xC3\xA1\xE2\x82\xAC\xF0\x9F\x98\x80", buf);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    rig_key(&rig, CYDUI_KEY_LEFT);
    rig_key(&rig, CYDUI_KEY_DELETE);
    frame();
    TEST_ASSERT_EQUAL_STRING("\xC3\xA1", buf);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_the_limit_of_characters_outlines_the_field_for_a_moment(void)
{
    uint64_t before, during, after;

    start();
    max_chars = 3;
    focus_the_field();
    type("abc");
    frames(2);
    before = hash();
    rig_char(&rig, 'd');
    TEST_ASSERT_EQUAL_UINT(0, frame());
    TEST_ASSERT_EQUAL_STRING("abc", buf);
    TEST_ASSERT_TRUE(more); /* another frame is wanted, to see the end of it */
    during = hash();
    TEST_ASSERT_TRUE(before != during);
    rig_advance(&rig, 100);
    frame();
    TEST_ASSERT_TRUE(more);
    TEST_ASSERT_EQUAL_UINT64(during, hash());
    rig_advance(&rig, 500);
    frame();
    frame();
    after = hash();
    TEST_ASSERT_EQUAL_UINT64(before, after);
    TEST_ASSERT_FALSE(more);
}

static void test_no_room_in_the_buffer_is_refused_the_same_way(void)
{
    uint64_t before;

    start();
    cap = 4; /* three characters and the NUL */
    focus_the_field();
    type("abcd");
    TEST_ASSERT_EQUAL_STRING("abc", buf);
    before = hash();
    frame();
    TEST_ASSERT_TRUE(more);
    rig_advance(&rig, 600);
    frames(2);
    TEST_ASSERT_TRUE(before != hash()); /* the outline is gone */
}

static void test_a_password_draws_bullets_and_never_the_text(void)
{
    static char other[32];
    uint64_t secret_a, secret_b, bullets;

    start();
    flags = CYDUI_TEXT_PASSWORD;
    focus_the_field();
    type("abc");
    frames(2);
    TEST_ASSERT_EQUAL_STRING("abc", buf);
    secret_a = hash();

    start();
    flags = CYDUI_TEXT_PASSWORD;
    focus_the_field();
    type("xyz");
    frames(2);
    secret_b = hash();
    TEST_ASSERT_EQUAL_UINT64(secret_a, secret_b);

    start(); /* a plain field that holds three real bullets looks the same */
    strcpy(other, "\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2");
    focus_the_field();
    tap(300, 20);
    strcpy(buf, other);
    tap(300, 20);
    frames(2);
    bullets = hash();
    TEST_ASSERT_EQUAL_UINT64(secret_a, bullets);
}

static void test_a_long_text_scrolls_to_keep_the_cursor_in_view(void)
{
    cydui_state *s;
    int i;

    start();
    focus_the_field();
    for (i = 0; i < 28; i++) rig_char(&rig, 'W');
    frame();
    frames(2);
    s = field_state();
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_TRUE(s->b > 0); /* 28 Ws are wider than the 288 px of text room */
    TEST_ASSERT_EQUAL_INT(28, (int)s->a);
    rig_key(&rig, CYDUI_KEY_HOME);
    frame();
    TEST_ASSERT_EQUAL_INT(0, s->b);
    rig_key(&rig, CYDUI_KEY_END);
    frame();
    TEST_ASSERT_TRUE(s->b > 0);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_a_text_longer_than_one_text_command_is_still_shown_at_the_end(void)
{
    int i;
    uint64_t a;

    start();
    cap = sizeof(buf);
    focus_the_field();
    for (i = 0; i < 30; i++) rig_char(&rig, 'i');
    frames(2);
    a = hash();
    TEST_ASSERT_EQUAL_UINT(30, strlen(buf));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_TRUE(a != 0);
}

static void test_the_hint_shows_while_it_is_empty(void)
{
    uint64_t with_hint, no_hint, typed, typed_no_hint;

    start();
    label = "Name";
    frames(3);
    with_hint = hash();
    start();
    label = "##x";
    frames(3);
    no_hint = hash();
    TEST_ASSERT_TRUE(with_hint != no_hint);

    start();
    label = "Name";
    focus_the_field();
    type("a");
    frames(2);
    typed = hash();
    start();
    label = "##x";
    focus_the_field();
    type("a");
    frames(2);
    typed_no_hint = hash();
    TEST_ASSERT_EQUAL_UINT64(typed, typed_no_hint);
}

static void test_a_disabled_field_and_a_missing_buffer_do_nothing(void)
{
    start();
    frames(2);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    cydui_text_input(&rig.ui, label, buf, cap, 0, 0);
    cydui_ui_end(&rig.ui);
    rig_down(&rig, 15, 20);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    cydui_text_input(&rig.ui, label, buf, cap, 0, 0);
    cydui_ui_end(&rig.ui);
    rig_up(&rig, 15, 20);
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    cydui_text_input(&rig.ui, label, buf, cap, 0, 0);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));

    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_EQUAL_UINT(0, cydui_text_input(&rig.ui, label, NULL, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT(0, cydui_text_input(&rig.ui, "B", buf, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT(0, cydui_text_input(&rig.ui, NULL, buf, cap, 0, 0)); /* a NULL label is an empty one */
    cydui_ui_end(&rig.ui);
    tap(15, 20);
    type("x");
    TEST_ASSERT_EQUAL_STRING("x", buf);
}

static void test_two_fields_the_focus_goes_to_the_one_tapped(void)
{
    static char other[16];

    start();
    frames(2);
    memset(other, 0, sizeof(other));
    rig_down(&rig, 15, 20);
    rig_up(&rig, 15, 20);
    for (int i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_text_input(&rig.ui, "One", buf, cap, 0, 0);
        cydui_text_input(&rig.ui, "Two", other, sizeof(other), 0, 0);
        cydui_ui_end(&rig.ui);
    }
    rig_char(&rig, '1');
    cydui_ui_begin(&rig.ui, 0);
    cydui_text_input(&rig.ui, "One", buf, cap, 0, 0);
    cydui_text_input(&rig.ui, "Two", other, sizeof(other), 0, 0);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_STRING("1", buf);
    TEST_ASSERT_EQUAL_STRING("", other);

    rig_down(&rig, 15, 20 + 40); /* the second field: 32 tall and 8 of spacing */
    rig_up(&rig, 15, 20 + 40);
    for (int i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_text_input(&rig.ui, "One", buf, cap, 0, 0);
        cydui_text_input(&rig.ui, "Two", other, sizeof(other), 0, 0);
        cydui_ui_end(&rig.ui);
    }
    rig_char(&rig, '2');
    cydui_ui_begin(&rig.ui, 0);
    cydui_text_input(&rig.ui, "One", buf, cap, 0, 0);
    cydui_text_input(&rig.ui, "Two", other, sizeof(other), 0, 0);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_STRING("1", buf);
    TEST_ASSERT_EQUAL_STRING("2", other);
}

static void test_a_key_is_taken_by_one_field_only_even_when_the_focus_moves_in_the_same_frame(void)
{
    static char other[16];
    int i;

    start();
    memset(other, 0, sizeof(other));
    frames(2);
    for (i = 0; i < 2; i++) { /* focus the first field */
        cydui_ui_begin(&rig.ui, 0);
        cydui_text_input(&rig.ui, "One", buf, cap, 0, 0);
        cydui_text_input(&rig.ui, "Two", other, sizeof(other), 0, 0);
        cydui_ui_end(&rig.ui);
    }
    rig_down(&rig, 15, 20);
    rig_up(&rig, 15, 20);
    for (i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_text_input(&rig.ui, "One", buf, cap, 0, 0);
        cydui_text_input(&rig.ui, "Two", other, sizeof(other), 0, 0);
        cydui_ui_end(&rig.ui);
    }
    rig_char(&rig, 'x'); /* typed, and in the same frame the second field is tapped */
    rig_down(&rig, 15, 60);
    rig_up(&rig, 15, 60);
    cydui_ui_begin(&rig.ui, 0);
    cydui_text_input(&rig.ui, "One", buf, cap, 0, 0);
    cydui_text_input(&rig.ui, "Two", other, sizeof(other), 0, 0);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_STRING("x", buf);
    TEST_ASSERT_EQUAL_STRING("", other);
}

static void test_a_buffer_that_is_not_closed_is_made_safe(void)
{
    start();
    memset(buf, 'k', sizeof(buf)); /* no NUL at all */
    cap = 8;
    focus_the_field();
    type("z");
    TEST_ASSERT_EQUAL_INT(0, buf[7]);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_tap_gives_the_focus_and_typing_edits);
    RUN_TEST(test_keys_typed_before_the_focus_are_not_kept_for_later);
    RUN_TEST(test_the_editing_keys);
    RUN_TEST(test_several_keys_in_one_frame_apply_in_order);
    RUN_TEST(test_enter_confirms_and_gives_up_the_focus_and_escape_only_the_focus);
    RUN_TEST(test_a_press_elsewhere_drops_the_focus_but_a_press_on_the_field_keeps_it);
    RUN_TEST(test_the_focus_is_dropped_when_the_field_is_not_called);
    RUN_TEST(test_focus_set_and_clear);
    RUN_TEST(test_a_tap_puts_the_cursor_between_the_characters);
    RUN_TEST(test_a_space_moves_the_cursor_like_any_other_character);
    RUN_TEST(test_multibyte_characters);
    RUN_TEST(test_the_limit_of_characters_outlines_the_field_for_a_moment);
    RUN_TEST(test_no_room_in_the_buffer_is_refused_the_same_way);
    RUN_TEST(test_a_password_draws_bullets_and_never_the_text);
    RUN_TEST(test_a_long_text_scrolls_to_keep_the_cursor_in_view);
    RUN_TEST(test_a_text_longer_than_one_text_command_is_still_shown_at_the_end);
    RUN_TEST(test_the_hint_shows_while_it_is_empty);
    RUN_TEST(test_a_disabled_field_and_a_missing_buffer_do_nothing);
    RUN_TEST(test_two_fields_the_focus_goes_to_the_one_tapped);
    RUN_TEST(test_a_key_is_taken_by_one_field_only_even_when_the_focus_moves_in_the_same_frame);
    RUN_TEST(test_a_buffer_that_is_not_closed_is_made_safe);
    return UNITY_END();
}
