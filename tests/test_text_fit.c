/* cydui_text_fit and cydui_text_ellipsize: what fits a width as drawn, and the same with an ellipsis. */

#include <stdlib.h>
#include <string.h>

#include "cydui/cydui.h"
#include "cydui_sans_12.h"
#include "cydui_sans_16.h"
#include "testutil.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static int width(const cydui_font *f, const char *s, size_t n)
{
    return cydui_text_size(f, s, n, 0).w;
}

/* a font of three glyphs and a box: no ellipsis in it */
static const cydui_glyph TINY_GLYPHS[] = {
    {0x20, 0, 0, 0, 0, 0, 3}, {'a', 0, 0, 0, 0, 0, 5}, {'b', 0, 0, 0, 0, 0, 7}, {0xFFFD, 0, 0, 0, 0, 0, 6}};
static const uint8_t TINY_BITMAP[1] = {0};
static const cydui_font TINY = {TINY_GLYPHS, 4, TINY_BITMAP, 1, 10, 8, 3};

void test_a_string_that_fits_is_all_there_is(void)
{
    const char *s = "Slay the inbox";
    TEST_ASSERT_EQUAL_size_t(strlen(s), cydui_text_fit(&cydui_sans_12, s, CYDUI_NTS, 300));
    TEST_ASSERT_EQUAL_size_t(strlen(s), cydui_text_fit(&cydui_sans_12, s, strlen(s), (int16_t)width(&cydui_sans_12, s, strlen(s)))); /* exactly equal fits */
    TEST_ASSERT_EQUAL_size_t(0, cydui_text_fit(&cydui_sans_12, "", CYDUI_NTS, 10));
}

void test_one_pixel_less_than_the_whole_gives_a_shorter_start_that_fits(void)
{
    const char *s = "Slay the inbox";
    int w = width(&cydui_sans_12, s, strlen(s));
    size_t k = cydui_text_fit(&cydui_sans_12, s, CYDUI_NTS, (int16_t)(w - 1));
    TEST_ASSERT_TRUE(k < strlen(s));
    TEST_ASSERT_TRUE(width(&cydui_sans_12, s, k) <= w - 1);
    TEST_ASSERT_TRUE(width(&cydui_sans_12, s, k + 1) > w - 1);
}

void test_nothing_fits_in_nothing_and_bad_arguments_give_zero(void)
{
    TEST_ASSERT_EQUAL_size_t(0, cydui_text_fit(&cydui_sans_12, "abc", CYDUI_NTS, 0));
    TEST_ASSERT_EQUAL_size_t(0, cydui_text_fit(&cydui_sans_12, "abc", CYDUI_NTS, -5));
    TEST_ASSERT_EQUAL_size_t(0, cydui_text_fit(&cydui_sans_12, NULL, CYDUI_NTS, 100));
    TEST_ASSERT_EQUAL_size_t(0, cydui_text_fit(NULL, "abc", CYDUI_NTS, 100));
}

void test_a_character_is_never_cut_in_the_middle(void)
{
    const char *s = "caf\xC3\xA9 cr\xC3\xA8me br\xC3\xBBl\xC3\xA9" "e \xE2\x82\xAC 5";
    int w;
    for (w = 0; w < 120; w++) {
        size_t k = cydui_text_fit(&cydui_sans_12, s, CYDUI_NTS, (int16_t)w);
        TEST_ASSERT_TRUE(k == strlen(s) || ((uint8_t)s[k] & 0xC0u) != 0x80u);
        TEST_ASSERT_TRUE(width(&cydui_sans_12, s, k) <= w);
    }
}

void test_the_longest_text_is_bounded_by_the_library_limit(void)
{
    static char big[CYDUI_MAX_STR * 4];
    size_t k;
    memset(big, 'i', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    k = cydui_text_fit(&cydui_sans_12, big, CYDUI_NTS, 32000);
    TEST_ASSERT_EQUAL_size_t(CYDUI_MAX_STR, k);
}

/* the width as drawn is not the sum of its pieces: the property holds over many strings and widths */
void test_over_random_strings_the_result_fits_and_one_more_character_does_not(void)
{
    static const char *const pool[] = {"a", "b", "i", "W", "m", " ", "l", ".", "\xC3\xA9", "\xE2\x82\xAC", "0", "g"};
    const cydui_font *fonts[2] = {&cydui_sans_12, &cydui_sans_16};
    int round;
    srand(7);
    for (round = 0; round < 3000; round++) {
        char s[80];
        size_t len = 0, k, next;
        const cydui_font *f = fonts[round & 1];
        int pieces = rand() % 20, i, w = rand() % 160;
        s[0] = '\0';
        for (i = 0; i < pieces; i++) {
            const char *p = pool[rand() % (sizeof(pool) / sizeof(pool[0]))];
            strcat(s, p);
        }
        len = strlen(s);
        k = cydui_text_fit(f, s, len, (int16_t)w);
        TEST_ASSERT_TRUE_MESSAGE(width(f, s, k) <= w, s);
        if (k < len) {
            next = k + 1;
            while (next < len && ((uint8_t)s[next] & 0xC0u) == 0x80u) {
                next++;
            }
            TEST_ASSERT_TRUE_MESSAGE(width(f, s, next) > w, s);
        }
    }
}

/* ---- ellipsize ---- */

void test_text_that_fits_is_copied_whole_and_not_marked_cut(void)
{
    char out[64];
    bool cut = true;
    size_t n = cydui_text_ellipsize(&cydui_sans_12, "Short", CYDUI_NTS, 200, out, sizeof(out), &cut);
    TEST_ASSERT_EQUAL_size_t(5, n);
    TEST_ASSERT_EQUAL_STRING("Short", out);
    TEST_ASSERT_FALSE(cut);
}

void test_long_text_is_cut_and_ends_with_the_ellipsis_within_the_width(void)
{
    const char *s = "A title that is far too long for the room it was given";
    char out[96];
    bool cut = false;
    int w;
    for (w = 20; w < 200; w += 3) {
        size_t n = cydui_text_ellipsize(&cydui_sans_16, s, CYDUI_NTS, (int16_t)w, out, sizeof(out), &cut);
        TEST_ASSERT_EQUAL_size_t(strlen(out), n);
        TEST_ASSERT_TRUE(cut);
        TEST_ASSERT_TRUE_MESSAGE(n >= 3 && memcmp(out + n - 3, "\xE2\x80\xA6", 3) == 0, "ends with the ellipsis");
        if (width(&cydui_sans_16, "\xE2\x80\xA6", 3) <= w) {
            TEST_ASSERT_TRUE_MESSAGE(width(&cydui_sans_16, out, n) <= w, out);
        }
    }
}

void test_when_even_the_ellipsis_is_too_wide_it_is_shown_alone(void)
{
    char out[16];
    size_t n = cydui_text_ellipsize(&cydui_sans_16, "Hello", CYDUI_NTS, 1, out, sizeof(out), NULL);
    TEST_ASSERT_EQUAL_STRING("\xE2\x80\xA6", out);
    TEST_ASSERT_EQUAL_size_t(3, n);
}

void test_a_font_without_the_ellipsis_gets_three_dots(void)
{
    char out[32];
    size_t n = cydui_text_ellipsize(&TINY, "ababababababab", CYDUI_NTS, 40, out, sizeof(out), NULL);
    TEST_ASSERT_TRUE(n >= 3 && memcmp(out + n - 3, "...", 3) == 0);
    TEST_ASSERT_TRUE(width(&TINY, out, n) <= 40);
}

void test_the_buffer_is_never_overrun_and_always_terminated(void)
{
    const char *s = "Some text with \xC3\xA9 and more to cut off here";
    size_t cap;
    for (cap = 1; cap < 24; cap++) {
        unsigned char buf[40];
        size_t n;
        memset(buf, 0xEE, sizeof(buf));
        n = cydui_text_ellipsize(&cydui_sans_12, s, CYDUI_NTS, 60, (char *)buf + 4, cap, NULL);
        TEST_ASSERT_TRUE(n < cap);
        TEST_ASSERT_EQUAL_UINT8(0, buf[4 + n]);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xEE, buf[3], "nothing before the buffer");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xEE, buf[4 + cap], "nothing after it");
    }
    TEST_ASSERT_EQUAL_size_t(0, cydui_text_ellipsize(&cydui_sans_12, s, CYDUI_NTS, 60, NULL, 10, NULL));
    TEST_ASSERT_EQUAL_size_t(0, cydui_text_ellipsize(&cydui_sans_12, s, CYDUI_NTS, 60, (char[2]){0}, 0, NULL));
}

void test_text_that_fits_the_width_but_not_the_buffer_is_cut_to_the_buffer(void)
{
    unsigned char buf[16];
    bool cut = false;
    size_t n;
    memset(buf, 0xEE, sizeof(buf));
    n = cydui_text_ellipsize(&cydui_sans_12, "abcdefgh", CYDUI_NTS, 300, (char *)buf + 2, 7, &cut);
    TEST_ASSERT_TRUE(cut);
    TEST_ASSERT_TRUE(n < 7);
    TEST_ASSERT_EQUAL_UINT8(0, buf[2 + n]);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xEE, buf[2 + 7], "nothing after the buffer");
}

void test_a_cap_too_small_for_the_ellipsis_gives_only_whole_characters(void)
{
    char out[3];
    size_t n = cydui_text_ellipsize(&cydui_sans_12, "a long text indeed", CYDUI_NTS, 10, out, sizeof(out), NULL);
    TEST_ASSERT_EQUAL_size_t(0, n); /* two bytes cannot hold the three of the ellipsis */
    TEST_ASSERT_EQUAL_UINT8(0, (uint8_t)out[0]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_string_that_fits_is_all_there_is);
    RUN_TEST(test_one_pixel_less_than_the_whole_gives_a_shorter_start_that_fits);
    RUN_TEST(test_nothing_fits_in_nothing_and_bad_arguments_give_zero);
    RUN_TEST(test_a_character_is_never_cut_in_the_middle);
    RUN_TEST(test_the_longest_text_is_bounded_by_the_library_limit);
    RUN_TEST(test_over_random_strings_the_result_fits_and_one_more_character_does_not);
    RUN_TEST(test_text_that_fits_is_copied_whole_and_not_marked_cut);
    RUN_TEST(test_long_text_is_cut_and_ends_with_the_ellipsis_within_the_width);
    RUN_TEST(test_when_even_the_ellipsis_is_too_wide_it_is_shown_alone);
    RUN_TEST(test_a_font_without_the_ellipsis_gets_three_dots);
    RUN_TEST(test_the_buffer_is_never_overrun_and_always_terminated);
    RUN_TEST(test_text_that_fits_the_width_but_not_the_buffer_is_cut_to_the_buffer);
    RUN_TEST(test_a_cap_too_small_for_the_ellipsis_gives_only_whole_characters);
    return UNITY_END();
}
