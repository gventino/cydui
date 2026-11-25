/*
 * Popups (FR-35): shown above everything wherever they are called, they take the touch from what is outside, close on a
 * press outside or Escape unless they are modal, take no room in their container, and cannot be nested or left open.
 *
 * Geometry on the 320 x 240 rig: a popup of 200 x 100 is the rectangle (60, 70, 200, 100); its content starts at the
 * padding of the theme (14, 8) inside it, at (74, 78).
 */

#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;
static bool open_;
static unsigned popup_flags;
static bool before_clicked, inner_clicked, after_clicked;
static bool popup_first; /* build the popup before the page, or after it */
static int built;

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    open_ = false;
    popup_flags = 0;
    popup_first = false;
    before_clicked = inner_clicked = after_clicked = false;
    built = 0;
}

static void popup(void)
{
    if (cydui_popup_begin(&rig.ui, "Dialog", &open_, 200, 100, popup_flags)) {
        built++;
        if (cydui_button(&rig.ui, "Ok")) inner_clicked = true;
        cydui_popup_end(&rig.ui);
    }
}

/* A big button (the whole width, where the popup will be) under the popup, and one at the end. */
static void frame(void)
{
    cydui_ui_begin(&rig.ui, 0);
    if (popup_first) popup();
    cydui_next_stretch(&rig.ui);
    cydui_next_height(&rig.ui, 200);
    if (cydui_button(&rig.ui, "Ok")) before_clicked = true; /* the same label as the one inside: no collision */
    if (!popup_first) popup();
    if (cydui_button(&rig.ui, "Later")) after_clicked = true;
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
    frames(2);
}

static cydui_color pixel(int x, int y)
{
    return rig.fb[y * RIG_W + x];
}

static void test_a_closed_popup_builds_nothing_and_an_open_one_does(void)
{
    start();
    frames(2);
    TEST_ASSERT_EQUAL_INT(0, built);
    open_ = true;
    frames(2);
    TEST_ASSERT_EQUAL_INT(2, built);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_the_content_reacts_to_the_touch(void)
{
    start();
    open_ = true;
    frames(3);
    tap(74 + 10, 78 + 10); /* the button inside */
    TEST_ASSERT_TRUE(inner_clicked);
    TEST_ASSERT_FALSE(before_clicked);
}

static void test_what_is_outside_does_not_react_while_it_is_open(void)
{
    start();
    open_ = true;
    popup_flags = CYDUI_POPUP_MODAL;
    frames(3);
    tap(20, 40); /* the big button below, outside the panel */
    tap(20, 215); /* "Later" is the widget after it */
    TEST_ASSERT_FALSE(before_clicked);
    TEST_ASSERT_FALSE(after_clicked);
    TEST_ASSERT_TRUE(open_);
    open_ = false;
    frames(3);
    tap(20, 40);
    TEST_ASSERT_TRUE(before_clicked); /* and it does again once the popup is gone */
}

static void test_a_press_outside_closes_a_popup_that_is_not_modal_and_not_a_modal_one(void)
{
    start();
    open_ = true;
    frames(3);
    tap(20, 40);
    TEST_ASSERT_FALSE(open_);
    TEST_ASSERT_FALSE(before_clicked); /* the press that closed it did nothing else */

    open_ = true;
    popup_flags = CYDUI_POPUP_MODAL;
    frames(3);
    tap(20, 40);
    TEST_ASSERT_TRUE(open_);
}

static void test_a_press_inside_the_panel_does_not_close_it(void)
{
    start();
    open_ = true;
    frames(3);
    tap(70, 160); /* the panel, away from the button */
    TEST_ASSERT_TRUE(open_);
}

static void test_escape_closes_a_popup_that_is_not_modal(void)
{
    start();
    open_ = true;
    frames(3);
    rig_key(&rig, CYDUI_KEY_ESCAPE);
    frame();
    TEST_ASSERT_FALSE(open_);

    open_ = true;
    popup_flags = CYDUI_POPUP_MODAL;
    frames(3);
    rig_key(&rig, CYDUI_KEY_ESCAPE);
    frame();
    TEST_ASSERT_TRUE(open_);
}

static void test_it_is_above_everything_wherever_it_is_called(void)
{
    uint64_t after_page, before_page;
    cydui_color c;

    start();
    open_ = true;
    frames(3);
    after_page = cydui_mem_hash(&rig.mem);
    c = pixel(70, 160); /* the panel, away from the button: the panel color, not the big button's */
    TEST_ASSERT_EQUAL_UINT16(rig.theme.style.panel, c);

    start();
    open_ = true;
    popup_first = true;
    frames(3);
    before_page = cydui_mem_hash(&rig.mem);
    TEST_ASSERT_EQUAL_UINT64(after_page, before_page);
}

static void test_what_is_behind_is_dimmed(void)
{
    cydui_color dimmed, plain;

    start();
    frames(3);
    plain = pixel(20, 40); /* the big button */
    open_ = true;
    frames(3);
    dimmed = pixel(20, 40);
    TEST_ASSERT_TRUE(plain != dimmed);
    TEST_ASSERT_TRUE(dimmed < plain || (dimmed & 0xF81Fu) != (plain & 0xF81Fu)); /* it is darker, not just different */
}

static void test_it_takes_no_room_in_its_container_and_is_not_clipped_by_it(void)
{
    cydui_rect before, after;

    start();
    frames(2);
    open_ = true;
    cydui_ui_begin(&rig.ui, 0);
    cydui_begin_column(&rig.ui);
    before = cydui_layout_avail(&rig.ui);
    popup();
    after = cydui_layout_avail(&rig.ui);
    cydui_end_column(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT(before.x, after.x);
    TEST_ASSERT_EQUAL_INT(before.y, after.y);
    TEST_ASSERT_EQUAL_INT(before.w, after.w);
    TEST_ASSERT_EQUAL_INT(before.h, after.h);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));

    /* in a scroll area whose window is far smaller than the popup */
    cydui_ui_begin(&rig.ui, 0);
    cydui_next_height(&rig.ui, 30);
    cydui_scroll_begin(&rig.ui, "box", 0);
    popup();
    cydui_scroll_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_EQUAL_UINT16(rig.theme.style.panel, pixel(70, 160));
}

static void test_only_one_popup_a_second_in_the_frame_is_refused(void)
{
    bool a = true, b = true;

    start();
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_TRUE(cydui_popup_begin(&rig.ui, "A", &a, 100, 60, 0));
    cydui_popup_end(&rig.ui);
    TEST_ASSERT_FALSE(cydui_popup_begin(&rig.ui, "B", &b, 100, 60, 0));
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE((cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT) != 0);
}

static void test_a_nested_popup_is_refused_and_a_missing_end_is_reported(void)
{
    bool a = true, b = true;

    start();
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_TRUE(cydui_popup_begin(&rig.ui, "A", &a, 100, 60, 0));
    TEST_ASSERT_FALSE(cydui_popup_begin(&rig.ui, "B", &b, 100, 60, 0));
    cydui_popup_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE((cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT) != 0);

    cydui_ui_clear_errors(&rig.ui);
    cydui_ui_begin(&rig.ui, 0);
    cydui_popup_end(&rig.ui); /* an end without a begin */
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_TRUE((cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT) != 0);

    cydui_ui_clear_errors(&rig.ui);
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_TRUE(cydui_popup_begin(&rig.ui, "A", &a, 100, 60, 0));
    cydui_ui_end(&rig.ui); /* never ended */
    TEST_ASSERT_TRUE((cydui_ui_errors(&rig.ui) & CYDUI_ERR_LAYOUT) != 0);
    cydui_ui_clear_errors(&rig.ui);
    frames(2);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui)); /* and the next frame is fine */
}

static void test_the_size_is_clamped_and_nothing_breaks_with_odd_arguments(void)
{
    bool o = true;

    start();
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_TRUE(cydui_popup_begin(&rig.ui, "Big", &o, 100000, 100000, 0));
    cydui_popup_end(&rig.ui);
    cydui_ui_end(&rig.ui);
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_TRUE(cydui_popup_begin(&rig.ui, NULL, &o, -5, 0, 0));
    cydui_popup_end(&rig.ui);
    TEST_ASSERT_FALSE(cydui_popup_begin(&rig.ui, "x", NULL, 100, 60, 0));
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_the_container_depth_limit_leaves_everything_balanced(void)
{
    bool o = true;
    int i;

    start();
    cydui_ui_begin(&rig.ui, 0);
    for (i = 0; i < CYDUI_MAX_LAYOUT_DEPTH; i++) cydui_begin_column(&rig.ui); /* as deep as it goes */
    if (cydui_popup_begin(&rig.ui, "Deep", &o, 100, 60, 0)) {
        cydui_popup_end(&rig.ui);
    }
    for (i = 0; i < CYDUI_MAX_LAYOUT_DEPTH; i++) cydui_end_column(&rig.ui);
    cydui_ui_end(&rig.ui);
    cydui_ui_clear_errors(&rig.ui);
    frames(2);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_a_text_field_in_a_popup_works_and_keeps_the_focus(void)
{
    static char text[16];
    bool o = true;
    int i;

    start();
    memset(text, 0, sizeof(text));
    for (i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        if (cydui_popup_begin(&rig.ui, "Name", &o, 220, 100, CYDUI_POPUP_MODAL)) {
            cydui_text_input(&rig.ui, "name", text, sizeof(text), 0, 0);
            cydui_popup_end(&rig.ui);
        }
        cydui_ui_end(&rig.ui);
    }
    rig_down(&rig, 80, 90);
    rig_up(&rig, 80, 90);
    for (i = 0; i < 3; i++) {
        cydui_ui_begin(&rig.ui, 0);
        if (cydui_popup_begin(&rig.ui, "Name", &o, 220, 100, CYDUI_POPUP_MODAL)) {
            cydui_text_input(&rig.ui, "name", text, sizeof(text), 0, 0);
            cydui_popup_end(&rig.ui);
        }
        cydui_ui_end(&rig.ui);
    }
    TEST_ASSERT_TRUE(cydui_has_focus(&rig.ui));
    rig_char(&rig, 'h');
    rig_char(&rig, 'i');
    cydui_ui_begin(&rig.ui, 0);
    if (cydui_popup_begin(&rig.ui, "Name", &o, 220, 100, CYDUI_POPUP_MODAL)) {
        cydui_text_input(&rig.ui, "name", text, sizeof(text), 0, 0);
        cydui_popup_end(&rig.ui);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_STRING("hi", text);
}

static void test_escape_first_drops_the_focus_of_a_field_in_the_popup_and_then_closes_it(void)
{
    static char text[16];
    bool o = true;
    int i;

    start();
    memset(text, 0, sizeof(text));
    for (i = 0; i < 8; i++) {
        cydui_ui_begin(&rig.ui, 0);
        if (cydui_popup_begin(&rig.ui, "Name", &o, 220, 100, 0)) {
            cydui_text_input(&rig.ui, "name", text, sizeof(text), 0, 0);
            cydui_popup_end(&rig.ui);
        }
        cydui_ui_end(&rig.ui);
        if (i == 2) {
            rig_down(&rig, 80, 90);
            rig_up(&rig, 80, 90);
        }
        if (i == 5) {
            rig_key(&rig, CYDUI_KEY_ESCAPE); /* the field has the focus: it is for the field */
        }
    }
    TEST_ASSERT_FALSE(cydui_has_focus(&rig.ui));
    TEST_ASSERT_TRUE(o);
    rig_key(&rig, CYDUI_KEY_ESCAPE);
    cydui_ui_begin(&rig.ui, 0);
    if (cydui_popup_begin(&rig.ui, "Name", &o, 220, 100, 0)) {
        cydui_popup_end(&rig.ui);
    }
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_FALSE(o);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_closed_popup_builds_nothing_and_an_open_one_does);
    RUN_TEST(test_the_content_reacts_to_the_touch);
    RUN_TEST(test_what_is_outside_does_not_react_while_it_is_open);
    RUN_TEST(test_a_press_outside_closes_a_popup_that_is_not_modal_and_not_a_modal_one);
    RUN_TEST(test_a_press_inside_the_panel_does_not_close_it);
    RUN_TEST(test_escape_closes_a_popup_that_is_not_modal);
    RUN_TEST(test_it_is_above_everything_wherever_it_is_called);
    RUN_TEST(test_what_is_behind_is_dimmed);
    RUN_TEST(test_it_takes_no_room_in_its_container_and_is_not_clipped_by_it);
    RUN_TEST(test_only_one_popup_a_second_in_the_frame_is_refused);
    RUN_TEST(test_a_nested_popup_is_refused_and_a_missing_end_is_reported);
    RUN_TEST(test_the_size_is_clamped_and_nothing_breaks_with_odd_arguments);
    RUN_TEST(test_the_container_depth_limit_leaves_everything_balanced);
    RUN_TEST(test_a_text_field_in_a_popup_works_and_keeps_the_focus);
    RUN_TEST(test_escape_first_drops_the_focus_of_a_field_in_the_popup_and_then_closes_it);
    return UNITY_END();
}
