/* Compiled by ctest with -DMODE=... -DPIN=...: it must build for a good pin and fail for a bad one (CT-14). */
#include "pins.h"

#ifndef PIN
#define PIN 13
#endif

#if MODE == 1
CYDUI_PIN_REQUIRE_OUTPUT(lcd_mosi, PIN);
#elif MODE == 2
CYDUI_PIN_REQUIRE_INPUT(touch_miso, PIN);
#elif MODE == 3
CYDUI_PIN_OPTIONAL_OUTPUT(lcd_rst, PIN);
#elif MODE == 4
CYDUI_PIN_OPTIONAL_INPUT(lcd_miso, PIN);
#endif

int main(void)
{
    return 0;
}
