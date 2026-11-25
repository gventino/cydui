/*
 * The pin checks (CT-14): the macros for every number from -5 to 45, compared with lists written out here
 * independently of the macros. The build-time errors (a flash pin as an output and so on) are checked by
 * ctest compiling tests/pins_check.c with good and bad pins (the pins_* tests).
 */

#include "pins.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static int in_list(int p, const int *list, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        if (list[i] == p) return 1;
    }
    return 0;
}

static void test_every_pin_number_is_classified_as_the_datasheet_says(void)
{
    static const int flash[] = {6, 7, 8, 9, 10, 11};
    static const int input_only[] = {34, 35, 36, 37, 38, 39};
    static const int missing[] = {20, 24, 28, 29, 30, 31, 40, 41, 42, 43, 44, 45, -5, -4, -3, -2};
    static const int strapping[] = {0, 2, 5, 12, 15};
    int p;

    for (p = -5; p <= 45; p++) {
        int is_flash = in_list(p, flash, 6), is_in = in_list(p, input_only, 6), is_missing = in_list(p, missing, 16);
        TEST_ASSERT_EQUAL_INT_MESSAGE(is_flash, CYDUI_PIN_IS_FLASH(p) ? 1 : 0, "flash");
        TEST_ASSERT_EQUAL_INT_MESSAGE(is_in, CYDUI_PIN_IS_INPUT_ONLY(p) ? 1 : 0, "input only");
        TEST_ASSERT_EQUAL_INT_MESSAGE(is_missing, CYDUI_PIN_IS_MISSING(p) ? 1 : 0, "missing");
        TEST_ASSERT_EQUAL_INT_MESSAGE(in_list(p, strapping, 5), CYDUI_PIN_IS_STRAPPING(p) ? 1 : 0, "strapping");
        TEST_ASSERT_EQUAL_INT_MESSAGE(is_flash || is_in || is_missing, CYDUI_PIN_BAD_OUTPUT(p) ? 1 : 0, "bad output");
        TEST_ASSERT_EQUAL_INT_MESSAGE(is_flash || is_missing, CYDUI_PIN_BAD_INPUT(p) ? 1 : 0, "bad input");
    }
}

static void test_the_pins_of_the_cyd_are_all_acceptable(void)
{
    /* LCD MOSI 13, SCLK 14, CS 15, DC 2, BL 21, MISO 12; touch CLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 */
    static const int outputs[] = {13, 14, 15, 2, 21, 25, 32, 33};
    static const int inputs[] = {12, 39, 36};
    int i;

    for (i = 0; i < 8; i++) TEST_ASSERT_FALSE(CYDUI_PIN_BAD_OUTPUT(outputs[i]));
    for (i = 0; i < 3; i++) TEST_ASSERT_FALSE(CYDUI_PIN_BAD_INPUT(inputs[i]));
    TEST_ASSERT_TRUE(CYDUI_PIN_IS_STRAPPING(2) && CYDUI_PIN_IS_STRAPPING(15) && CYDUI_PIN_IS_STRAPPING(12));
    TEST_ASSERT_FALSE(CYDUI_PIN_BAD_OUTPUT(-1)); /* -1 is "not connected", which the REQUIRE macros refuse and the OPTIONAL ones accept */
    TEST_ASSERT_FALSE(CYDUI_PIN_IS_MISSING(-1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_every_pin_number_is_classified_as_the_datasheet_says);
    RUN_TEST(test_the_pins_of_the_cyd_are_all_acceptable);
    return UNITY_END();
}
