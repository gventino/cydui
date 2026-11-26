/* Fonts by role in a theme, and the font, color and alpha modifiers of labels. */

#include <string.h>

#include "cydui_sans_12.h"
#include "cydui_sans_16.h"
#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig R;
static cydui_color saved[RIG_W * RIG_H];

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&R)); /* the 12 px font is the body */
}

static void keep(void)
{
    memcpy(saved, R.fb, sizeof(saved));
}

static bool same_as_kept(void)
{
    return memcmp(saved, R.fb, sizeof(saved)) == 0;
}

/* a frame in which `draw` runs, and the screen it leaves */
typedef void (*draw_fn)(cydui_ui *ui);
static void frame(draw_fn draw)
{
    cydui_ui_begin(&R.ui, 0);
    draw(&R.ui);
    cydui_ui_end(&R.ui);
}

/* ---- roles ---- */

void test_a_theme_gives_its_fonts_by_role_and_the_body_font_for_what_is_unset(void)
{
    cydui_theme t;
    cydui_theme_default(&t, &cydui_sans_12);
    TEST_ASSERT_EQUAL_PTR(&cydui_sans_12, cydui_theme_font(&t, CYDUI_FONT_BODY));
    TEST_ASSERT_EQUAL_PTR(&cydui_sans_12, cydui_theme_font(&t, CYDUI_FONT_SMALL));
    TEST_ASSERT_EQUAL_PTR(&cydui_sans_12, cydui_theme_font(&t, CYDUI_FONT_TITLE));
    t.style.font_title = &cydui_sans_16;
    TEST_ASSERT_EQUAL_PTR(&cydui_sans_16, cydui_theme_font(&t, CYDUI_FONT_TITLE));
    TEST_ASSERT_EQUAL_PTR(&cydui_sans_12, cydui_theme_font(&t, CYDUI_FONT_SMALL));
    t.style.font_small = &cydui_sans_16;
    TEST_ASSERT_EQUAL_PTR(&cydui_sans_16, cydui_theme_font(&t, CYDUI_FONT_SMALL));
    TEST_ASSERT_EQUAL_PTR(&cydui_sans_12, cydui_theme_font(&t, CYDUI_FONT_BODY));
    TEST_ASSERT_EQUAL_PTR(&cydui_sans_12, cydui_theme_font(&t, (cydui_font_role)99));
    TEST_ASSERT_NULL(cydui_theme_font(NULL, CYDUI_FONT_BODY));
}

void test_an_unusable_optional_font_makes_a_theme_invalid_and_a_missing_one_does_not(void)
{
    static const cydui_font empty = {NULL, 0, NULL, 0, 10, 8, 0};
    cydui_theme t;
    cydui_theme_default(&t, &cydui_sans_12);
    TEST_ASSERT_EQUAL_INT(CYDUI_THEME_OK, cydui_theme_validate(&t));
    t.style.font_small = &empty;
    TEST_ASSERT_EQUAL_INT(CYDUI_THEME_ERR_FONT, cydui_theme_validate(&t));
    t.style.font_small = NULL;
    t.style.font_title = &empty;
    TEST_ASSERT_EQUAL_INT(CYDUI_THEME_ERR_FONT, cydui_theme_validate(&t));
}

/* ---- the label modifiers ---- */

static void plain_label(cydui_ui *ui) { cydui_label(ui, "Quest log"); }
static void label_in_other_font(cydui_ui *ui) { cydui_next_font(ui, &cydui_sans_16); cydui_label(ui, "Quest log"); }
static void manual_label_other_font(cydui_ui *ui)
{
    const char *s = "Quest log";
    cydui_text_extent e = cydui_text_size(&cydui_sans_16, s, strlen(s), 0);
    cydui_rect r = cydui_alloc_rect(ui, e.w, e.h);
    cydui_draw_text(&ui->ctx, &cydui_sans_16, r.x, r.y, 0, CYDUI_ALIGN_LEFT, s, strlen(s), ui->theme->style.text, 255);
}

void test_a_label_in_another_font_is_drawn_as_that_font_would_be_and_differs_from_the_default(void)
{
    start();
    frame(plain_label);
    keep();
    frame(label_in_other_font);
    TEST_ASSERT_FALSE_MESSAGE(same_as_kept(), "another font looks different");
    keep();
    frame(manual_label_other_font);
    TEST_ASSERT_TRUE_MESSAGE(same_as_kept(), "and is what drawing it by hand gives");
}

static void two_labels_first_modified(cydui_ui *ui) { cydui_next_font(ui, &cydui_sans_16); cydui_next_color(ui, 0xF800); cydui_label(ui, "one"); cydui_label(ui, "two"); }
static void two_labels_first_by_hand(cydui_ui *ui)
{
    cydui_text_extent e = cydui_text_size(&cydui_sans_16, "one", 3, 0);
    cydui_rect r = cydui_alloc_rect(ui, e.w, e.h);
    cydui_draw_text(&ui->ctx, &cydui_sans_16, r.x, r.y, 0, CYDUI_ALIGN_LEFT, "one", 3, 0xF800, 255);
    cydui_label(ui, "two");
}

void test_the_modifiers_are_for_the_next_label_only(void)
{
    start();
    frame(two_labels_first_by_hand);
    keep();
    frame(two_labels_first_modified);
    TEST_ASSERT_TRUE(same_as_kept());
}

static void modifiers_then_a_button_then_a_label(cydui_ui *ui)
{
    cydui_next_font(ui, &cydui_sans_16);
    cydui_next_color(ui, 0x07E0);
    cydui_next_alpha(ui, 10);
    cydui_button(ui, "Go");
    cydui_label(ui, "after");
}
static void button_then_a_label(cydui_ui *ui) { cydui_button(ui, "Go"); cydui_label(ui, "after"); }

void test_a_widget_that_is_not_a_label_forgets_the_modifiers(void)
{
    start();
    frame(button_then_a_label);
    keep();
    frame(modifiers_then_a_button_then_a_label);
    TEST_ASSERT_TRUE_MESSAGE(same_as_kept(), "the button looks as always and the label after it is not modified");
}

static void red_label(cydui_ui *ui) { cydui_next_color(ui, 0xF800); cydui_label(ui, "red"); }
static void disabled_red_label(cydui_ui *ui) { cydui_next_disabled(ui); cydui_next_color(ui, 0xF800); cydui_label(ui, "red"); }
static void disabled_label(cydui_ui *ui) { cydui_next_disabled(ui); cydui_label(ui, "red"); }

void test_a_color_replaces_the_text_color_also_for_a_disabled_label(void)
{
    start();
    frame(red_label);
    keep();
    frame(disabled_red_label);
    TEST_ASSERT_TRUE(same_as_kept());
    frame(disabled_label);
    TEST_ASSERT_FALSE_MESSAGE(same_as_kept(), "a disabled label without a color is muted, not red");
}

static void nothing(cydui_ui *ui) { (void)ui; }
static void invisible_label(cydui_ui *ui) { cydui_next_alpha(ui, 0); cydui_label(ui, "ghost"); }
static void opaque_label(cydui_ui *ui) { cydui_next_alpha(ui, 255); cydui_label(ui, "ghost"); }
static void default_ghost(cydui_ui *ui) { cydui_label(ui, "ghost"); }
static void half_label(cydui_ui *ui) { cydui_next_alpha(ui, 128); cydui_label(ui, "ghost"); }

void test_alpha_zero_draws_nothing_255_is_the_default_and_in_between_is_in_between(void)
{
    start();
    frame(nothing);
    keep();
    frame(invisible_label);
    TEST_ASSERT_TRUE_MESSAGE(same_as_kept(), "alpha 0 leaves the empty screen");
    frame(default_ghost);
    keep();
    frame(opaque_label);
    TEST_ASSERT_TRUE_MESSAGE(same_as_kept(), "alpha 255 is the default");
    frame(half_label);
    TEST_ASSERT_FALSE(same_as_kept());
}

static void label_with_unusable_font(cydui_ui *ui)
{
    static const cydui_font empty = {NULL, 0, NULL, 0, 10, 8, 0};
    cydui_next_font(ui, &empty);
    cydui_label(ui, "Quest log");
}

void test_a_null_or_unusable_font_keeps_the_theme_font(void)
{
    start();
    frame(plain_label);
    keep();
    frame(label_with_unusable_font);
    TEST_ASSERT_TRUE(same_as_kept());
    cydui_next_font(&R.ui, NULL);
    frame(plain_label);
    TEST_ASSERT_TRUE(same_as_kept());
}

static const char *const LONG = "A long title that has to wrap over two or three lines of the screen";
static void wrapped_in_a_title_font(cydui_ui *ui)
{
    cydui_next_font(ui, &cydui_sans_16);
    cydui_next_color(ui, 0x001F);
    cydui_label_wrapped(ui, LONG);
}
static void wrapped_by_hand(cydui_ui *ui)
{
    int wrap = cydui_layout_avail(ui).w;
    cydui_text_extent e = cydui_text_size(&cydui_sans_16, LONG, strlen(LONG), (int16_t)wrap);
    cydui_rect r = cydui_alloc_rect(ui, e.w, e.h);
    cydui_rect clip = ui->ctx.clip;
    cydui_set_clip(&ui->ctx, cydui_rect_intersect(clip, r));
    cydui_draw_text(&ui->ctx, &cydui_sans_16, r.x, r.y, (int16_t)wrap, CYDUI_ALIGN_LEFT, LONG, strlen(LONG), 0x001F, 255);
    ui->ctx.clip = clip;
}

void test_a_wrapped_label_in_another_font_and_color_is_what_the_hand_made_widget_drew(void)
{
    start();
    frame(wrapped_by_hand);
    keep();
    frame(wrapped_in_a_title_font);
    TEST_ASSERT_TRUE(same_as_kept());
}

void test_no_error_comes_of_any_of_it(void)
{
    start();
    frame(wrapped_in_a_title_font);
    frame(modifiers_then_a_button_then_a_label);
    frame(label_with_unusable_font);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&R.ui));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_theme_gives_its_fonts_by_role_and_the_body_font_for_what_is_unset);
    RUN_TEST(test_an_unusable_optional_font_makes_a_theme_invalid_and_a_missing_one_does_not);
    RUN_TEST(test_a_label_in_another_font_is_drawn_as_that_font_would_be_and_differs_from_the_default);
    RUN_TEST(test_the_modifiers_are_for_the_next_label_only);
    RUN_TEST(test_a_widget_that_is_not_a_label_forgets_the_modifiers);
    RUN_TEST(test_a_color_replaces_the_text_color_also_for_a_disabled_label);
    RUN_TEST(test_alpha_zero_draws_nothing_255_is_the_default_and_in_between_is_in_between);
    RUN_TEST(test_a_null_or_unusable_font_keeps_the_theme_font);
    RUN_TEST(test_a_wrapped_label_in_another_font_and_color_is_what_the_hand_made_widget_drew);
    RUN_TEST(test_no_error_comes_of_any_of_it);
    return UNITY_END();
}
