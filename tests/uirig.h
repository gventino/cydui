#ifndef UIRIG_H
#define UIRIG_H

/*
 * A complete UI on a 320x240 in-memory screen, with every byte of memory inside the struct, and
 * shortcuts to feed it synthetic touches and a controlled clock.
 */

#include "cydui/ui.h"
#include "cydui_mem.h"

#define RIG_W 320
#define RIG_H 240

typedef struct {
    cydui_mem mem;
    cydui_ui ui;
    cydui_theme theme;
    cydui_color fb[RIG_W * RIG_H];
    cydui_cmd cmds[512];
    cydui_color strip[RIG_W * 8];
    char text[4096];
    cydui_state state[64];
    uint32_t idset[1024];
} uirig;

/** Starts a rig with the 12 px bundled font. Returns 0 or a CYDUI_ERR_* flag. */
uint32_t rig_init(uirig *r);

/** Same, with a state table of `state_entries` entries (to test a full table). */
uint32_t rig_init_with(uirig *r, size_t state_entries, uint16_t long_press_ms);

/* Queue touches (they are read by the next cydui_ui_begin). */
void rig_down(uirig *r, int x, int y);
void rig_move(uirig *r, int x, int y);
void rig_up(uirig *r, int x, int y);
void rig_advance(uirig *r, uint32_t ms);
/** Queue a typed character (a code point) or an editing key (a cydui_key). */
void rig_char(uirig *r, uint32_t cp);
void rig_key(uirig *r, cydui_key key);

/** begin + end with nothing in between; returns what cydui_ui_end returned. */
bool rig_empty_frame(uirig *r);

#endif /* UIRIG_H */
