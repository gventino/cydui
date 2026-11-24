/* Themes (FR-50, FR-51, FR-52, NFR-43, CT-09): validation, switching, the table, painters. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cydui_sans_12.h"
#include "cydui_sans_16.h"
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

/* ---- validation (CT-09) ---- */

static void test_validate_accepts_the_default_and_rejects_the_rest(void)
{
    cydui_theme t;
    cydui_font bad_font;

    cydui_theme_default(&t, &cydui_sans_12);
    TEST_ASSERT_EQUAL_INT(CYDUI_THEME_OK, cydui_theme_validate(&t));
    TEST_ASSERT_EQUAL_INT(CYDUI_THEME_ERR_NULL, cydui_theme_validate(NULL));

    t.style.font = NULL;
    TEST_ASSERT_EQUAL_INT(CYDUI_THEME_ERR_FONT, cydui_theme_validate(&t));

    memset(&bad_font, 0, sizeof(bad_font)); /* no glyphs: unusable */
    t.style.font = &bad_font;
    TEST_ASSERT_EQUAL_INT(CYDUI_THEME_ERR_FONT, cydui_theme_validate(&t));

    cydui_theme_default(&t, &cydui_sans_12);
    bad_font = cydui_sans_12;
    bad_font.fallback = bad_font.glyph_count; /* fallback out of range */
    t.style.font = &bad_font;
    TEST_ASSERT_EQUAL_INT(CYDUI_THEME_ERR_FONT, cydui_theme_validate(&t));
}

static void test_any_table_and_painter_values_are_valid(void)
{
    cydui_theme t;
    unsigned char *raw;
    size_t i;

    cydui_theme_default(&t, &cydui_sans_12);
    raw = (unsigned char *)t.table; /* garbage in every entry: still a valid theme */
    for (i = 0; i < sizeof(t.table); i++) {
        raw[i] = (unsigned char)(i * 37u + 11u);
    }
    TEST_ASSERT_EQUAL_INT(CYDUI_THEME_OK, cydui_theme_validate(&t));
}

static void test_init_refuses_an_invalid_theme(void)
{
    cydui_ui_config cfg;
    cydui_ui ui;

    start();
    memset(&cfg, 0, sizeof(cfg));
    cfg.core.backend = &rig.mem.backend;
    cfg.core.cmd_mem = rig.cmds;
    cfg.core.cmd_mem_size = sizeof(rig.cmds);
    cfg.core.strip_mem = rig.strip;
    cfg.core.strip_mem_size = sizeof(rig.strip);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_ui_init(&ui, &cfg)); /* no theme */
    rig.theme.style.font = NULL;
    cfg.theme = &rig.theme;
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_ui_init(&ui, &cfg));
}

/* ---- the default theme's table ---- */

static void test_default_table_follows_the_rules_of_the_style(void)
{
    cydui_theme t;
    const cydui_style *s;

    cydui_theme_default(&t, &cydui_sans_12);
    s = &t.style;
    TEST_ASSERT_EQUAL_HEX16(s->accent, t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL].fill);
    TEST_ASSERT_EQUAL_HEX16(cydui_blend(s->accent, 0, 90), t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_PRESSED].fill);
    TEST_ASSERT_EQUAL_HEX16(s->panel, t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_DISABLED].fill);
    TEST_ASSERT_EQUAL_HEX16(s->text_muted, t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_DISABLED].text);
    TEST_ASSERT_EQUAL_UINT8(s->radius, t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL].radius);

    TEST_ASSERT_EQUAL_UINT8(2, t.table[CYDUI_WIDGET_CHECKBOX][0][CYDUI_STATE_NORMAL].border_w);
    TEST_ASSERT_EQUAL_UINT8(0, t.table[CYDUI_WIDGET_CHECKBOX][1][CYDUI_STATE_NORMAL].border_w);
    TEST_ASSERT_EQUAL_HEX16(s->accent, t.table[CYDUI_WIDGET_CHECKBOX][1][CYDUI_STATE_NORMAL].fill);
    TEST_ASSERT_EQUAL_HEX16(cydui_blend(s->bg, s->accent, 110), t.table[CYDUI_WIDGET_CHECKBOX][1][CYDUI_STATE_DISABLED].fill);

    TEST_ASSERT_EQUAL_HEX16(s->border, t.table[CYDUI_WIDGET_TOGGLE][0][CYDUI_STATE_NORMAL].fill);
    TEST_ASSERT_EQUAL_HEX16(s->accent, t.table[CYDUI_WIDGET_TOGGLE][1][CYDUI_STATE_NORMAL].fill);
    TEST_ASSERT_EQUAL_UINT8(255, t.table[CYDUI_WIDGET_TOGGLE][1][CYDUI_STATE_NORMAL].radius); /* a pill */
    TEST_ASSERT_EQUAL_UINT8(255, t.table[CYDUI_WIDGET_TOGGLE][0][CYDUI_STATE_NORMAL].radius);
    TEST_ASSERT_EQUAL_UINT8(255, t.table[CYDUI_WIDGET_TOGGLE][0][CYDUI_STATE_DISABLED].radius);

    /* hover and focus have a place and, for now, look like normal */
    TEST_ASSERT_EQUAL_MEMORY(&t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL],
                             &t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_HOVER], sizeof(cydui_wstyle));
    TEST_ASSERT_EQUAL_MEMORY(&t.table[CYDUI_WIDGET_TOGGLE][1][CYDUI_STATE_NORMAL],
                             &t.table[CYDUI_WIDGET_TOGGLE][1][CYDUI_STATE_FOCUSED], sizeof(cydui_wstyle));
}

static void test_a_new_palette_gives_a_new_table(void)
{
    cydui_theme t;

    cydui_theme_default(&t, &cydui_sans_12);
    t.style.accent = CYDUI_RGB(200, 40, 40);
    cydui_theme_derive_table(&t);
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(200, 40, 40), t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL].fill);
    TEST_ASSERT_EQUAL_HEX16(cydui_blend(CYDUI_RGB(200, 40, 40), 0, 90),
                            t.table[CYDUI_WIDGET_TOGGLE][1][CYDUI_STATE_PRESSED].fill);
}

/* A theme in a heap block of exactly its size: ASan sees any read past the table. */
static void test_painting_with_out_of_range_kinds_and_states_stays_inside_the_theme(void)
{
    cydui_theme *t = (cydui_theme *)malloc(sizeof(*t));
    cydui_paint_info pi;
    int k, st;

    TEST_ASSERT_NOT_NULL(t);
    start();
    cydui_theme_default(t, &cydui_sans_12);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, t));
    cydui_ui_begin(&rig.ui, 0);
    for (k = -3; k < 14; k++) { /* every kind there is, and some that are not */
        for (st = -3; st < 300; st += (st < 9 ? 1 : 13)) { /* every state, then far out */
            cydui_wstyle copy;
            memset(&pi, 0, sizeof(pi));
            pi.kind = (cydui_widget_kind)k;
            pi.state = (cydui_widget_state)st;
            pi.selected = ((k + st) & 1) != 0;
            pi.rect = (cydui_rect){10, 10, 40, 30};
            cydui_paint(&rig.ui, &pi);
            copy = *cydui_wstyle_of(&rig.ui, pi.kind, pi.state, pi.selected); /* actually READ the entry */
            TEST_ASSERT_TRUE(copy.border_w <= 255);
        }
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &rig.theme)); /* let go of it before freeing */
    free(t);
}

/* ---- the table decides what a widget looks like (NFR-43) ---- */

static void one_button(void)
{
    cydui_ui_begin(&rig.ui, 0);
    cydui_button(&rig.ui, "Add");
    cydui_ui_end(&rig.ui);
}

static void test_changing_the_table_changes_the_pixels(void)
{
    start();
    one_button();
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent, rig.fb[24 * RIG_W + 12]); /* inside, left of the text */

    rig.theme.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL].fill = CYDUI_RGB(255, 0, 0);
    one_button();
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(255, 0, 0), rig.fb[24 * RIG_W + 12]);

    rig.theme.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL].border_w = 3; /* an outline appears */
    rig.theme.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL].border = CYDUI_RGB(0, 255, 0);
    one_button();
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(0, 255, 0), rig.fb[24 * RIG_W + 9]); /* the outline, 1 px in from the left edge */
}

static void test_the_label_of_a_button_is_drawn_in_the_text_color_of_the_table(void)
{
    uint64_t white_text, red_text, green_text;

    start();
    one_button();
    white_text = screen_hash();
    rig.theme.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL].text = CYDUI_RGB(255, 0, 0);
    one_button();
    red_text = screen_hash();
    rig.theme.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL].text = CYDUI_RGB(0, 255, 0);
    one_button();
    green_text = screen_hash();
    TEST_ASSERT_TRUE_MESSAGE(white_text != red_text && red_text != green_text,
                             "the label does not follow the text color of the table");
    /* only the label changed: the body of the button is the same (the antialiased glyph pixels differ) */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent, rig.fb[24 * RIG_W + 12]);
}

/* The check mark belongs to a checked box only; the switch is a pill; the knob follows the value. */
static void test_the_check_mark_the_pill_and_the_knob(void)
{
    bool off = false, on = true;
    const cydui_color panel = rig.theme.style.panel;
    (void)panel;

    start();
    cydui_ui_begin(&rig.ui, 0);
    cydui_checkbox(&rig.ui, "", &off); /* the box at (8,14) 20 x 20: the mark passes through (16,27) */
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.panel, rig.fb[27 * RIG_W + 16]);

    cydui_ui_begin(&rig.ui, 0);
    cydui_checkbox(&rig.ui, "", &on);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent_text, rig.fb[27 * RIG_W + 16]);

    cydui_ui_begin(&rig.ui, 0);
    cydui_toggle(&rig.ui, "", &off); /* the track is 44 x 24 at (8,12) */
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_HEX16(0, rig.fb[12 * RIG_W + 8]);                     /* a corner pixel: round, so background */
    TEST_ASSERT_EQUAL_HEX16(0, rig.fb[35 * RIG_W + 51]);                    /* and the opposite one */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent_text, rig.fb[24 * RIG_W + 20]); /* the knob on the left */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.border, rig.fb[24 * RIG_W + 40]);

    cydui_ui_begin(&rig.ui, 0);
    cydui_toggle(&rig.ui, "", &on);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent_text, rig.fb[24 * RIG_W + 40]); /* the knob on the right */
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent, rig.fb[24 * RIG_W + 20]);
}

static void test_pressed_and_disabled_use_their_own_entries(void)
{
    start();
    rig.theme.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_PRESSED].fill = CYDUI_RGB(1, 2, 3);
    rig.theme.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_DISABLED].fill = CYDUI_RGB(4, 5, 6);

    rig_down(&rig, TAP_X, TAP_Y);
    one_button();
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(1, 2, 3) & 0xFFFF, rig.fb[24 * RIG_W + 12]);

    rig_up(&rig, 250, 200);
    one_button();
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_disabled(&rig.ui);
    cydui_button(&rig.ui, "Add");
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_HEX16(CYDUI_RGB(4, 5, 6), rig.fb[24 * RIG_W + 12]);
}

/* ---- painters (FR-51) ---- */

typedef struct {
    cydui_widget_kind kind;
    cydui_widget_state state;
    bool selected;
    cydui_rect rect;
    const cydui_theme *theme;
} call_t;

static call_t calls[32];
static int ncalls;

static void recording_painter(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *info)
{
    (void)ui;
    if (ncalls < 32) {
        calls[ncalls].kind = info->kind;
        calls[ncalls].state = info->state;
        calls[ncalls].selected = info->selected;
        calls[ncalls].rect = info->rect;
        calls[ncalls].theme = t;
        ncalls++;
    }
}

static void use_recording_painters(void)
{
    cydui_widget_kind k;
    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) {
        rig.theme.paint[k] = recording_painter;
    }
}

static void test_a_painter_is_called_with_what_it_needs(void)
{
    bool on = true, off = false;

    start();
    use_recording_painters();
    ncalls = 0;
    cydui_ui_begin(&rig.ui, 0);
    cydui_button(&rig.ui, "Add");
    cydui_next_disabled(&rig.ui);
    cydui_button(&rig.ui, "Del");
    cydui_checkbox(&rig.ui, "A", &on);
    cydui_checkbox(&rig.ui, "B", &off);
    cydui_toggle(&rig.ui, "C", &on);
    cydui_ui_end(&rig.ui);

    TEST_ASSERT_EQUAL_INT(5, ncalls);
    TEST_ASSERT_EQUAL_INT(CYDUI_WIDGET_BUTTON, calls[0].kind);
    TEST_ASSERT_EQUAL_INT(CYDUI_STATE_NORMAL, calls[0].state);
    TEST_ASSERT_FALSE(calls[0].selected);
    TEST_ASSERT_EQUAL_INT(8, calls[0].rect.x);
    TEST_ASSERT_EQUAL_INT(8, calls[0].rect.y);
    TEST_ASSERT_EQUAL_INT(32, calls[0].rect.h);
    TEST_ASSERT_TRUE(calls[0].theme == &rig.theme);

    TEST_ASSERT_EQUAL_INT(CYDUI_STATE_DISABLED, calls[1].state);
    TEST_ASSERT_EQUAL_INT(8 + 32 + 8, calls[1].rect.y); /* the second widget, one gap below */

    TEST_ASSERT_EQUAL_INT(CYDUI_WIDGET_CHECKBOX, calls[2].kind);
    TEST_ASSERT_TRUE(calls[2].selected);
    TEST_ASSERT_EQUAL_INT(20, calls[2].rect.w); /* the box only, not the label */
    TEST_ASSERT_EQUAL_INT(20, calls[2].rect.h);
    TEST_ASSERT_FALSE(calls[3].selected);

    TEST_ASSERT_EQUAL_INT(CYDUI_WIDGET_TOGGLE, calls[4].kind);
    TEST_ASSERT_EQUAL_INT(44, calls[4].rect.w);
    TEST_ASSERT_EQUAL_INT(24, calls[4].rect.h);
    TEST_ASSERT_TRUE(calls[4].selected);
}

static void test_a_painter_sees_the_pressed_state(void)
{
    start();
    use_recording_painters();
    rig_down(&rig, TAP_X, TAP_Y);
    ncalls = 0;
    one_button();
    TEST_ASSERT_EQUAL_INT(1, ncalls);
    TEST_ASSERT_EQUAL_INT(CYDUI_STATE_PRESSED, calls[0].state);
}

static void painter_that_draws_nothing(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *info)
{
    (void)ui;
    (void)t;
    (void)info;
}

static void painter_that_defers(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *info)
{
    cydui_paint_default(ui, t, info);
}

static void test_painters_replace_or_extend_the_default(void)
{
    uint64_t plain;

    start();
    one_button();
    plain = screen_hash();

    rig.theme.paint[CYDUI_WIDGET_BUTTON] = painter_that_defers; /* the same look through the override */
    one_button();
    TEST_ASSERT_TRUE(screen_hash() == plain);

    rig.theme.paint[CYDUI_WIDGET_BUTTON] = painter_that_draws_nothing; /* a button that is not drawn */
    one_button();
    TEST_ASSERT_TRUE(screen_hash() != plain);
    TEST_ASSERT_EQUAL_HEX16(0, rig.fb[24 * RIG_W + 12]);

    rig.theme.paint[CYDUI_WIDGET_BUTTON] = NULL; /* NULL falls back to the default */
    one_button();
    TEST_ASSERT_TRUE(screen_hash() == plain);
}

static void test_painters_are_per_widget(void)
{
    bool v = false;

    start();
    rig.theme.paint[CYDUI_WIDGET_TOGGLE] = painter_that_draws_nothing;
    cydui_ui_begin(&rig.ui, 0);
    cydui_button(&rig.ui, "Add");
    cydui_toggle(&rig.ui, "T", &v);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent, rig.fb[24 * RIG_W + 12]); /* the button is still drawn */
    TEST_ASSERT_EQUAL_HEX16(0, rig.fb[(8 + 32 + 8 + 16) * RIG_W + 20]);        /* the toggle is not */
}

/* ---- cydui_paint ---- */

static void test_paint_draws_a_widget_in_a_forced_state(void)
{
    cydui_paint_info pi;
    uint64_t normal, pressed, disabled;

    start();
    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_BUTTON;
    pi.rect = (cydui_rect){10, 10, 80, 32};
    pi.text = "Hi";
    pi.text_len = 2;
    pi.text_extent = cydui_text_size(rig.theme.style.font, "Hi", 2, 0);

    cydui_ui_begin(&rig.ui, 0);
    cydui_paint(&rig.ui, &pi);
    cydui_ui_end(&rig.ui);
    normal = screen_hash();
    TEST_ASSERT_EQUAL_HEX16(rig.theme.style.accent, rig.fb[26 * RIG_W + 14]);

    pi.state = CYDUI_STATE_PRESSED;
    cydui_ui_begin(&rig.ui, 0);
    cydui_paint(&rig.ui, &pi);
    cydui_ui_end(&rig.ui);
    pressed = screen_hash();

    pi.state = CYDUI_STATE_DISABLED;
    cydui_ui_begin(&rig.ui, 0);
    cydui_paint(&rig.ui, &pi);
    cydui_ui_end(&rig.ui);
    disabled = screen_hash();

    TEST_ASSERT_TRUE(normal != pressed && pressed != disabled && normal != disabled);
}

static void test_paint_is_safe_with_any_arguments(void)
{
    cydui_paint_info pi;
    cydui_widget_kind kinds[] = {CYDUI_WIDGET_BUTTON, CYDUI_WIDGET_CHECKBOX, CYDUI_WIDGET_TOGGLE,
                                 CYDUI_WIDGET_COUNT, (cydui_widget_kind)99, (cydui_widget_kind)-1};
    cydui_rect rects[] = {{0, 0, 0, 0}, {-50, -50, 20, 20}, {300, 200, 500, 500}, {0, 0, 32767, 32767},
                          {10, 10, -5, -5}, {0, 0, 1, 1}};
    size_t k, r, st;

    start();
    memset(&rig.theme.table, 0xFF, sizeof(rig.theme.table)); /* radius, border and shadow all 255 */
    cydui_ui_begin(&rig.ui, 0);
    for (k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
        for (r = 0; r < sizeof(rects) / sizeof(rects[0]); r++) {
            for (st = 0; st < 8; st++) { /* includes states that do not exist */
                memset(&pi, 0, sizeof(pi));
                pi.kind = kinds[k];
                pi.state = (cydui_widget_state)st;
                pi.selected = (st & 1u) != 0;
                pi.rect = rects[r];
                pi.text = "x";
                pi.text_len = 1;
                pi.text_extent = cydui_text_size(rig.theme.style.font, "x", 1, 0);
                cydui_paint(&rig.ui, &pi);
            }
        }
    }
    cydui_paint(&rig.ui, NULL); /* nothing to paint: ignored */
    cydui_paint_default(&rig.ui, &rig.theme, NULL);
    cydui_paint_default(NULL, &rig.theme, &pi);
    cydui_paint_default(&rig.ui, NULL, &pi);
    cydui_ui_end(&rig.ui);
}

static void test_wstyle_of_never_returns_null_and_matches_the_table(void)
{
    start();
    TEST_ASSERT_TRUE(cydui_wstyle_of(&rig.ui, CYDUI_WIDGET_BUTTON, CYDUI_STATE_PRESSED, false) ==
                     &rig.theme.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_PRESSED]);
    TEST_ASSERT_TRUE(cydui_wstyle_of(&rig.ui, CYDUI_WIDGET_TOGGLE, CYDUI_STATE_NORMAL, true) ==
                     &rig.theme.table[CYDUI_WIDGET_TOGGLE][1][CYDUI_STATE_NORMAL]);
    TEST_ASSERT_NOT_NULL(cydui_wstyle_of(&rig.ui, (cydui_widget_kind)77, (cydui_widget_state)77, true));
    TEST_ASSERT_TRUE(cydui_theme_of(&rig.ui) == &rig.theme);
}

/* ---- switching at run time (FR-52) ---- */

static cydui_theme other;

static void test_switching_theme_changes_the_look_and_asks_for_a_frame(void)
{
    uint64_t a, b;

    start();
    cydui_theme_default(&other, &cydui_sans_16); /* a different font and palette */
    other.style.accent = CYDUI_RGB(200, 60, 60);
    cydui_theme_derive_table(&other);

    one_button();
    a = screen_hash();
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &other));
    TEST_ASSERT_TRUE(cydui_theme_of(&rig.ui) == &other);
    cydui_ui_begin(&rig.ui, 0);
    cydui_button(&rig.ui, "Add");
    TEST_ASSERT_TRUE_MESSAGE(cydui_ui_end(&rig.ui), "a theme switch must ask for another frame");
    b = screen_hash();
    TEST_ASSERT_TRUE(a != b);
    TEST_ASSERT_FALSE(rig_empty_frame(&rig)); /* and then it is quiet again */

    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &rig.theme)); /* back: the same pixels as at first */
    one_button();
    one_button();
    TEST_ASSERT_TRUE(screen_hash() == a);
}

static void test_an_invalid_theme_is_refused_and_the_current_one_stays(void)
{
    cydui_theme bad;
    uint64_t before;

    start();
    one_button();
    before = screen_hash();
    cydui_theme_default(&bad, &cydui_sans_12);
    bad.style.font = NULL;
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_set_theme(&rig.ui, &bad));
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_BAD_ARG);
    TEST_ASSERT_TRUE(cydui_theme_of(&rig.ui) == &rig.theme);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_set_theme(&rig.ui, NULL));
    cydui_ui_clear_errors(&rig.ui);
    one_button();
    TEST_ASSERT_TRUE(screen_hash() == before);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static int used_state_entries(void)
{
    int i, n = 0;
    for (i = 0; i < (int)rig.ui.state_cap; i++) {
        n += rig.ui.state[i].used;
    }
    return n;
}

/* A theme is only a pointer: switching it over and over leaks nothing and disturbs no state. */
static void test_a_thousand_switches_change_nothing_else(void)
{
    int i, used_before, used_after;
    uint64_t first = 0, last;

    start();
    cydui_theme_default(&other, &cydui_sans_12);
    other.style.accent = CYDUI_RGB(30, 160, 90);
    cydui_theme_derive_table(&other);

    for (i = 0; i < 6; i++) { /* settle a fill, so that the state table has something in it */
        cydui_ui_begin(&rig.ui, 0);
        CYDUI_ROW(&rig.ui) {
            cydui_button(&rig.ui, "Add");
            cydui_spacer_fill(&rig.ui);
            cydui_label(&rig.ui, "x");
        }
        cydui_ui_end(&rig.ui);
    }
    used_before = used_state_entries();
    TEST_ASSERT_TRUE(used_before > 0);

    for (i = 0; i < 1000; i++) {
        TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, (i & 1) ? &other : &rig.theme));
        cydui_ui_begin(&rig.ui, 0);
        cydui_button(&rig.ui, "Add");
        cydui_ui_end(&rig.ui);
        if (i == 1) first = screen_hash();
    }
    cydui_set_theme(&rig.ui, &other);
    one_button();
    last = screen_hash();
    used_after = used_state_entries();

    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui) & ~(uint32_t)CYDUI_ERR_LAYOUT);
    TEST_ASSERT_TRUE(used_after <= used_before + 1); /* nothing piled up */
    TEST_ASSERT_TRUE(first != 0 && last != 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_validate_accepts_the_default_and_rejects_the_rest);
    RUN_TEST(test_any_table_and_painter_values_are_valid);
    RUN_TEST(test_init_refuses_an_invalid_theme);
    RUN_TEST(test_default_table_follows_the_rules_of_the_style);
    RUN_TEST(test_a_new_palette_gives_a_new_table);
    RUN_TEST(test_painting_with_out_of_range_kinds_and_states_stays_inside_the_theme);
    RUN_TEST(test_changing_the_table_changes_the_pixels);
    RUN_TEST(test_the_label_of_a_button_is_drawn_in_the_text_color_of_the_table);
    RUN_TEST(test_the_check_mark_the_pill_and_the_knob);
    RUN_TEST(test_pressed_and_disabled_use_their_own_entries);
    RUN_TEST(test_a_painter_is_called_with_what_it_needs);
    RUN_TEST(test_a_painter_sees_the_pressed_state);
    RUN_TEST(test_painters_replace_or_extend_the_default);
    RUN_TEST(test_painters_are_per_widget);
    RUN_TEST(test_paint_draws_a_widget_in_a_forced_state);
    RUN_TEST(test_paint_is_safe_with_any_arguments);
    RUN_TEST(test_wstyle_of_never_returns_null_and_matches_the_table);
    RUN_TEST(test_switching_theme_changes_the_look_and_asks_for_a_frame);
    RUN_TEST(test_an_invalid_theme_is_refused_and_the_current_one_stays);
    RUN_TEST(test_a_thousand_switches_change_nothing_else);
    return UNITY_END();
}
