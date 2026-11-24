/* Font data: glyph lookup, fallback, validation, and bounds-checked bitmap access (CT-09, AB-16). */

#include <string.h>

#include "cydui/font.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* A tiny valid font: 'A', 'B', c-cedilla and the U+FFFD box used as fallback. */
static const uint8_t bits[] = {
    0xF0, 0xF0, /* 'A': 3 x 2, 2 bytes per row -> offsets 0..3 */
    0xF0, 0xF0,
    0xFF, 0x00, /* 'B': 4 x 1 at offset 4 */
    0xAA, 0xAA, /* c-cedilla: 2 x 2 at offset 6 */
    0xFF, 0xFF, /* box: 2 x 4 at offset 8 (one byte per row) */
    0xFF, 0xFF
};

static const cydui_glyph table[] = {
    {0x0020, 0, 0, 0, 0, 0, 4},  /* space: blank */
    {0x0041, 0, 3, 2, 0, 2, 5},
    {0x0042, 4, 4, 1, 0, 1, 5},
    {0x00E7, 6, 2, 2, 0, 2, 4},
    {0xFFFD, 8, 2, 4, 0, 4, 3},
};

static cydui_font good(void)
{
    cydui_font f;
    f.glyphs = table;
    f.glyph_count = 5;
    f.bitmap = bits;
    f.bitmap_len = sizeof(bits);
    f.line_height = 12;
    f.ascent = 9;
    f.fallback = 4;
    return f;
}

static void test_lookup_finds_every_glyph(void)
{
    cydui_font f = good();
    size_t i;

    for (i = 0; i < 5; i++) {
        const cydui_glyph *g = cydui_font_lookup(&f, table[i].cp);
        TEST_ASSERT_NOT_NULL(g);
        TEST_ASSERT_EQUAL_PTR(&table[i], g); /* first, middle and last all hit */
    }
}

static void test_lookup_falls_back_for_missing_code_points(void)
{
    cydui_font f = good();
    uint32_t missing[] = {0x0000, 0x001F, 0x0021, 0x0040, 0x0043, 0x00E6, 0x00E8, 0x20AC,
                          0xFFFC, 0xFFFE, 0x10FFFF, 0xFFFFFFFFu};
    size_t i;

    for (i = 0; i < sizeof(missing) / sizeof(missing[0]); i++) {
        TEST_ASSERT_EQUAL_PTR(&table[4], cydui_font_lookup(&f, missing[i]));
    }
}

static void test_lookup_single_glyph_font(void)
{
    cydui_font f = good();
    f.glyph_count = 1;
    f.fallback = 0;
    TEST_ASSERT_EQUAL_PTR(&table[0], cydui_font_lookup(&f, 0x20));
    TEST_ASSERT_EQUAL_PTR(&table[0], cydui_font_lookup(&f, 0x41)); /* missing -> the only glyph */
}

static void test_lookup_never_crashes_on_unusable_fonts(void)
{
    cydui_font f = good();

    TEST_ASSERT_NULL(cydui_font_lookup(NULL, 'A'));
    f.glyph_count = 0;
    TEST_ASSERT_NULL(cydui_font_lookup(&f, 'A'));
    f = good();
    f.glyphs = NULL;
    TEST_ASSERT_NULL(cydui_font_lookup(&f, 'A'));
    f = good();
    f.fallback = 99; /* out of range: even a hit is refused, nothing can be trusted */
    TEST_ASSERT_NULL(cydui_font_lookup(&f, 'A'));
    TEST_ASSERT_NULL(cydui_font_lookup(&f, 'Z'));
}

static void test_valid_font_passes(void)
{
    cydui_font f = good();
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_OK, cydui_font_validate(&f));
}

static void test_validate_rejects_malformed_fonts(void)
{
    cydui_font f;
    static cydui_glyph bad[5];

    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_NULL, cydui_font_validate(NULL));

    f = good();
    f.glyphs = NULL;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_NULL, cydui_font_validate(&f));

    f = good();
    f.glyph_count = 0;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_EMPTY, cydui_font_validate(&f));

    f = good();
    f.line_height = 0;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_METRICS, cydui_font_validate(&f));

    f = good();
    f.fallback = 5;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_FALLBACK, cydui_font_validate(&f));

    memcpy(bad, table, sizeof(bad)); /* swapped order */
    bad[1] = table[2];
    bad[2] = table[1];
    f = good();
    f.glyphs = bad;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_ORDER, cydui_font_validate(&f));

    memcpy(bad, table, sizeof(bad)); /* duplicate code point */
    bad[2].cp = bad[1].cp;
    f = good();
    f.glyphs = bad;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_ORDER, cydui_font_validate(&f));

    memcpy(bad, table, sizeof(bad)); /* bitmap runs past the data by one byte */
    bad[4].offset = 9;
    f = good();
    f.glyphs = bad;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_BITMAP, cydui_font_validate(&f));

    memcpy(bad, table, sizeof(bad)); /* offset so large that offset + size would wrap */
    bad[3].offset = 0xFFFFFFFFu;
    f = good();
    f.glyphs = bad;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_BITMAP, cydui_font_validate(&f));

    f = good();
    f.bitmap = NULL; /* glyphs with pixels but no data */
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_ERR_BITMAP, cydui_font_validate(&f));
}

static void test_glyph_data_is_bounds_checked(void)
{
    cydui_font f = good();
    cydui_glyph g;

    TEST_ASSERT_EQUAL_size_t(0, cydui_glyph_bytes(&table[0])); /* space */
    TEST_ASSERT_EQUAL_size_t(4, cydui_glyph_bytes(&table[1])); /* 3 px -> 2 bytes per row, 2 rows */
    TEST_ASSERT_EQUAL_size_t(2, cydui_glyph_bytes(&table[2])); /* 4 px -> 2 bytes, 1 row */
    TEST_ASSERT_EQUAL_size_t(2, cydui_glyph_bytes(&table[3]));
    TEST_ASSERT_EQUAL_size_t(4, cydui_glyph_bytes(&table[4]));

    TEST_ASSERT_NULL(cydui_font_glyph_data(&f, &table[0])); /* blank glyph has no data */
    TEST_ASSERT_EQUAL_PTR(bits + 4, cydui_font_glyph_data(&f, &table[2]));
    TEST_ASSERT_EQUAL_PTR(bits + 8, cydui_font_glyph_data(&f, &table[4]));

    g = table[4]; /* last byte of the array is the limit */
    g.offset = 8;
    TEST_ASSERT_NOT_NULL(cydui_font_glyph_data(&f, &g));
    g.offset = 9; /* one byte too far */
    TEST_ASSERT_NULL(cydui_font_glyph_data(&f, &g));
    g.offset = 0xFFFFFFFFu; /* would wrap in 32-bit arithmetic */
    TEST_ASSERT_NULL(cydui_font_glyph_data(&f, &g));
    g = table[4];
    g.w = 255; /* a huge declared size */
    g.h = 255;
    TEST_ASSERT_NULL(cydui_font_glyph_data(&f, &g));

    f.bitmap = NULL;
    TEST_ASSERT_NULL(cydui_font_glyph_data(&f, &table[1]));
    TEST_ASSERT_NULL(cydui_font_glyph_data(NULL, &table[1]));
    f = good();
    TEST_ASSERT_NULL(cydui_font_glyph_data(&f, NULL));
}

/* Blank glyphs (spaces) carry no pixels: a font made only of them needs no bitmap data at all. */
static void test_blank_glyphs_need_no_bitmap(void)
{
    static const cydui_glyph blanks[] = {{0x20, 12345, 0, 0, 0, 0, 4}, {0xFFFD, 0, 0, 5, 0, 0, 6}};
    cydui_font f;

    f.glyphs = blanks;
    f.glyph_count = 2;
    f.bitmap = NULL;
    f.bitmap_len = 0;
    f.line_height = 10;
    f.ascent = 8;
    f.fallback = 1;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_OK, cydui_font_validate(&f));
    TEST_ASSERT_NULL(cydui_font_glyph_data(&f, &blanks[0]));
    TEST_ASSERT_EQUAL_PTR(&blanks[0], cydui_font_lookup(&f, 0x20));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_lookup_finds_every_glyph);
    RUN_TEST(test_lookup_falls_back_for_missing_code_points);
    RUN_TEST(test_lookup_single_glyph_font);
    RUN_TEST(test_lookup_never_crashes_on_unusable_fonts);
    RUN_TEST(test_valid_font_passes);
    RUN_TEST(test_validate_rejects_malformed_fonts);
    RUN_TEST(test_glyph_data_is_bounds_checked);
    RUN_TEST(test_blank_glyphs_need_no_bitmap);
    return UNITY_END();
}
