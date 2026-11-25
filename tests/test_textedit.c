/*
 * The editing of a string in a buffer (cydui/textedit.h): UTF-8 characters, the limits of the buffer and of the
 * number of characters, controls and bidi marks, and buffers that are not what they should be. The expected
 * values are worked out by hand from the UTF-8 byte lengths: "á" is 2 bytes, "€" 3, "😀" 4.
 */

#include <string.h>

#include "cydui/textedit.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define BUF 32

static char buf[BUF];
static size_t cur;

static void set(const char *s, size_t at)
{
    memset(buf, 'X', sizeof(buf)); /* what is after the NUL must not matter */
    strcpy(buf, s);
    cur = at;
}

static void test_insert_ascii_in_the_middle_and_at_the_ends(void)
{
    set("ac", 1);
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(buf, BUF, &cur, 'b', 0, 0));
    TEST_ASSERT_EQUAL_STRING("abc", buf);
    TEST_ASSERT_EQUAL_UINT(2, cur);
    cur = 0;
    cydui_textedit_insert(buf, BUF, &cur, '>', 0, 0);
    cur = 4;
    cydui_textedit_insert(buf, BUF, &cur, '<', 0, 0);
    TEST_ASSERT_EQUAL_STRING(">abc<", buf);
    TEST_ASSERT_EQUAL_UINT(5, cur);
}

static void test_insert_multibyte_characters(void)
{
    set("", 0);
    cydui_textedit_insert(buf, BUF, &cur, 0xE1, 0, 0);    /* á */
    cydui_textedit_insert(buf, BUF, &cur, 0x20AC, 0, 0);  /* € */
    cydui_textedit_insert(buf, BUF, &cur, 0x1F600, 0, 0); /* 😀 */
    TEST_ASSERT_EQUAL_STRING("\xC3\xA1\xE2\x82\xAC\xF0\x9F\x98\x80", buf);
    TEST_ASSERT_EQUAL_UINT(9, cur);
    TEST_ASSERT_EQUAL_UINT(3, cydui_textedit_chars(buf, BUF));
}

static void test_the_buffer_limit_counts_the_nul_and_never_cuts_a_character(void)
{
    char small[4];
    size_t c = 0;

    memset(small, 0, sizeof(small));
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(small, 4, &c, 'a', 0, 0));
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(small, 4, &c, 0xE1, 0, 0)); /* 3 bytes */
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_FULL, cydui_textedit_insert(small, 4, &c, 'b', 0, 0));                      /* a 4th would need 5 */
    TEST_ASSERT_EQUAL_STRING("a\xC3\xA1", small);
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_FULL, cydui_textedit_insert(small, 4, &c, 0x20AC, 0, 0));
    cydui_textedit_backspace(small, 4, &c);
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_FULL, cydui_textedit_insert(small, 4, &c, 0x20AC, 0, 0)); /* 1 + 3 = 4 > 3 */
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(small, 4, &c, 0xE1, 0, 0));
    TEST_ASSERT_EQUAL_STRING("a\xC3\xA1", small);
}

static void test_a_buffer_of_one_or_zero_bytes_is_safe(void)
{
    char one[1] = {'x'};
    size_t c = 5;

    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_FULL, cydui_textedit_insert(one, 1, &c, 'a', 0, 0));
    TEST_ASSERT_EQUAL_INT(0, one[0]); /* made safe: a NUL, and the cursor inside */
    TEST_ASSERT_EQUAL_UINT(0, c);
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_backspace(one, 1, &c));
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_delete(one, 1, &c));
    c = 3;
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_insert(NULL, 0, &c, 'a', 0, 0));
    TEST_ASSERT_EQUAL_UINT(0, c);
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_len(NULL, 0));
}

static void test_the_limit_of_characters(void)
{
    set("ab", 2);
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_FULL, cydui_textedit_insert(buf, BUF, &cur, 'c', 0, 2));
    TEST_ASSERT_EQUAL_STRING("ab", buf);
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(buf, BUF, &cur, 'c', 0, 3));
    set("\xC3\xA1\xE2\x82\xAC", 5); /* two characters, five bytes */
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_FULL, cydui_textedit_insert(buf, BUF, &cur, 'c', 0, 2)); /* characters, not bytes */
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(buf, BUF, &cur, 'c', 0, 3));
}

static void test_what_is_not_accepted_leaves_everything_alone(void)
{
    static const uint32_t no[] = {0, 0x01, 0x09, 0x0A, 0x0D, 0x1F, 0x7F, 0x80, 0x9F, 0xD800, 0xDFFF, 0x110000, 0xFFFFFFFFu,
                                  0x061C, 0x200E, 0x200F, 0x202A, 0x202E, 0x2066, 0x2069};
    size_t i;

    set("ok", 2);
    for (i = 0; i < sizeof(no) / sizeof(no[0]); i++) {
        TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_insert(buf, BUF, &cur, no[i], 0, 0));
    }
    TEST_ASSERT_EQUAL_STRING("ok", buf);
    TEST_ASSERT_EQUAL_UINT(2, cur);
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(buf, BUF, &cur, 0xA0, 0, 0)); /* first after the C1 block */
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(buf, BUF, &cur, 0x20, 0, 0));
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(buf, BUF, &cur, 0x2065, 0, 0)); /* next to the bidi range */
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(buf, BUF, &cur, 0x202F, 0, 0));
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(buf, BUF, &cur, 0x10FFFF, 0, 0));
}

static void test_the_newline_only_in_a_multiline_field(void)
{
    set("ab", 1);
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_insert(buf, BUF, &cur, '\n', 0, 0));
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_insert(buf, BUF, &cur, '\n', CYDUI_EDIT_MULTILINE, 0));
    TEST_ASSERT_EQUAL_STRING("a\nb", buf);
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_insert(buf, BUF, &cur, '\t', CYDUI_EDIT_MULTILINE, 0));
}

static void test_backspace_and_delete_remove_whole_characters(void)
{
    set("a\xC3\xA1\xE2\x82\xAC\xF0\x9F\x98\x80z", 10); /* a á € 😀 z, the cursor before z */
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT | CYDUI_EDIT_CURSOR, cydui_textedit_backspace(buf, BUF, &cur));
    TEST_ASSERT_EQUAL_STRING("a\xC3\xA1\xE2\x82\xACz", buf);
    TEST_ASSERT_EQUAL_UINT(6, cur);
    cydui_textedit_backspace(buf, BUF, &cur);
    cydui_textedit_backspace(buf, BUF, &cur);
    TEST_ASSERT_EQUAL_STRING("az", buf);
    TEST_ASSERT_EQUAL_UINT(1, cur);
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT, cydui_textedit_delete(buf, BUF, &cur));
    TEST_ASSERT_EQUAL_STRING("a", buf);
    TEST_ASSERT_EQUAL_UINT(1, cur);
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_delete(buf, BUF, &cur)); /* at the end */
    cur = 0;
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_backspace(buf, BUF, &cur)); /* at the start */
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_TEXT, cydui_textedit_delete(buf, BUF, &cur));
    TEST_ASSERT_EQUAL_STRING("", buf);
    set("\xF0\x9F\x98\x80", 0);
    cydui_textedit_delete(buf, BUF, &cur); /* four bytes at once */
    TEST_ASSERT_EQUAL_STRING("", buf);
}

static void test_the_text_after_the_cursor_moves_with_the_nul(void)
{
    set("hello", 2);
    cydui_textedit_insert(buf, BUF, &cur, '-', 0, 0);
    TEST_ASSERT_EQUAL_STRING("he-llo", buf);
    TEST_ASSERT_EQUAL_INT(0, buf[6]);
    cydui_textedit_backspace(buf, BUF, &cur);
    TEST_ASSERT_EQUAL_STRING("hello", buf);
    TEST_ASSERT_EQUAL_INT(0, buf[5]);
}

static void test_moves_by_character(void)
{
    set("a\xC3\xA1z", 0);
    TEST_ASSERT_EQUAL_UINT(CYDUI_EDIT_CURSOR, cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_RIGHT, 0));
    TEST_ASSERT_EQUAL_UINT(1, cur);
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_RIGHT, 0);
    TEST_ASSERT_EQUAL_UINT(3, cur); /* over the two bytes of á */
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_RIGHT, 0);
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_RIGHT, 0)); /* at the end */
    TEST_ASSERT_EQUAL_UINT(4, cur);
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_LEFT, 0);
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_LEFT, 0);
    TEST_ASSERT_EQUAL_UINT(1, cur);
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_LEFT, 0);
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_LEFT, 0));
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_END, 0);
    TEST_ASSERT_EQUAL_UINT(4, cur);
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_HOME, 0);
    TEST_ASSERT_EQUAL_UINT(0, cur);
}

static void test_home_and_end_of_a_line(void)
{
    set("ab\ncde\nf", 5); /* the cursor after "cd" */
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_HOME, CYDUI_EDIT_MULTILINE);
    TEST_ASSERT_EQUAL_UINT(3, cur);
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_END, CYDUI_EDIT_MULTILINE);
    TEST_ASSERT_EQUAL_UINT(6, cur);
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_END, CYDUI_EDIT_MULTILINE));
    cur = 0;
    TEST_ASSERT_EQUAL_UINT(0, cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_HOME, CYDUI_EDIT_MULTILINE));
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_END, CYDUI_EDIT_MULTILINE);
    TEST_ASSERT_EQUAL_UINT(2, cur); /* the end of the first line, before its '\n' */
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_END, 0);
    TEST_ASSERT_EQUAL_UINT(8, cur); /* a single line field: the whole text */
    set("ab\ncd", 1); /* in the first line, Home reaches the very start (offset 0) */
    cydui_textedit_move(buf, BUF, &cur, CYDUI_MOVE_HOME, CYDUI_EDIT_MULTILINE);
    TEST_ASSERT_EQUAL_UINT(0, cur);
}

static void test_a_cursor_in_the_middle_of_a_character_snaps_back(void)
{
    set("\xE2\x82\xAC" "a", 2); /* inside the euro sign */
    cydui_textedit_normalize(buf, BUF, &cur);
    TEST_ASSERT_EQUAL_UINT(0, cur);
    cur = 99; /* beyond the end */
    cydui_textedit_normalize(buf, BUF, &cur);
    TEST_ASSERT_EQUAL_UINT(4, cur);
    set("\xE2\x82\xAC" "a", 1);
    cydui_textedit_insert(buf, BUF, &cur, 'x', 0, 0);
    TEST_ASSERT_EQUAL_STRING("x\xE2\x82\xAC" "a", buf);
}

static void test_a_buffer_without_a_nul_is_closed_and_ill_formed_bytes_are_characters(void)
{
    char raw[6];
    size_t c = 6;

    memset(raw, 'k', sizeof(raw));
    cydui_textedit_normalize(raw, sizeof(raw), &c);
    TEST_ASSERT_EQUAL_INT(0, raw[5]);
    TEST_ASSERT_EQUAL_UINT(5, cydui_textedit_len(raw, sizeof(raw)));
    TEST_ASSERT_EQUAL_UINT(5, c);

    set("a\xFF" "b\x80", 4); /* \xFF and \x80 are one character each, as the decoder says */
    TEST_ASSERT_EQUAL_UINT(4, cydui_textedit_chars(buf, BUF));
    cydui_textedit_backspace(buf, BUF, &cur);
    TEST_ASSERT_EQUAL_STRING("a\xFF" "b", buf);
    cydui_textedit_backspace(buf, BUF, &cur);
    cydui_textedit_backspace(buf, BUF, &cur);
    TEST_ASSERT_EQUAL_STRING("a", buf);

    set("\xE2\x82", 2); /* a truncated sequence is one character */
    TEST_ASSERT_EQUAL_UINT(1, cydui_textedit_chars(buf, BUF));
    cydui_textedit_backspace(buf, BUF, &cur);
    TEST_ASSERT_EQUAL_STRING("", buf);
}

static void test_deleting_between_broken_bytes_keeps_the_cursor_on_a_boundary(void)
{
    /* E2 is the start of a 3-byte character, 82 a continuation: with 'x' between them they are two characters
     * (E2, x, 82), and without it one (E2 82). The cursor must not stay in the middle of that one. */
    set("\xE2" "x\x82", 2);
    cydui_textedit_backspace(buf, BUF, &cur);
    TEST_ASSERT_EQUAL_STRING("\xE2\x82", buf);
    TEST_ASSERT_EQUAL_UINT(0, cur);
    set("\xE2" "x\x82", 1);
    cydui_textedit_delete(buf, BUF, &cur);
    TEST_ASSERT_EQUAL_STRING("\xE2\x82", buf);
    TEST_ASSERT_EQUAL_UINT(0, cur);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_insert_ascii_in_the_middle_and_at_the_ends);
    RUN_TEST(test_insert_multibyte_characters);
    RUN_TEST(test_the_buffer_limit_counts_the_nul_and_never_cuts_a_character);
    RUN_TEST(test_a_buffer_of_one_or_zero_bytes_is_safe);
    RUN_TEST(test_the_limit_of_characters);
    RUN_TEST(test_what_is_not_accepted_leaves_everything_alone);
    RUN_TEST(test_the_newline_only_in_a_multiline_field);
    RUN_TEST(test_backspace_and_delete_remove_whole_characters);
    RUN_TEST(test_the_text_after_the_cursor_moves_with_the_nul);
    RUN_TEST(test_moves_by_character);
    RUN_TEST(test_home_and_end_of_a_line);
    RUN_TEST(test_a_cursor_in_the_middle_of_a_character_snaps_back);
    RUN_TEST(test_a_buffer_without_a_nul_is_closed_and_ill_formed_bytes_are_characters);
    RUN_TEST(test_deleting_between_broken_bytes_keeps_the_cursor_on_a_boundary);
    return UNITY_END();
}
