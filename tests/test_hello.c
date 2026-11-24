/* NFR-40: a "hello" application in at most 25 lines of C (blank lines are not counted). */

#include <stdio.h>
#include <string.h>

#include "unity.h"

#ifndef HELLO_SRC
#error "HELLO_SRC must be the path of examples/hello/main.c"
#endif

void setUp(void) {}
void tearDown(void) {}

static void test_hello_fits_in_25_lines(void)
{
    FILE *f = fopen(HELLO_SRC, "r");
    char line[256];
    int lines = 0;

    TEST_ASSERT_NOT_NULL_MESSAGE(f, "cannot open the hello example");
    while (fgets(line, sizeof(line), f)) {
        if (line[strspn(line, " \t\r\n")] != '\0') {
            lines++;
        }
    }
    fclose(f);
    printf("hello is %d lines\n", lines);
    TEST_ASSERT_TRUE_MESSAGE(lines >= 10, "suspiciously short: is this the right file?");
    TEST_ASSERT_TRUE_MESSAGE(lines <= 25, "the hello example grew past 25 lines (NFR-40)");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_hello_fits_in_25_lines);
    return UNITY_END();
}
