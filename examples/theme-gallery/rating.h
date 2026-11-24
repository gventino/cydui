#ifndef RATING_H
#define RATING_H

/*
 * A star rating, written as an application would write a widget of its own: with nothing but the
 * public headers of the library (see "writing a widget" in cydui/ui.h).
 */

#include "cydui/ui.h"

#define RATING_STARS 5

/**
 * Five stars with `label` (if it shows anything) to their left. A tap on star i sets *value to
 * i + 1; a tap on the star that is already the last lit one clears it. A value outside 0..5 is
 * clamped quietly. Returns true in a frame in which the user changed the value.
 */
bool rating(cydui_ui *ui, const char *label, int *value);

#endif /* RATING_H */
