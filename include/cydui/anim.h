#ifndef CYDUI_ANIM_H
#define CYDUI_ANIM_H

/*
 * Easing curves, in integers only (R-08): a curve maps a position in time, 0..CYDUI_ANIM_ONE, to a
 * position in value, 0..CYDUI_ANIM_ONE (a curve with a spring in it goes beyond that on the way).
 * Binding a curve to a widget over time is cydui_animate() in ui.h.
 */

#include "cydui/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** One, in the fixed point of the curves (12 bits). */
#define CYDUI_ANIM_ONE 4096

typedef enum {
    CYDUI_EASE_LINEAR,
    CYDUI_EASE_IN_QUAD,
    CYDUI_EASE_OUT_QUAD,
    CYDUI_EASE_IN_OUT_QUAD,
    CYDUI_EASE_IN_CUBIC,
    CYDUI_EASE_OUT_CUBIC,
    CYDUI_EASE_IN_OUT_CUBIC,
    CYDUI_EASE_OUT_BACK,    /**< overshoots the end a little and comes back */
    CYDUI_EASE_OUT_ELASTIC, /**< swings around the end and settles */
    CYDUI_EASE_COUNT
} cydui_ease;

/**
 * The curve `e` at time `t` (clamped to 0..CYDUI_ANIM_ONE). 0 gives 0 and CYDUI_ANIM_ONE gives
 * CYDUI_ANIM_ONE exactly, for every curve. An unknown `e` is linear.
 */
int32_t cydui_ease_apply(cydui_ease e, int32_t t);

/** `a` towards `b` by `k` (CYDUI_ANIM_ONE is all the way; more or less than that extrapolates). Safe for the whole int32 range. */
int32_t cydui_lerp(int32_t a, int32_t b, int32_t k);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_ANIM_H */
