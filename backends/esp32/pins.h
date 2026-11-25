#ifndef CYDUI_PINS_H
#define CYDUI_PINS_H

/*
 * Checks on the GPIO numbers of the ESP32 backend, at compile time (CT-14). Every macro is a constant
 * expression of the pin number, so the same ones serve the tests on the PC and the Kconfig values in the
 * firmware. -1 means "not connected".
 *
 *   flash       6..11   wired to the flash chip: an output there crashes the module
 *   input only  34..39  have no output driver
 *   missing     20, 24, 28..31 and anything above 39 are not on the chip
 *   strapping   0, 2, 5, 12, 15 are read at boot: fine as outputs afterwards (the CYD wires its display to
 *               2 and 15) but suspicious on another board, so they only warn when the profile is not the CYD
 */

#define CYDUI_PIN_IS_FLASH(p) ((p) >= 6 && (p) <= 11)
#define CYDUI_PIN_IS_INPUT_ONLY(p) ((p) >= 34 && (p) <= 39)
#define CYDUI_PIN_IS_MISSING(p) ((p) == 20 || (p) == 24 || ((p) >= 28 && (p) <= 31) || (p) > 39 || (p) < -1)
#define CYDUI_PIN_IS_STRAPPING(p) ((p) == 0 || (p) == 2 || (p) == 5 || (p) == 12 || (p) == 15)

/** A pin that cannot drive a signal. */
#define CYDUI_PIN_BAD_OUTPUT(p) (CYDUI_PIN_IS_FLASH(p) || CYDUI_PIN_IS_INPUT_ONLY(p) || CYDUI_PIN_IS_MISSING(p))
/** A pin that cannot read a signal. */
#define CYDUI_PIN_BAD_INPUT(p) (CYDUI_PIN_IS_FLASH(p) || CYDUI_PIN_IS_MISSING(p))

#define CYDUI_PINS_CAT2(a, b) a##b
#define CYDUI_PINS_CAT(a, b) CYDUI_PINS_CAT2(a, b)

/** Stops the build, with `msg` in the error, when the condition is false (C99, no static_assert). */
#define CYDUI_PINS_ASSERT(cond, msg) typedef char CYDUI_PINS_CAT(cydui_pin_check_, msg)[(cond) ? 1 : -1]

/** `name` must be a valid output (use a CONFIG_... value for `pin`; -1 passes for pins that are optional). */
#define CYDUI_PIN_REQUIRE_OUTPUT(name, pin) CYDUI_PINS_ASSERT(!CYDUI_PIN_BAD_OUTPUT(pin) && (pin) >= 0, name##_is_not_an_output_pin)
#define CYDUI_PIN_REQUIRE_INPUT(name, pin) CYDUI_PINS_ASSERT(!CYDUI_PIN_BAD_INPUT(pin) && (pin) >= 0, name##_is_not_an_input_pin)
#define CYDUI_PIN_OPTIONAL_OUTPUT(name, pin) CYDUI_PINS_ASSERT((pin) == -1 || !CYDUI_PIN_BAD_OUTPUT(pin), name##_is_not_an_output_pin)
#define CYDUI_PIN_OPTIONAL_INPUT(name, pin) CYDUI_PINS_ASSERT((pin) == -1 || !CYDUI_PIN_BAD_INPUT(pin), name##_is_not_an_input_pin)

#endif /* CYDUI_PINS_H */
