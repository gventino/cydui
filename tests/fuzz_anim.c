/*
 * libFuzzer harness for the easing curves and the interpolation (FR-53, R-08): any curve number, any time, any ends.
 * Oracles: a curve is 0 at 0 and exactly one at one whatever it overshoots in between, an unknown curve is the straight
 * line, and the interpolation of two int32 values stays in the int32 range and keeps its ends.
 *
 * Input: curve (1 byte), time (4 bytes), a (4), b (4), k (4).
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/anim.h"

static int32_t i32(const uint8_t *d, size_t size, size_t at)
{
    uint32_t v = 0;
    size_t i;

    for (i = 0; i < 4; i++) {
        v |= (uint32_t)(at + i < size ? d[at + i] : 0) << (8 * i);
    }
    return (int32_t)v;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    cydui_ease e = (cydui_ease)(size ? data[0] : 0);
    int32_t t = i32(data, size, 1), a = i32(data, size, 5), b = i32(data, size, 9), k = i32(data, size, 13), v, lerped;

    v = cydui_ease_apply(e, t);
    if (cydui_ease_apply(e, 0) != 0 || cydui_ease_apply(e, CYDUI_ANIM_ONE) != CYDUI_ANIM_ONE) abort();
    if ((unsigned)e >= CYDUI_EASE_COUNT && v != (t < 0 ? 0 : (t > CYDUI_ANIM_ONE ? CYDUI_ANIM_ONE : t))) abort();
    if (e == CYDUI_EASE_OUT_ELASTIC || e == CYDUI_EASE_OUT_BACK) { /* a spring may leave the range, but not by much */
        if (v < -2 * CYDUI_ANIM_ONE || v > 3 * CYDUI_ANIM_ONE) abort();
    } else if ((unsigned)e < CYDUI_EASE_COUNT && (v < 0 || v > CYDUI_ANIM_ONE)) {
        abort();
    }
    lerped = cydui_lerp(a, b, 0);
    if (lerped != a) abort();
    if (cydui_lerp(a, b, CYDUI_ANIM_ONE) != b) abort();
    lerped = cydui_lerp(a, b, k);
    if (a == b && lerped != a) abort();
    if (k >= 0 && k <= CYDUI_ANIM_ONE) { /* between the ends */
        int32_t lo = a < b ? a : b, hi = a < b ? b : a;
        if (lerped < lo || lerped > hi) abort();
    }
    return 0;
}
