/*
 * Text: measurement, wrapping, alignment, the CYDUI_MAX_STR limit (CT-02), control and bidi
 * filtering (CT-04), the text arena (CT-01), and robustness against malformed fonts (AB-16).
 *
 * Layout is tested with a monospace font whose numbers are easy to check by hand: every glyph
 * advances 5 px, is a solid 4x6 block, and a line is 8 px high.
 */

#include <stdlib.h>
#include <string.h>

#include "testutil.h"
#include "unity.h"

#define WHITE 0xFFFFu
#define BLACK 0x0000u

void setUp(void) {}
void tearDown(void) {}

/* ---- test font ---- */

#define MONO_GLYPHS 116
static cydui_glyph mono_table[MONO_GLYPHS];
static const uint8_t mono_bits[24] = {
    /* solid block: 4x6, 2 bytes per row */
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* hollow box: the fallback */
    0xFF, 0xFF, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xF0, 0x0F, 0xFF, 0xFF};
static cydui_font mono;

static void glyph(cydui_glyph *g, uint32_t cp, int blank, uint32_t offset)
{
    g->cp = cp;
    g->offset = offset;
    g->w = blank ? 0 : 4;
    g->h = blank ? 0 : 6;
    g->left = 0;
    g->top = 6;
    g->advance = 5;
}

/* Builds the font on first use: ' ', '!'..'~', plus glyphs the filter must override. */
static const cydui_font *mono_font(void)
{
    size_t n = 0;
    uint32_t cp;

    if (mono.glyphs) {
        return &mono;
    }
    glyph(&mono_table[n++], 0x01, 0, 0); /* a real glyph for a control character */
    glyph(&mono_table[n++], 0x20, 1, 0);
    for (cp = 0x21; cp <= 0x7E; cp++) {
        glyph(&mono_table[n++], cp, 0, 0);
    }
    /* Real glyphs on both sides of every range the filter replaces (CT-04). */
    glyph(&mono_table[n++], 0x7F, 0, 0);    /* C0/C1 edge: DEL .. */
    glyph(&mono_table[n++], 0x80, 0, 0);
    glyph(&mono_table[n++], 0x9F, 0, 0);    /* .. last C1 control */
    glyph(&mono_table[n++], 0xA0, 1, 0);    /* NBSP: NOT filtered, blank like a space */
    glyph(&mono_table[n++], 0x061B, 0, 0);  /* not filtered */
    glyph(&mono_table[n++], 0x061C, 0, 0);  /* ARABIC LETTER MARK */
    glyph(&mono_table[n++], 0x200D, 0, 0);  /* not filtered */
    glyph(&mono_table[n++], 0x200E, 0, 0);
    glyph(&mono_table[n++], 0x200F, 0, 0);
    glyph(&mono_table[n++], 0x2010, 0, 0);  /* not filtered */
    glyph(&mono_table[n++], 0x2029, 0, 0);  /* not filtered */
    glyph(&mono_table[n++], 0x202A, 0, 0);  /* first of the embedding/override range */
    glyph(&mono_table[n++], 0x202D, 0, 0);
    glyph(&mono_table[n++], 0x202E, 0, 0);  /* RIGHT-TO-LEFT OVERRIDE */
    glyph(&mono_table[n++], 0x202F, 0, 0);  /* not filtered */
    glyph(&mono_table[n++], 0x2065, 0, 0);  /* not filtered */
    glyph(&mono_table[n++], 0x2066, 0, 0);  /* first isolate */
    glyph(&mono_table[n++], 0x2069, 0, 0);  /* last isolate */
    glyph(&mono_table[n++], 0x206A, 0, 0);  /* not filtered */
    glyph(&mono_table[n++], 0xFFFD, 0, 12);
    TEST_ASSERT_EQUAL_size_t(MONO_GLYPHS, n);
    mono.glyphs = mono_table;
    mono.glyph_count = MONO_GLYPHS;
    mono.bitmap = mono_bits;
    mono.bitmap_len = sizeof(mono_bits);
    mono.line_height = 8;
    mono.ascent = 6;
    mono.fallback = MONO_GLYPHS - 1;
    TEST_ASSERT_EQUAL_INT(CYDUI_FONT_OK, cydui_font_validate(&mono));
    return &mono;
}

/* ---- measurement and layout ---- */

typedef struct {
    char text[8][24];
    int width[8];
    size_t n;
} lines_t;

static lines_t layout(const char *s, size_t len, int wrap)
{
    lines_t out;
    cydui_text_iter it;
    cydui_text_line ln;
    bool trunc;

    memset(&out, 0, sizeof(out));
    len = cydui_text_clip_len(s, len, &trunc);
    cydui_text_iter_init(&it, mono_font(), s, len, (int16_t)wrap);
    while (cydui_text_next_line(&it, &ln)) {
        TEST_ASSERT_TRUE(out.n < 8);
        TEST_ASSERT_TRUE(ln.end - ln.start < 24);
        memcpy(out.text[out.n], s + ln.start, ln.end - ln.start);
        out.width[out.n] = ln.width;
        out.n++;
    }
    return out;
}

static void test_size_of_simple_text(void)
{
    const cydui_font *f = mono_font();
    cydui_text_extent e;

    e = cydui_text_size(f, "abc", CYDUI_NTS, 0);
    TEST_ASSERT_EQUAL_INT(15, e.w);
    TEST_ASSERT_EQUAL_INT(8, e.h);
    TEST_ASSERT_EQUAL_UINT(1, e.lines);

    e = cydui_text_size(f, "", CYDUI_NTS, 0);
    TEST_ASSERT_EQUAL_UINT(0, e.lines);
    TEST_ASSERT_EQUAL_INT(0, e.w);
    TEST_ASSERT_EQUAL_INT(0, e.h);

    e = cydui_text_size(f, "a\nbb", CYDUI_NTS, 0);
    TEST_ASSERT_EQUAL_INT(10, e.w);
    TEST_ASSERT_EQUAL_INT(16, e.h);
    TEST_ASSERT_EQUAL_UINT(2, e.lines);

    e = cydui_text_size(f, "ab   ", CYDUI_NTS, 0); /* trailing spaces are not ink */
    TEST_ASSERT_EQUAL_INT(10, e.w);
    e = cydui_text_size(f, "  a", CYDUI_NTS, 0); /* leading spaces are */
    TEST_ASSERT_EQUAL_INT(15, e.w);
    e = cydui_text_size(f, "abcdef", 3, 0); /* explicit length */
    TEST_ASSERT_EQUAL_INT(15, e.w);

    e = cydui_text_size(NULL, "abc", CYDUI_NTS, 0); /* unusable font: nothing, no crash */
    TEST_ASSERT_EQUAL_UINT(0, e.lines);
    e = cydui_text_size(f, NULL, 5, 0);
    TEST_ASSERT_EQUAL_UINT(0, e.lines);
}

static void test_wrapping_at_spaces(void)
{
    lines_t l = layout("ab cd ef", CYDUI_NTS, 20); /* four characters per line */
    TEST_ASSERT_EQUAL_size_t(3, l.n);
    TEST_ASSERT_EQUAL_STRING("ab", l.text[0]);
    TEST_ASSERT_EQUAL_STRING("cd", l.text[1]);
    TEST_ASSERT_EQUAL_STRING("ef", l.text[2]);
    TEST_ASSERT_EQUAL_INT(10, l.width[0]);

    l = layout("ab cdefgh", CYDUI_NTS, 20); /* a long word follows a short one */
    TEST_ASSERT_EQUAL_size_t(3, l.n);
    TEST_ASSERT_EQUAL_STRING("ab", l.text[0]);
    TEST_ASSERT_EQUAL_STRING("cdef", l.text[1]);
    TEST_ASSERT_EQUAL_STRING("gh", l.text[2]);

    l = layout("ab   cd", CYDUI_NTS, 20); /* a run of spaces collapses at the break */
    TEST_ASSERT_EQUAL_size_t(2, l.n);
    TEST_ASSERT_EQUAL_STRING("ab", l.text[0]);
    TEST_ASSERT_EQUAL_STRING("cd", l.text[1]);

    l = layout("ab cd", CYDUI_NTS, 100); /* fits: one line, spaces inside kept */
    TEST_ASSERT_EQUAL_size_t(1, l.n);
    TEST_ASSERT_EQUAL_STRING("ab cd", l.text[0]);
    TEST_ASSERT_EQUAL_INT(25, l.width[0]);
}

static void test_words_wider_than_the_line_are_broken(void)
{
    lines_t l = layout("abcdefgh", CYDUI_NTS, 15);
    TEST_ASSERT_EQUAL_size_t(3, l.n);
    TEST_ASSERT_EQUAL_STRING("abc", l.text[0]);
    TEST_ASSERT_EQUAL_STRING("def", l.text[1]);
    TEST_ASSERT_EQUAL_STRING("gh", l.text[2]);

    /* A line narrower than one glyph still makes progress: one glyph per line. */
    l = layout("abc", CYDUI_NTS, 3);
    TEST_ASSERT_EQUAL_size_t(3, l.n);
    TEST_ASSERT_EQUAL_STRING("a", l.text[0]);
    TEST_ASSERT_EQUAL_STRING("c", l.text[2]);
}

static void test_newlines_and_edge_cases(void)
{
    lines_t l = layout("a\n\nb", CYDUI_NTS, 0);
    TEST_ASSERT_EQUAL_size_t(3, l.n);
    TEST_ASSERT_EQUAL_STRING("a", l.text[0]);
    TEST_ASSERT_EQUAL_STRING("", l.text[1]);
    TEST_ASSERT_EQUAL_STRING("b", l.text[2]);

    l = layout("a\n", CYDUI_NTS, 0); /* a final newline does not add a line */
    TEST_ASSERT_EQUAL_size_t(1, l.n);

    l = layout("\n", CYDUI_NTS, 0);
    TEST_ASSERT_EQUAL_size_t(1, l.n);

    l = layout("   ", CYDUI_NTS, 0); /* only spaces: one blank line */
    TEST_ASSERT_EQUAL_size_t(1, l.n);
    TEST_ASSERT_EQUAL_INT(0, l.width[0]);

    l = layout("a\r\nb", CYDUI_NTS, 0); /* CR is dropped and has no width */
    TEST_ASSERT_EQUAL_size_t(2, l.n);
    TEST_ASSERT_EQUAL_INT(5, l.width[0]);

    /* Leading spaces are kept (an indent), and a long word after them is still broken. */
    l = layout("      abcdefg", CYDUI_NTS, 20);
    TEST_ASSERT_EQUAL_size_t(3, l.n);
    TEST_ASSERT_EQUAL_STRING("      ", l.text[0]);
    TEST_ASSERT_EQUAL_INT(0, l.width[0]); /* blank: no ink */
    TEST_ASSERT_EQUAL_STRING("abcd", l.text[1]);
    TEST_ASSERT_EQUAL_STRING("efg", l.text[2]);
}

/* Whatever the input, a loop over the lines ends and consumes the whole string. */
static void test_layout_always_terminates(void)
{
    static const char *inputs[] = {"", " ", "  ", "a", "a b", " a", "a ", "\n\n\n", "x y z w v u t",
                                   "\xFF\xFE\xFD", "ab\xE2\x82", "a\xC3", "\t\t\t", "a  b  c  d"};
    size_t i;
    int wrap;

    for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
        for (wrap = 0; wrap <= 30; wrap += 1) {
            cydui_text_iter it;
            cydui_text_line ln;
            size_t len = strlen(inputs[i]), lines = 0, last_end = 0;

            cydui_text_iter_init(&it, mono_font(), inputs[i], len, (int16_t)wrap);
            while (cydui_text_next_line(&it, &ln)) {
                TEST_ASSERT_TRUE(ln.start <= ln.end && ln.end <= len);
                TEST_ASSERT_TRUE(it.pos > ln.start || len == 0); /* progress */
                TEST_ASSERT_TRUE(ln.start >= last_end || ln.start == last_end);
                last_end = ln.end;
                TEST_ASSERT_TRUE_MESSAGE(++lines <= len + 1, "more lines than bytes");
            }
            TEST_ASSERT_EQUAL_size_t(len, it.pos);
        }
    }
}

/* ---- limits (CT-02) ---- */

static void test_clip_len_limit_and_boundary(void)
{
    char big[400];
    bool trunc;

    memset(big, 'a', sizeof(big));
    TEST_ASSERT_EQUAL_size_t(10, cydui_text_clip_len(big, 10, &trunc));
    TEST_ASSERT_FALSE(trunc);
    TEST_ASSERT_EQUAL_size_t(CYDUI_MAX_STR, cydui_text_clip_len(big, CYDUI_MAX_STR, &trunc));
    TEST_ASSERT_FALSE(trunc);
    TEST_ASSERT_EQUAL_size_t(CYDUI_MAX_STR, cydui_text_clip_len(big, 400, &trunc));
    TEST_ASSERT_TRUE(trunc);
    TEST_ASSERT_EQUAL_size_t(0, cydui_text_clip_len(NULL, 5, &trunc));

    /* the cut falls inside a two-byte character: it is dropped whole */
    memset(big, 'a', sizeof(big));
    big[CYDUI_MAX_STR - 1] = (char)0xC3;
    big[CYDUI_MAX_STR] = (char)0xA9;
    TEST_ASSERT_EQUAL_size_t(CYDUI_MAX_STR - 1, cydui_text_clip_len(big, 400, &trunc));
    TEST_ASSERT_TRUE(trunc);

    /* ... a three-byte character cut after two bytes */
    big[CYDUI_MAX_STR - 2] = (char)0xE2;
    big[CYDUI_MAX_STR - 1] = (char)0x82;
    big[CYDUI_MAX_STR] = (char)0xAC;
    TEST_ASSERT_EQUAL_size_t(CYDUI_MAX_STR - 2, cydui_text_clip_len(big, 400, &trunc));

    /* ... a four-byte character cut after three bytes */
    big[CYDUI_MAX_STR - 3] = (char)0xF0;
    big[CYDUI_MAX_STR - 2] = (char)0x9F;
    big[CYDUI_MAX_STR - 1] = (char)0x98;
    big[CYDUI_MAX_STR] = (char)0x80;
    TEST_ASSERT_EQUAL_size_t(CYDUI_MAX_STR - 3, cydui_text_clip_len(big, 400, &trunc));

    /* a character that ends exactly at the limit is kept */
    memset(big, 'a', sizeof(big));
    big[CYDUI_MAX_STR - 2] = (char)0xC3;
    big[CYDUI_MAX_STR - 1] = (char)0xA9;
    TEST_ASSERT_EQUAL_size_t(CYDUI_MAX_STR, cydui_text_clip_len(big, 400, &trunc));
}

static void test_nul_search_is_bounded(void)
{
    char terminated[10] = "hello";
    char *exact = (char *)malloc(CYDUI_MAX_STR); /* no NUL anywhere: reading past it would be OOB */
    bool trunc;

    TEST_ASSERT_NOT_NULL(exact);
    memset(exact, 'x', CYDUI_MAX_STR);
    TEST_ASSERT_EQUAL_size_t(5, cydui_text_clip_len(terminated, CYDUI_NTS, &trunc));
    TEST_ASSERT_FALSE(trunc);
    TEST_ASSERT_EQUAL_size_t(CYDUI_MAX_STR, cydui_text_clip_len(exact, CYDUI_NTS, &trunc));
    TEST_ASSERT_TRUE(trunc); /* hit the limit without a NUL: treated as cut, and never read further */
    free(exact);
}

/* ---- drawing through a context ---- */

static const cydui_font *g_font; /* scenes below take no arguments */

static void scene_align_left(cydui_ctx *c)
{
    cydui_draw_text(c, g_font, 10, 20, 50, CYDUI_ALIGN_LEFT, "ab", CYDUI_NTS, WHITE, 255);
}
static void scene_align_center(cydui_ctx *c)
{
    cydui_draw_text(c, g_font, 10, 20, 50, CYDUI_ALIGN_CENTER, "ab", CYDUI_NTS, WHITE, 255);
}
static void scene_align_right(cydui_ctx *c)
{
    cydui_draw_text(c, g_font, 10, 20, 50, CYDUI_ALIGN_RIGHT, "ab", CYDUI_NTS, WHITE, 255);
}

static int leftmost_ink(int y0, int y1)
{
    int x, y;
    for (x = 0; x < TU_W; x++) {
        for (y = y0; y < y1; y++) {
            if (tu_px(x, y) != BLACK) {
                return x;
            }
        }
    }
    return -1;
}

static void test_alignment_positions_lines_in_the_box(void)
{
    g_font = mono_font();
    tu_render(scene_align_left, TU_H, BLACK);
    TEST_ASSERT_EQUAL_INT(10, leftmost_ink(0, TU_H));
    tu_render(scene_align_center, TU_H, BLACK);
    TEST_ASSERT_EQUAL_INT(30, leftmost_ink(0, TU_H)); /* 10 + (50 - 10) / 2 */
    tu_render(scene_align_right, TU_H, BLACK);
    TEST_ASSERT_EQUAL_INT(50, leftmost_ink(0, TU_H)); /* 10 + 50 - 10 */
}

static void scene_one_glyph(cydui_ctx *c)
{
    cydui_draw_text(c, g_font, 10, 20, 0, CYDUI_ALIGN_LEFT, "a", CYDUI_NTS, WHITE, 255);
}

static void test_glyph_is_drawn_where_the_metrics_say(void)
{
    int x, y, ink = 0;

    g_font = mono_font();
    tu_render(scene_one_glyph, TU_H, BLACK);
    for (y = 0; y < TU_H; y++) {
        for (x = 0; x < TU_W; x++) {
            if (tu_px(x, y) == WHITE) {
                ink++;
                TEST_ASSERT_TRUE(x >= 10 && x < 14 && y >= 20 && y < 26); /* 4 x 6 at (10, 20) */
            }
        }
    }
    TEST_ASSERT_EQUAL_INT(24, ink);
}

/* ---- control and bidi characters (CT-04) ---- */

static const char *g_text;

static void scene_text(cydui_ctx *c)
{
    cydui_draw_text(c, g_font, 4, 4, 0, CYDUI_ALIGN_LEFT, g_text, CYDUI_NTS, WHITE, 255);
}

static uint64_t hash_of(const char *text)
{
    g_font = mono_font();
    g_text = text;
    return tu_render(scene_text, TU_H, BLACK);
}

static void test_controls_and_bidi_become_the_box(void)
{
    uint64_t box = hash_of("a\xEF\xBF\xBD" "b"); /* an explicit U+FFFD: the box */
    uint64_t plain = hash_of("a" "b");

    TEST_ASSERT_TRUE(box != plain);
    /* the font HAS glyphs for these; the filter must still draw the box (CT-04) */
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\xAE" "b") == box, "U+202E (bidi override)");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\x8F" "b") == box, "U+200F (RLM)");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\x01" "b") == box, "C0 control with a real glyph");
    /* and the font does not have these: the box too */
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\t" "b") == box, "tab");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\x7F" "b") == box, "DEL");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xC2\x85" "b") == box, "C1 control");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x81\xA6" "b") == box, "U+2066 isolate");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\xAA" "b") == box, "U+202A embedding");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xD8\x9C" "b") == box, "U+061C ALM");
}

/* Every code point of the filtered set, and the ones just outside it (all have real glyphs). */
static void test_filter_boundaries_are_exact(void)
{
    uint64_t box = hash_of("a\xEF\xBF\xBD" "b");
    uint64_t solid = hash_of("aXb"); /* every real glyph is the same solid block */
    uint64_t blank = hash_of("a b");

    /* filtered: the box */
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\x7F" "b") == box, "U+007F");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xC2\x80" "b") == box, "U+0080");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xC2\x9F" "b") == box, "U+009F");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xD8\x9C" "b") == box, "U+061C");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\x8E" "b") == box, "U+200E");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\x8F" "b") == box, "U+200F");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\xAA" "b") == box, "U+202A");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\xAD" "b") == box, "U+202D");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\xAE" "b") == box, "U+202E");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x81\xA6" "b") == box, "U+2066");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x81\xA9" "b") == box, "U+2069");

    /* just outside: drawn normally */
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xC2\xA0" "b") == blank, "U+00A0 (blank, not a box)");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xD8\x9B" "b") == solid, "U+061B");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\x8D" "b") == solid, "U+200D");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\x90" "b") == solid, "U+2010");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\xA9" "b") == solid, "U+2029");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x80\xAF" "b") == solid, "U+202F");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x81\xA5" "b") == solid, "U+2065");
    TEST_ASSERT_TRUE_MESSAGE(hash_of("a\xE2\x81\xAA" "b") == solid, "U+206A");
    TEST_ASSERT_TRUE(box != solid && solid != blank);
}

static void test_invalid_utf8_becomes_one_box_per_sequence(void)
{
    uint64_t box = hash_of("a\xEF\xBF\xBD" "b");

    TEST_ASSERT_TRUE(hash_of("a\xFF" "b") == box);
    TEST_ASSERT_TRUE(hash_of("a\xC0\xAF" "b") != box); /* two bad bytes: two boxes */
    TEST_ASSERT_TRUE(hash_of("a\xE2\x82" "b") == box); /* one truncated sequence: ONE box */
    TEST_ASSERT_TRUE(hash_of("a\xED\xA0\x80" "b") != box); /* a surrogate is three bad bytes */
}

static void test_carriage_return_is_invisible(void)
{
    TEST_ASSERT_TRUE(hash_of("a\r" "b") == hash_of("a" "b"));
    TEST_ASSERT_TRUE(hash_of("\r\r\r") == hash_of(""));
}

/* ---- the text arena and unusable fonts (CT-01) ---- */

typedef struct {
    cydui_mem mem;
    cydui_ctx ctx;
    cydui_cmd cmds[16];
    cydui_color fb[TU_W * TU_H];
    cydui_color strip[TU_W * 4];
    char text[16];
} fixture;

static uint32_t init_fixture(fixture *f, size_t text_bytes)
{
    cydui_config cfg;

    cydui_mem_init(&f->mem, f->fb, TU_W, TU_H);
    memset(&cfg, 0, sizeof(cfg));
    cfg.backend = &f->mem.backend;
    cfg.cmd_mem = f->cmds;
    cfg.cmd_mem_size = sizeof(f->cmds);
    cfg.strip_mem = f->strip;
    cfg.strip_mem_size = sizeof(f->strip);
    cfg.text_mem = text_bytes ? f->text : NULL;
    cfg.text_mem_size = text_bytes;
    return cydui_init(&f->ctx, &cfg);
}

static void test_text_needs_text_memory(void)
{
    static fixture f;

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, 0));
    cydui_begin_frame(&f.ctx, 0);
    cydui_draw_text(&f.ctx, mono_font(), 0, 0, 0, CYDUI_ALIGN_LEFT, "hi", CYDUI_NTS, WHITE, 255);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_cmd_count(&f.ctx)); /* the command was taken back */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_NO_MEMORY, cydui_errors(&f.ctx));
    cydui_end_frame(&f.ctx); /* still renders */
}

static void test_text_arena_fills_and_resets_each_frame(void)
{
    static fixture f;

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, 10));
    cydui_begin_frame(&f.ctx, 0);
    cydui_draw_text(&f.ctx, mono_font(), 0, 0, 0, CYDUI_ALIGN_LEFT, "abcdef", CYDUI_NTS, WHITE, 255);
    TEST_ASSERT_EQUAL_UINT32(1, cydui_cmd_count(&f.ctx));
    cydui_draw_text(&f.ctx, mono_font(), 0, 20, 0, CYDUI_ALIGN_LEFT, "ghijkl", CYDUI_NTS, WHITE, 255);
    TEST_ASSERT_EQUAL_UINT32(1, cydui_cmd_count(&f.ctx)); /* no room: dropped, not corrupting */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_NO_MEMORY, cydui_errors(&f.ctx));
    cydui_end_frame(&f.ctx);
    TEST_ASSERT_EQUAL_HEX16(WHITE, f.fb[1 * TU_W + 1]); /* the first text did draw */

    cydui_clear_errors(&f.ctx);
    cydui_begin_frame(&f.ctx, 0); /* a new frame has the whole arena again */
    cydui_draw_text(&f.ctx, mono_font(), 0, 20, 0, CYDUI_ALIGN_LEFT, "ghijkl", CYDUI_NTS, WHITE, 255);
    TEST_ASSERT_EQUAL_UINT32(1, cydui_cmd_count(&f.ctx));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_errors(&f.ctx));
}

static void test_text_is_copied(void)
{
    static fixture f;
    char temp[8];

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, 16));
    cydui_begin_frame(&f.ctx, 0);
    strcpy(temp, "a");
    cydui_draw_text(&f.ctx, mono_font(), 0, 0, 0, CYDUI_ALIGN_LEFT, temp, CYDUI_NTS, WHITE, 255);
    memset(temp, ' ', sizeof(temp)); /* the caller reuses the buffer before the frame ends */
    temp[7] = 0;                     /* (a space has no ink, so an uncopied string draws nothing) */
    cydui_end_frame(&f.ctx);
    TEST_ASSERT_EQUAL_HEX16(WHITE, f.fb[0]);
}

static void test_unusable_font_and_invisible_text_are_handled(void)
{
    static fixture f;
    cydui_font bad = {0};

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, 16));
    cydui_begin_frame(&f.ctx, 0);
    cydui_draw_text(&f.ctx, NULL, 0, 0, 0, CYDUI_ALIGN_LEFT, "x", CYDUI_NTS, WHITE, 255);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_errors(&f.ctx));
    cydui_clear_errors(&f.ctx);
    cydui_draw_text(&f.ctx, &bad, 0, 0, 0, CYDUI_ALIGN_LEFT, "x", CYDUI_NTS, WHITE, 255);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_errors(&f.ctx));
    cydui_clear_errors(&f.ctx);

    cydui_draw_text(&f.ctx, mono_font(), 0, 0, 0, CYDUI_ALIGN_LEFT, "", CYDUI_NTS, WHITE, 255);
    cydui_draw_text(&f.ctx, mono_font(), 0, 0, 0, CYDUI_ALIGN_LEFT, NULL, 4, WHITE, 255);
    cydui_draw_text(&f.ctx, mono_font(), 0, 0, 0, CYDUI_ALIGN_LEFT, "x", CYDUI_NTS, WHITE, 0);
    cydui_draw_text(&f.ctx, mono_font(), 900, 900, 0, CYDUI_ALIGN_LEFT, "x", CYDUI_NTS, WHITE, 255);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_cmd_count(&f.ctx)); /* empty, transparent, off screen */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_errors(&f.ctx));
}

static void test_long_text_is_truncated_and_flagged(void)
{
    static fixture f;
    static char big[1000];

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, 16 * 1024 > sizeof(f.text) ? sizeof(f.text) : 16));
    memset(big, 'a', sizeof(big));
    cydui_begin_frame(&f.ctx, 0);
    /* with a 16 byte arena a 256 byte copy cannot fit: dropped (and the cut flagged first) */
    cydui_draw_text(&f.ctx, mono_font(), 0, 0, 0, CYDUI_ALIGN_LEFT, big, sizeof(big), WHITE, 255);
    TEST_ASSERT_TRUE(cydui_errors(&f.ctx) & CYDUI_ERR_TEXT_TRUNCATED);
    TEST_ASSERT_TRUE(cydui_errors(&f.ctx) & CYDUI_ERR_NO_MEMORY);
}

/* ---- malformed fonts must not read out of bounds (AB-16) ---- */

static const uint8_t tiny_bits[4] = {0xFF, 0xFF, 0xFF, 0xFF};

static void scene_malformed_fonts(cydui_ctx *c)
{
    /* each font is broken in a different way; the text is "abc!?" in all of them */
    static cydui_glyph ink_past_end[2] = {{'a', 0, 4, 6, 0, 6, 5}, {0xFFFD, 3, 4, 6, 0, 6, 5}};
    static cydui_glyph huge_offset[2] = {{'a', 0xFFFFFFF0u, 4, 6, 0, 6, 5}, {0xFFFD, 0, 2, 2, 0, 2, 5}};
    static cydui_glyph huge_size[2] = {{'a', 0, 255, 255, 0, 6, 5}, {0xFFFD, 0, 255, 255, -128, 127, 5}};
    static cydui_glyph wild_bearing[2] = {{'a', 0, 2, 2, -128, 127, 200}, {0xFFFD, 0, 2, 2, 127, -128, 0}};
    static cydui_glyph unsorted[3] = {{'z', 0, 2, 2, 0, 2, 5}, {'a', 0, 2, 2, 0, 2, 5}, {0xFFFD, 0, 2, 2, 0, 2, 5}};
    static const cydui_font fonts[] = {
        {ink_past_end, 2, tiny_bits, sizeof(tiny_bits), 8, 6, 1},    /* glyph data runs past the array */
        {huge_offset, 2, tiny_bits, sizeof(tiny_bits), 8, 6, 1},     /* offset that would wrap */
        {huge_size, 2, tiny_bits, sizeof(tiny_bits), 8, 6, 1},       /* 255 x 255 from 4 bytes */
        {wild_bearing, 2, tiny_bits, sizeof(tiny_bits), 1, 255, 1},  /* extreme metrics */
        {unsorted, 3, tiny_bits, sizeof(tiny_bits), 8, 6, 2},        /* binary search on a bad table */
        {ink_past_end, 2, NULL, 0, 8, 6, 1},                         /* glyphs with ink but no data */
        {ink_past_end, 2, tiny_bits, sizeof(tiny_bits), 0, 6, 1},    /* zero line height */
    };
    size_t i;

    for (i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
        cydui_draw_text(c, &fonts[i], 5, (int16_t)(5 + 30 * i), 60, CYDUI_ALIGN_CENTER, "abc!? a b c",
                        CYDUI_NTS, WHITE, 255);
    }
}

static void test_malformed_fonts_never_read_out_of_bounds(void)
{
    /* The assertion is the absence of an ASan/UBSan report; zero line height must not hang. */
    tu_render(scene_malformed_fonts, TU_H, BLACK);
    TEST_PASS();
}

/* ---- the image must not depend on the strip height ---- */

/* Ink lands 160 px BELOW and 120 px ABOVE the line box, though lines are only 4 px high. */
static cydui_glyph below_glyphs[2] = {{'a', 0, 3, 2, 0, -60, 5}, {0xFFFD, 0, 3, 2, 0, -60, 5}};
static cydui_glyph above_glyphs[2] = {{'a', 0, 3, 2, 0, 120, 5}, {0xFFFD, 0, 3, 2, 0, 120, 5}};
static const cydui_font far_below = {below_glyphs, 2, tiny_bits, sizeof(tiny_bits), 4, 100, 1};
static const cydui_font far_above = {above_glyphs, 2, tiny_bits, sizeof(tiny_bits), 4, 0, 1};

static void scene_far_ink(cydui_ctx *c)
{
    cydui_draw_text(c, &far_below, 20, 10, 0, CYDUI_ALIGN_LEFT, "aaaa", CYDUI_NTS, WHITE, 255);
    cydui_draw_text(c, &far_above, 60, 200, 0, CYDUI_ALIGN_LEFT, "aaaa", CYDUI_NTS, WHITE, 255);
    cydui_draw_text(c, &far_below, 100, 10, 30, CYDUI_ALIGN_LEFT, "aa aa aa aa", CYDUI_NTS, WHITE,
                    255); /* several lines, each with far ink */
    /* The line box is OFF the screen, only the ink is on it. */
    cydui_draw_text(c, &far_above, 140, 300, 0, CYDUI_ALIGN_LEFT, "aaaa", CYDUI_NTS, WHITE, 255);
    cydui_draw_text(c, &far_below, 180, -100, 0, CYDUI_ALIGN_LEFT, "aaaa", CYDUI_NTS, WHITE, 255);
}

static void test_ink_far_from_the_line_box_is_drawn(void)
{
    int y, x, below = 0, above = 0, off_above = 0, off_below = 0;

    tu_render(scene_far_ink, TU_H, BLACK);
    for (y = 0; y < TU_H; y++) {
        for (x = 20; x < 40; x++) {
            if (tu_px(x, y) == WHITE) { /* first text: line box at y=10..14, ink 160 lower */
                TEST_ASSERT_TRUE(y >= 10 + 100 + 60);
                below++;
            }
        }
        for (x = 60; x < 80; x++) {
            if (tu_px(x, y) == WHITE) { /* second text: line box at y=200..204, ink 120 higher */
                TEST_ASSERT_TRUE(y < 200);
                above++;
            }
        }
    }
    for (y = 0; y < TU_H; y++) {
        for (x = 140; x < 160; x++) {
            off_above += tu_px(x, y) == WHITE; /* box at y=300 (below the screen), ink at ~180 */
        }
        for (x = 180; x < 200; x++) {
            off_below += tu_px(x, y) == WHITE; /* box at y=-100 (above the screen), ink at ~60 */
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(off_above > 0, "text whose box is below the screen was culled");
    TEST_ASSERT_TRUE_MESSAGE(off_below > 0, "text whose box is above the screen was culled");
    TEST_ASSERT_TRUE_MESSAGE(below > 0, "ink far below the line box was culled");
    TEST_ASSERT_TRUE_MESSAGE(above > 0, "ink far above the line box was culled");
}

static void scene_text_everything(cydui_ctx *c)
{
    const cydui_font *f = mono_font();

    scene_far_ink(c);
    cydui_draw_text(c, f, 5, 5, 0, CYDUI_ALIGN_LEFT, "Hello, strips!", CYDUI_NTS, WHITE, 255);
    cydui_draw_text(c, f, 5, 30, 60, CYDUI_ALIGN_CENTER, "wrapped text in a narrow box that is long",
                    CYDUI_NTS, 0x07E0, 255);
    cydui_draw_text(c, f, 100, 30, 80, CYDUI_ALIGN_RIGHT, "right aligned\nand two lines", CYDUI_NTS,
                    0xF800, 200);
    cydui_draw_text(c, f, -12, 100, 0, CYDUI_ALIGN_LEFT, "partly off the left edge", CYDUI_NTS,
                    WHITE, 255);
    cydui_draw_text(c, f, 300, 110, 0, CYDUI_ALIGN_LEFT, "off the right", CYDUI_NTS, WHITE, 255);
    cydui_draw_text(c, f, 5, 230, 0, CYDUI_ALIGN_LEFT, "bottom edge", CYDUI_NTS, WHITE, 255);
    cydui_draw_text(c, f, 5, 120, 0, CYDUI_ALIGN_LEFT, "a\xE2\x80\xAE" "b\xFF" "c\t" "d", CYDUI_NTS,
                    WHITE, 255);
    cydui_set_clip(c, tu_rect(40, 150, 60, 20));
    cydui_draw_text(c, f, 30, 148, 0, CYDUI_ALIGN_LEFT, "clipped by a rectangle", CYDUI_NTS, WHITE,
                    255);
    cydui_reset_clip(c);
    scene_malformed_fonts(c); /* wild bearings and sizes: culling must stay exact */
}

static void test_strip_height_does_not_change_text(void)
{
    static const int heights[] = {1, 2, 3, 5, 7, 8, 13, 24, 40, 97, 239, 240};
    uint64_t ref = tu_render(scene_text_everything, TU_H, 0x0841);
    size_t i;

    for (i = 0; i < sizeof(heights) / sizeof(heights[0]); i++) {
        TEST_ASSERT_TRUE_MESSAGE(tu_render(scene_text_everything, heights[i], 0x0841) == ref,
                                 "text changed with the strip height");
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_size_of_simple_text);
    RUN_TEST(test_wrapping_at_spaces);
    RUN_TEST(test_words_wider_than_the_line_are_broken);
    RUN_TEST(test_newlines_and_edge_cases);
    RUN_TEST(test_layout_always_terminates);
    RUN_TEST(test_clip_len_limit_and_boundary);
    RUN_TEST(test_nul_search_is_bounded);
    RUN_TEST(test_alignment_positions_lines_in_the_box);
    RUN_TEST(test_glyph_is_drawn_where_the_metrics_say);
    RUN_TEST(test_controls_and_bidi_become_the_box);
    RUN_TEST(test_filter_boundaries_are_exact);
    RUN_TEST(test_invalid_utf8_becomes_one_box_per_sequence);
    RUN_TEST(test_ink_far_from_the_line_box_is_drawn);
    RUN_TEST(test_carriage_return_is_invisible);
    RUN_TEST(test_text_needs_text_memory);
    RUN_TEST(test_text_arena_fills_and_resets_each_frame);
    RUN_TEST(test_text_is_copied);
    RUN_TEST(test_unusable_font_and_invisible_text_are_handled);
    RUN_TEST(test_long_text_is_truncated_and_flagged);
    RUN_TEST(test_malformed_fonts_never_read_out_of_bounds);
    RUN_TEST(test_strip_height_does_not_change_text);
    return UNITY_END();
}
