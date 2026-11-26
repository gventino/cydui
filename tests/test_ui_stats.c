/* cydui_ui_stats: what a UI has used, and the flag for a state table that ran out. */

#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig R;

void test_a_fresh_ui_has_used_nothing_and_reports_what_it_was_given(void)
{
    cydui_ui_stats s;
    memset(&s, 0xAA, sizeof(s));
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&R));
    cydui_ui_stats_get(&R.ui, &s);
    TEST_ASSERT_EQUAL_UINT32(0, s.cmds_peak);
    TEST_ASSERT_EQUAL_UINT32(0, s.text_peak);
    TEST_ASSERT_EQUAL_UINT32(0, s.state_used);
    TEST_ASSERT_EQUAL_UINT32(0, s.tweens_peak);
    TEST_ASSERT_EQUAL_UINT32(0, s.widgets_peak);
    TEST_ASSERT_EQUAL_UINT32(512, s.cmds_cap);
    TEST_ASSERT_EQUAL_UINT32(4096, s.text_cap);
    TEST_ASSERT_EQUAL_UINT32(64, s.state_cap);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_MAX_TWEENS, s.tweens_cap);
}

void test_the_peaks_follow_the_busiest_frame_and_do_not_fall_when_it_is_over(void)
{
    cydui_ui_stats s, after;
    uint32_t cmds;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&R));
    cydui_ui_begin(&R.ui, 0);
    cydui_label(&R.ui, "one");
    cydui_label(&R.ui, "two");
    cydui_button(&R.ui, "Go");
    cmds = cydui_cmd_count(&R.ui.ctx);
    cydui_ui_end(&R.ui);
    cydui_ui_stats_get(&R.ui, &s);
    TEST_ASSERT_EQUAL_UINT32(cmds, s.cmds_peak);
    TEST_ASSERT_TRUE(cmds > 3);
    TEST_ASSERT_EQUAL_UINT32(3, s.widgets_peak);
    TEST_ASSERT_TRUE_MESSAGE(s.text_peak >= strlen("one") + strlen("two") + strlen("Go"), "the strings of the frame are in the text memory");
    TEST_ASSERT_TRUE(s.text_peak < s.text_cap);
    rig_empty_frame(&R);
    rig_empty_frame(&R);
    cydui_ui_stats_get(&R.ui, &after);
    TEST_ASSERT_EQUAL_UINT32(s.cmds_peak, after.cmds_peak);
    TEST_ASSERT_EQUAL_UINT32(s.text_peak, after.text_peak);
    TEST_ASSERT_EQUAL_UINT32(s.widgets_peak, after.widgets_peak);
}

void test_a_bigger_frame_raises_the_peak_and_reset_starts_the_peaks_over_but_not_the_caps(void)
{
    cydui_ui_stats s;
    int i;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&R));
    cydui_ui_begin(&R.ui, 0);
    cydui_label(&R.ui, "a");
    cydui_ui_end(&R.ui);
    cydui_ui_stats_get(&R.ui, &s);
    {
        uint32_t small_cmds = s.cmds_peak;
        cydui_ui_begin(&R.ui, 0);
        for (i = 0; i < 6; i++) {
            cydui_label(&R.ui, "a longer line of text");
        }
        cydui_ui_end(&R.ui);
        cydui_ui_stats_get(&R.ui, &s);
        TEST_ASSERT_TRUE(s.cmds_peak > small_cmds);
        TEST_ASSERT_EQUAL_UINT32(6, s.widgets_peak);
    }
    cydui_ui_stats_reset(&R.ui);
    cydui_ui_stats_get(&R.ui, &s);
    TEST_ASSERT_EQUAL_UINT32(0, s.cmds_peak);
    TEST_ASSERT_EQUAL_UINT32(0, s.text_peak);
    TEST_ASSERT_EQUAL_UINT32(0, s.widgets_peak);
    TEST_ASSERT_EQUAL_UINT32(512, s.cmds_cap);
    TEST_ASSERT_EQUAL_UINT32(64, s.state_cap);
}

void test_tweens_moving_at_once_are_counted(void)
{
    cydui_ui_stats s;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&R));
    cydui_ui_begin(&R.ui, 0);
    cydui_animate(&R.ui, cydui_id_str(&R.ui, "a"), 0, 0, 200, CYDUI_EASE_LINEAR);
    cydui_animate(&R.ui, cydui_id_str(&R.ui, "b"), 0, 0, 200, CYDUI_EASE_LINEAR);
    cydui_ui_end(&R.ui);
    rig_advance(&R, 20);
    cydui_ui_begin(&R.ui, 0);
    cydui_animate(&R.ui, cydui_id_str(&R.ui, "a"), 0, 100, 200, CYDUI_EASE_LINEAR);
    cydui_animate(&R.ui, cydui_id_str(&R.ui, "b"), 0, 100, 200, CYDUI_EASE_LINEAR);
    cydui_ui_end(&R.ui);
    cydui_ui_stats_get(&R.ui, &s);
    TEST_ASSERT_EQUAL_UINT32(2, s.tweens_peak);
}

void test_state_used_counts_the_entries_in_use(void)
{
    cydui_ui_stats s;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&R));
    cydui_ui_begin(&R.ui, 0);
    TEST_ASSERT_NOT_NULL(cydui_state_get(&R.ui, cydui_id_str(&R.ui, "x"), true));
    TEST_ASSERT_NOT_NULL(cydui_state_get(&R.ui, cydui_id_str(&R.ui, "y"), true));
    cydui_ui_end(&R.ui);
    cydui_ui_stats_get(&R.ui, &s);
    TEST_ASSERT_EQUAL_UINT32(2, s.state_used);
}

void test_a_full_state_table_says_so_exactly_when_an_entry_cannot_be_made(void)
{
    static const char *const names[] = {"a", "b", "c", "d", "e"};
    int i;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&R, 4, 0));
    cydui_ui_begin(&R.ui, 0);
    for (i = 0; i < 4; i++) {
        TEST_ASSERT_NOT_NULL(cydui_state_get(&R.ui, cydui_id_str(&R.ui, names[i]), true));
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(0, cydui_ui_errors(&R.ui) & CYDUI_ERR_STATE_FULL, "four entries fit in four");
    }
    TEST_ASSERT_NOT_NULL(cydui_state_get(&R.ui, cydui_id_str(&R.ui, "a"), true)); /* finding one that exists is no problem */
    TEST_ASSERT_EQUAL_HEX32(0, cydui_ui_errors(&R.ui) & CYDUI_ERR_STATE_FULL);
    TEST_ASSERT_NULL_MESSAGE(cydui_state_get(&R.ui, cydui_id_str(&R.ui, names[4]), false), "not asking to create asks for nothing");
    TEST_ASSERT_EQUAL_HEX32(0, cydui_ui_errors(&R.ui) & CYDUI_ERR_STATE_FULL);
    cydui_state_get(&R.ui, cydui_id_str(&R.ui, names[4]), true); /* the fifth: something is replaced */
    TEST_ASSERT_EQUAL_HEX32(CYDUI_ERR_STATE_FULL, cydui_ui_errors(&R.ui) & CYDUI_ERR_STATE_FULL);
    cydui_ui_end(&R.ui);
}

void test_a_state_table_of_no_entries_is_a_choice_and_raises_nothing(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&R, 0, 0));
    cydui_ui_begin(&R.ui, 0);
    TEST_ASSERT_NULL(cydui_state_get(&R.ui, cydui_id_str(&R.ui, "a"), true));
    TEST_ASSERT_EQUAL_HEX32(0, cydui_ui_errors(&R.ui) & CYDUI_ERR_STATE_FULL);
    cydui_ui_end(&R.ui);
}

void test_an_entry_used_in_the_previous_frame_is_still_in_use_when_it_is_replaced(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&R, 2, 0));
    cydui_ui_begin(&R.ui, 0);
    cydui_state_get(&R.ui, cydui_id_str(&R.ui, "a"), true);
    cydui_state_get(&R.ui, cydui_id_str(&R.ui, "b"), true);
    cydui_ui_end(&R.ui);
    cydui_ui_begin(&R.ui, 0); /* the next frame asks for a third */
    cydui_state_get(&R.ui, cydui_id_str(&R.ui, "c"), true);
    TEST_ASSERT_EQUAL_HEX32(CYDUI_ERR_STATE_FULL, cydui_ui_errors(&R.ui) & CYDUI_ERR_STATE_FULL);
    cydui_ui_end(&R.ui);
}

void test_replacing_an_entry_that_nobody_has_used_for_a_while_is_not_an_error(void)
{
    int i;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&R, 2, 0));
    cydui_ui_begin(&R.ui, 0);
    cydui_state_get(&R.ui, cydui_id_str(&R.ui, "a"), true);
    cydui_state_get(&R.ui, cydui_id_str(&R.ui, "b"), true);
    cydui_ui_end(&R.ui);
    for (i = 0; i < 5; i++) {
        rig_empty_frame(&R); /* both go unused */
    }
    cydui_ui_begin(&R.ui, 0);
    cydui_state_get(&R.ui, cydui_id_str(&R.ui, "c"), true);
    cydui_state_get(&R.ui, cydui_id_str(&R.ui, "d"), true);
    TEST_ASSERT_EQUAL_HEX32(0, cydui_ui_errors(&R.ui) & CYDUI_ERR_STATE_FULL);
    cydui_ui_end(&R.ui);
}

void test_stats_calls_with_a_null_pointer_do_nothing(void)
{
    cydui_ui_stats s;
    memset(&s, 7, sizeof(s));
    cydui_ui_stats_get(NULL, &s);
    TEST_ASSERT_EQUAL_UINT8(7, ((uint8_t *)&s)[0]);
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&R));
    cydui_ui_stats_get(&R.ui, NULL);
    cydui_ui_stats_reset(NULL);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_fresh_ui_has_used_nothing_and_reports_what_it_was_given);
    RUN_TEST(test_the_peaks_follow_the_busiest_frame_and_do_not_fall_when_it_is_over);
    RUN_TEST(test_a_bigger_frame_raises_the_peak_and_reset_starts_the_peaks_over_but_not_the_caps);
    RUN_TEST(test_tweens_moving_at_once_are_counted);
    RUN_TEST(test_state_used_counts_the_entries_in_use);
    RUN_TEST(test_a_full_state_table_says_so_exactly_when_an_entry_cannot_be_made);
    RUN_TEST(test_a_state_table_of_no_entries_is_a_choice_and_raises_nothing);
    RUN_TEST(test_an_entry_used_in_the_previous_frame_is_still_in_use_when_it_is_replaced);
    RUN_TEST(test_replacing_an_entry_that_nobody_has_used_for_a_while_is_not_an_error);
    RUN_TEST(test_stats_calls_with_a_null_pointer_do_nothing);
    return UNITY_END();
}
