/* The generated fonts in fonts/ must be valid and cover what they promise. */

#include <string.h>

#include "cydui/font.h"
#include "cydui_sans_12.h"
#include "cydui_sans_16.h"
#include "cydui_sans_24.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

typedef struct {
    const char *name;
    const cydui_font *font;
    int line_height;
} entry;

static const entry fonts[] = {
    {"cydui_sans_12", &cydui_sans_12, 14},
    {"cydui_sans_16", &cydui_sans_16, 18},
    {"cydui_sans_24", &cydui_sans_24, 28},
};

#define N_FONTS (sizeof(fonts) / sizeof(fonts[0]))

static void test_bundled_fonts_validate(void)
{
    size_t i;
    for (i = 0; i < N_FONTS; i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(CYDUI_FONT_OK, cydui_font_validate(fonts[i].font),
                                      fonts[i].name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(fonts[i].line_height, fonts[i].font->line_height,
                                      fonts[i].name);
        TEST_ASSERT_TRUE_MESSAGE(fonts[i].font->ascent > 0 &&
                                     fonts[i].font->ascent < fonts[i].font->line_height,
                                 fonts[i].name);
    }
}

/* Every character the subset promises, in particular everything Portuguese needs. */
static void test_subset_is_present(void)
{
    static const uint32_t wanted[] = {
        0x20, 0x41, 0x5A, 0x61, 0x7A, 0x30, 0x39, 0x7E,                       /* ASCII */
        0xE1, 0xE0, 0xE2, 0xE3, 0xE9, 0xEA, 0xED, 0xF3, 0xF4, 0xF5, 0xFA, 0xFC, /* a-grave... */
        0xE7, 0xC1, 0xC0, 0xC2, 0xC3, 0xC9, 0xCA, 0xCD, 0xD3, 0xD4, 0xD5, 0xDA, 0xC7,
        0xA0, 0xFF,                                                           /* Latin-1 ends */
        0x2022, 0x2026, 0x20AC, 0x2190, 0x2191, 0x2192, 0x2193,               /* symbols */
    };
    size_t i, k;

    for (i = 0; i < N_FONTS; i++) {
        const cydui_glyph *fb = &fonts[i].font->glyphs[fonts[i].font->fallback];
        for (k = 0; k < sizeof(wanted) / sizeof(wanted[0]); k++) {
            const cydui_glyph *g = cydui_font_lookup(fonts[i].font, wanted[k]);
            TEST_ASSERT_NOT_NULL(g);
            TEST_ASSERT_TRUE_MESSAGE(g != fb, "a promised character resolved to the fallback box");
            TEST_ASSERT_EQUAL_HEX32(wanted[k], g->cp);
            if (wanted[k] != 0x20 && wanted[k] != 0xA0) { /* everything else must show ink */
                const uint8_t *px = cydui_font_glyph_data(fonts[i].font, g);
                size_t b, inked = 0;
                TEST_ASSERT_NOT_NULL_MESSAGE(px, "a visible character has no bitmap");
                for (b = 0; b < cydui_glyph_bytes(g); b++) {
                    inked += px[b] != 0;
                }
                TEST_ASSERT_TRUE_MESSAGE(inked > 0, "a visible character is entirely blank");
            }
        }
    }
}

static void test_fallback_is_a_visible_box(void)
{
    size_t i;
    for (i = 0; i < N_FONTS; i++) {
        const cydui_font *f = fonts[i].font;
        const cydui_glyph *fb = &f->glyphs[f->fallback];
        const uint8_t *px = cydui_font_glyph_data(f, fb);

        TEST_ASSERT_EQUAL_HEX32(0xFFFD, fb->cp);
        TEST_ASSERT_TRUE(fb->w >= 4 && fb->h >= 5 && fb->advance > fb->w);
        TEST_ASSERT_NOT_NULL(px);
        TEST_ASSERT_EQUAL_HEX8(0xFF, px[0]); /* top edge starts with two full-coverage pixels */
        /* a character the subset does not include (the check mark) resolves to the box */
        TEST_ASSERT_EQUAL_PTR(fb, cydui_font_lookup(f, 0x2713));
    }
}

static void test_metrics_are_sane(void)
{
    size_t i;
    for (i = 0; i < N_FONTS; i++) {
        const cydui_font *f = fonts[i].font;
        const cydui_glyph *space = cydui_font_lookup(f, ' ');
        const cydui_glyph *a = cydui_font_lookup(f, 'A');
        const cydui_glyph *i_ = cydui_font_lookup(f, 'i');
        const cydui_glyph *w = cydui_font_lookup(f, 'W');

        TEST_ASSERT_EQUAL_UINT8(0, space->w); /* a space has no pixels but has width */
        TEST_ASSERT_TRUE(space->advance > 0);
        TEST_ASSERT_NOT_NULL(cydui_font_glyph_data(f, a));
        TEST_ASSERT_TRUE(a->top > 0 && a->top <= f->ascent + 1); /* sits on the baseline */
        TEST_ASSERT_TRUE(i_->advance < w->advance);              /* proportional font */
    }
    /* sizes are ordered */
    TEST_ASSERT_TRUE(cydui_sans_12.line_height < cydui_sans_16.line_height);
    TEST_ASSERT_TRUE(cydui_sans_16.line_height < cydui_sans_24.line_height);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bundled_fonts_validate);
    RUN_TEST(test_subset_is_present);
    RUN_TEST(test_fallback_is_a_visible_box);
    RUN_TEST(test_metrics_are_sane);
    return UNITY_END();
}
