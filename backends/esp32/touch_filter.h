#ifndef CYDUI_TOUCH_FILTER_H
#define CYDUI_TOUCH_FILTER_H

/*
 * The filter between a resistive touch controller and the UI (FR-43, R-04). Once per tick the driver
 * hands over a few conversions (x, y and pressure) taken while the screen is touched; the filter turns
 * them into one steady position and into DOWN / MOVE / UP events:
 *   - the position is the mean of the burst without its smallest and largest value;
 *   - a burst whose values spread too far (a contact that is bouncing) is thrown away;
 *   - a press needs the pressure to be over a threshold for a few ticks in a row, and a release needs it
 *     to be under a lower one for a few ticks in a row (hysteresis and debounce), so a dropout of one
 *     tick neither ends the touch nor starts a new one;
 *   - a single tick that jumps far from the last position is believed only if the next tick agrees;
 *   - MOVE is only sent for a change of at least `move_min`, so a held finger does not tremble.
 * Pure C: positions are raw controller units, mapped to pixels by the calibration afterwards.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cydui/backend.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CYDUI_TOUCH_MAX_READS 9 /* conversions per tick the filter looks at; more are ignored */

typedef struct {
    uint16_t x, y, z; /* one conversion: the two positions and the pressure (larger = harder) */
} cydui_touch_read;

typedef struct {
    uint16_t z_on, z_off;  /* pressure to start a press / to keep it (z_off <= z_on) */
    uint8_t down_ticks;    /* consecutive ticks over z_on before DOWN (at least 1) */
    uint8_t up_ticks;      /* consecutive ticks under z_off before UP (at least 1) */
    uint16_t move_min;     /* raw units of change for a MOVE */
    uint16_t max_spread;   /* a burst whose x or y span is wider than this is thrown away */
    uint16_t max_jump;     /* a tick this far from the last position needs a second one to agree */
} cydui_touch_cfg;

typedef struct {
    cydui_touch_cfg cfg;
    bool down;
    uint8_t press_run, release_run;
    bool jump_pending;
    uint16_t x, y;   /* the last reported position */
    uint16_t jx, jy; /* a jump waiting for its second tick */
} cydui_touch_filter;

/** Sensible numbers for an XPT2046 on a CYD (raw range about 400..3600, pressure 1100..2800 while touched). */
cydui_touch_cfg cydui_touch_cfg_default(void);

void cydui_touch_filter_init(cydui_touch_filter *f, const cydui_touch_cfg *cfg);

/**
 * One tick. `irq_low` is the state of the controller's pen interrupt (touched = low); with it high
 * the tick is a release. `reads` may be NULL when n is 0. Returns true and fills `ev` (x, y in raw
 * units) when there is an event, at most one per tick.
 */
bool cydui_touch_tick(cydui_touch_filter *f, const cydui_touch_read *reads, int n, bool irq_low, cydui_event *ev);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_TOUCH_FILTER_H */
