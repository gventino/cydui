/*
 * The text area: several lines that wrap, a cursor that moves between them, scrolling by lines, taps on a line, and
 * texts longer than one text command can hold. 'l' is 3 px wide and 'W' 11 px in the 12 px font, which makes the
 * columns easy to work out by hand; the text room is 304 - 16 px less the cursor, so a line holds 26 'W's (286 / 11).
 */

#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;
static char buf[700];
static size_t cap, max_chars;
static int rows;

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    memset(buf, 0, sizeof(buf));
    cap = sizeof(buf);
    max_chars = 0;
    rows = 3;
}

static unsigned frame(void)
{
    unsigned r;

    cydui_ui_begin(&rig.ui, 0);
    r = cydui_text_area(&rig.ui, "Notes", buf, cap, max_chars, rows, 0);
    cydui_button(&rig.ui, "Other");
    cydui_ui_end(&rig.ui);
    return r;
}

static void frames(int n)
{
    int i;
    for (i = 0; i < n; i++) frame();
}

static int lh(void)
{
    return rig.theme.style.font->line_height;
}

/* The y of the middle of row `row` of the area, which starts at (8, 8) with 6 px of padding. */
static int row_y(int r)
{
    return 8 + 6 + r * lh() + lh() / 2;
}

static void tap(int x, int y)
{
    rig_down(&rig, x, y);
    frame();
    rig_up(&rig, x, y);
    frames(2);
}

static void focus_it(void)
{
    frames(2);
    tap(15, row_y(0));
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
}

static void type(const char *s)
{
    for (; *s; s++) rig_char(&rig, (uint32_t)(unsigned char)*s);
    frame();
}

static cydui_state *state(void)
{
    return cydui_state_get(&rig.ui, cydui_id_str(&rig.ui, "Notes"), false);
}

static void test_typing_and_enter_make_lines(void)
{
    start();
    focus_it();
    type("ab");
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_char(&rig, 'c');
    rig_char(&rig, '\n'); /* a typed newline is the same as Enter */
    rig_char(&rig, 'd');
    TEST_ASSERT_EQUAL_UINT(CYDUI_TEXT_CHANGED, frame());
    TEST_ASSERT_EQUAL_STRING("ab\nc\nd", buf);
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui)); /* Enter does not confirm here */
}

static void test_up_and_down_keep_the_column(void)
{
    start();
    strcpy(buf, "llllll\nll\nllllll");
    focus_it();
    rig_key(&rig, CYDUI_KEY_END); /* the end of line 0 */
    rig_key(&rig, CYDUI_KEY_DOWN); /* column 18 px: past the short line, so its end */
    rig_char(&rig, 'X');
    frame();
    TEST_ASSERT_EQUAL_STRING("llllll\nllX\nllllll", buf);
    rig_key(&rig, CYDUI_KEY_DOWN); /* from after "llX": 6 + 9 = 15 px, and 15 is exactly after 5 'l' */
    rig_char(&rig, 'Y');
    frame();
    TEST_ASSERT_EQUAL_STRING("llllll\nllX\nlllllYl", buf);
    rig_key(&rig, CYDUI_KEY_UP);
    rig_key(&rig, CYDUI_KEY_UP);
    rig_key(&rig, CYDUI_KEY_UP); /* nothing above the first line */
    rig_char(&rig, 'Z');
    frame();
    TEST_ASSERT_TRUE(strchr(buf, 'Z') != NULL);
    TEST_ASSERT_TRUE(strchr(buf, 'Z') < strchr(buf, '\n')); /* it went into the first line */
}

static void test_down_on_the_last_line_does_nothing(void)
{
    start();
    strcpy(buf, "ab\ncd");
    focus_it();
    rig_key(&rig, CYDUI_KEY_END);
    rig_key(&rig, CYDUI_KEY_DOWN);
    rig_key(&rig, CYDUI_KEY_DOWN);
    rig_key(&rig, CYDUI_KEY_DOWN);
    rig_char(&rig, '!');
    frame();
    TEST_ASSERT_EQUAL_STRING("ab\ncd!", buf);
}

static void test_a_long_word_wraps_and_home_end_follow_the_line_as_shown(void)
{
    int i;

    start();
    focus_it();
    for (i = 0; i < 40; i++) rig_char(&rig, 'W'); /* 26 on the first line and 14 on the second */
    frame();
    rig_key(&rig, CYDUI_KEY_HOME); /* the start of the second line, not of the text */
    rig_char(&rig, '|');
    frame();
    TEST_ASSERT_EQUAL_UINT(26, (unsigned)(strchr(buf, '|') - buf));
    rig_key(&rig, CYDUI_KEY_UP);
    rig_key(&rig, CYDUI_KEY_END); /* the end of the first line: after its 26th character */
    rig_char(&rig, '#');
    frame();
    TEST_ASSERT_EQUAL_UINT(26, (unsigned)(strchr(buf, '#') - buf));
}

static void test_words_wrap_at_the_spaces(void)
{
    int i;
    cydui_state *s;

    start();
    rows = 2;
    focus_it();
    for (i = 0; i < 20; i++) type("word "); /* 100 characters of ~7 px: three or four lines */
    s = state();
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_TRUE(s->b >= 1); /* scrolled: the cursor is on a line below the two that show */
    TEST_ASSERT_EQUAL_INT(100, (int)s->a);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_scrolling_follows_the_cursor_by_lines_and_comes_back(void)
{
    cydui_state *s;

    start();
    rows = 2;
    focus_it();
    type("a");
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_char(&rig, 'b');
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_char(&rig, 'c');
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_char(&rig, 'd');
    frame();
    s = state();
    TEST_ASSERT_EQUAL_INT(2, s->b); /* four lines, the cursor on the last one, two rows: the top is line 2 */
    rig_key(&rig, CYDUI_KEY_UP);
    rig_key(&rig, CYDUI_KEY_UP);
    rig_key(&rig, CYDUI_KEY_UP);
    frame();
    TEST_ASSERT_EQUAL_INT(0, s->b);
    rig_key(&rig, CYDUI_KEY_DOWN);
    frame();
    TEST_ASSERT_EQUAL_INT(0, s->b); /* line 1 still shows */
    rig_key(&rig, CYDUI_KEY_DOWN);
    frame();
    TEST_ASSERT_EQUAL_INT(1, s->b);
    /* the text gets shorter: the top cannot be past what exists */
    rig_key(&rig, CYDUI_KEY_END);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    frame();
    TEST_ASSERT_EQUAL_STRING("\nd", buf); /* the cursor was at the end of \"c\": it and what came before it are gone */
    TEST_ASSERT_EQUAL_INT(0, s->b);
}

static void test_a_tap_puts_the_cursor_on_the_line_and_the_character(void)
{
    start();
    strcpy(buf, "aa\nbb\ncc");
    frames(2);
    tap(16 + 7 + 1, row_y(1)); /* 'b' is about 7 px: just past the first b */
    type("X");
    TEST_ASSERT_EQUAL_STRING("aa\nbXb\ncc", buf);
    tap(300, row_y(2)); /* far to the right of the third line */
    type("Y");
    TEST_ASSERT_EQUAL_STRING("aa\nbXb\nccY", buf);
    tap(20, row_y(2) + 200); /* below the area? that is the button or nothing: the focus is dropped */
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
}

static void test_a_tap_on_a_scrolled_area_counts_from_the_top_line_shown(void)
{
    start();
    rows = 2;
    strcpy(buf, "a\nb\nc\nd");
    focus_it();
    rig_key(&rig, CYDUI_KEY_DOWN);
    rig_key(&rig, CYDUI_KEY_DOWN);
    rig_key(&rig, CYDUI_KEY_DOWN);
    frames(2); /* the cursor on line 3: the top line shown is 2 ("c") */
    tap(300, row_y(0)); /* the first row shown is line 2 */
    type("X");
    TEST_ASSERT_EQUAL_STRING("a\nb\ncX\nd", buf);
}

static void test_the_top_line_never_passes_what_is_needed(void)
{
    cydui_state *s;

    start();
    rows = 2;
    focus_it();
    type("a");
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_char(&rig, 'b');
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_char(&rig, 'c');
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_char(&rig, 'd');
    frame();
    s = state();
    TEST_ASSERT_EQUAL_INT(2, s->b);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    frame();
    TEST_ASSERT_EQUAL_STRING("a\nb\nc", buf);
    TEST_ASSERT_EQUAL_INT(1, s->b); /* three lines and two rows: the last two show, not "c" and a blank */
}

static void test_the_first_line_is_drawn(void)
{
    uint64_t empty, with_text;

    start();
    strcpy(buf, "\n"); /* nothing to see, and no hint: the hint is for an empty text */
    frames(3);
    empty = cydui_mem_hash(&rig.mem);
    strcpy(buf, "W");
    frames(3);
    with_text = cydui_mem_hash(&rig.mem);
    TEST_ASSERT_TRUE(empty != with_text);
}

static void test_a_tap_below_the_text_goes_to_the_last_line(void)
{
    start();
    rows = 4;
    strcpy(buf, "aa\nbb");
    frames(2);
    tap(60, row_y(3)); /* the fourth row is empty */
    type("Z");
    TEST_ASSERT_EQUAL_STRING("aa\nbbZ", buf);
}

static void test_the_empty_line_after_a_final_newline_holds_the_cursor(void)
{
    start();
    focus_it();
    type("ab");
    rig_key(&rig, CYDUI_KEY_ENTER);
    frame();
    rig_char(&rig, 'c');
    frame();
    TEST_ASSERT_EQUAL_STRING("ab\nc", buf);
    rig_key(&rig, CYDUI_KEY_BACKSPACE);
    rig_key(&rig, CYDUI_KEY_UP); /* the cursor was on the empty second line, column 0: now the start of the first */
    rig_char(&rig, '!');
    frame();
    TEST_ASSERT_EQUAL_STRING("!ab\n", buf);
}

static void test_a_text_longer_than_one_text_command_is_all_there(void)
{
    cydui_state *s;
    int i;

    start();
    rows = 4;
    focus_it();
    for (i = 0; i < 400; i++) { /* 400 'W' is 16 lines of 26 (the last of 10): far beyond CYDUI_MAX_STR bytes */
        rig_char(&rig, 'W');
        if (i % 20 == 19) frame();
    }
    frames(2);
    TEST_ASSERT_EQUAL_UINT(400, strlen(buf));
    s = state();
    TEST_ASSERT_EQUAL_INT(400, (int)s->a);
    TEST_ASSERT_EQUAL_INT(12, s->b); /* 16 lines, the cursor on the last one, four rows: the top is line 12 */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    rig_key(&rig, CYDUI_KEY_HOME);
    frame();
    rig_key(&rig, CYDUI_KEY_UP);
    rig_key(&rig, CYDUI_KEY_UP);
    rig_char(&rig, 'x');
    frame();
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_EQUAL_UINT(401, strlen(buf));
}

static void test_the_limit_counts_the_newlines_too(void)
{
    start();
    max_chars = 4;
    focus_it();
    type("a");
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_key(&rig, CYDUI_KEY_ENTER);
    rig_char(&rig, 'b');
    rig_char(&rig, 'c'); /* turned down */
    frame();
    TEST_ASSERT_EQUAL_STRING("a\n\nb", buf);
}

static void test_rows_are_clamped_and_the_hint_and_a_disabled_area_behave(void)
{
    uint64_t hint, plain;

    start();
    rows = 0;
    frames(2);
    rows = 1000;
    frames(2);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));

    start();
    frames(3);
    hint = cydui_mem_hash(&rig.mem);
    cydui_ui_begin(&rig.ui, 0);
    cydui_text_area(&rig.ui, "##x", buf, cap, 0, rows, 0);
    cydui_button(&rig.ui, "Other");
    cydui_ui_end(&rig.ui);
    cydui_ui_begin(&rig.ui, 0);
    cydui_text_area(&rig.ui, "##x", buf, cap, 0, rows, 0);
    cydui_button(&rig.ui, "Other");
    cydui_ui_end(&rig.ui);
    plain = cydui_mem_hash(&rig.mem);
    TEST_ASSERT_TRUE(hint != plain);

    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_EQUAL_UINT(0, cydui_text_area(&rig.ui, "n", NULL, 0, 0, 3, 0));
    TEST_ASSERT_EQUAL_UINT(0, cydui_text_area(&rig.ui, NULL, buf, 0, 0, 3, 0));
    cydui_next_disabled(&rig.ui);
    TEST_ASSERT_EQUAL_UINT(0, cydui_text_area(&rig.ui, "d", buf, cap, 0, 3, 0));
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_the_area_and_the_field_share_the_focus_rules(void)
{
    start();
    focus_it();
    rig_key(&rig, CYDUI_KEY_ESCAPE);
    frame();
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
    tap(15, row_y(0));
    tap(300, 230); /* elsewhere */
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_typing_and_enter_make_lines);
    RUN_TEST(test_up_and_down_keep_the_column);
    RUN_TEST(test_down_on_the_last_line_does_nothing);
    RUN_TEST(test_a_long_word_wraps_and_home_end_follow_the_line_as_shown);
    RUN_TEST(test_words_wrap_at_the_spaces);
    RUN_TEST(test_scrolling_follows_the_cursor_by_lines_and_comes_back);
    RUN_TEST(test_a_tap_puts_the_cursor_on_the_line_and_the_character);
    RUN_TEST(test_a_tap_on_a_scrolled_area_counts_from_the_top_line_shown);
    RUN_TEST(test_the_top_line_never_passes_what_is_needed);
    RUN_TEST(test_the_first_line_is_drawn);
    RUN_TEST(test_a_tap_below_the_text_goes_to_the_last_line);
    RUN_TEST(test_the_empty_line_after_a_final_newline_holds_the_cursor);
    RUN_TEST(test_a_text_longer_than_one_text_command_is_all_there);
    RUN_TEST(test_the_limit_counts_the_newlines_too);
    RUN_TEST(test_rows_are_clamped_and_the_hint_and_a_disabled_area_behave);
    RUN_TEST(test_the_area_and_the_field_share_the_focus_rules);
    return UNITY_END();
}
