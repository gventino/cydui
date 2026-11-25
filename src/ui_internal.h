#ifndef CYDUI_UI_INTERNAL_H
#define CYDUI_UI_INTERNAL_H

/* Private to the library: shared by ui.c, layout.c and widgets.c. */

#include "cydui/ui.h"

/** Mixes `n` bytes into `parent`: the hash behind every widget and container id. */
cydui_id cydui__id_mix(cydui_id parent, const void *bytes, size_t n);

/** Debug check that the UI is used from the task that created it (CT-11). */
void cydui__check_thread(cydui_ui *ui);

/** Start and end of a frame, for the layout part. */
void cydui__layout_begin_frame(cydui_ui *ui);
void cydui__layout_end_frame(cydui_ui *ui);

/** Containers, for the scroll areas: open one (false if too deep, the end is then skipped), open one
 * at a given place with a given size, end one of `kind`. */
bool cydui__open_region(cydui_ui *ui, cydui_layout_kind kind, int cols, int pad);
bool cydui__open_region_at(cydui_ui *ui, cydui_layout_kind kind, cydui_rect rect);
void cydui__end_kind(cydui_ui *ui, cydui_layout_kind kind);

/** The keyboard on the screen: true if a press at (x, y) is on the keyboard shown in the previous frame; and the step
 * that handles and draws it at the end of a frame, returning how many events its keys produced (at most 8). */
bool cydui__keyboard_hit(const cydui_ui *ui, int x, int y);
int cydui__keyboard_top(const cydui_ui *ui); /* where it begins when it is shown */
unsigned cydui__keyboard_run(cydui_ui *ui, cydui_event *out);

/** Scrolls the innermost scroll area that holds the widget at `r` (screen coordinates, as drawn in this frame) so that its
 * bottom is not below `limit` and its top is not above the area's window. Does nothing outside a scroll area. */
void cydui__scroll_reveal(cydui_ui *ui, cydui_rect r, int limit);

/** A popup's container: at the rectangle, and taking no room in its parent. */
bool cydui__open_overlay(cydui_ui *ui, cydui_rect rect);
bool cydui__close_overlay(cydui_ui *ui);

/** Moves the commands [mid, end) of the list in front of [start, mid), keeping the order within each. */
void cydui__cmd_move_tail(cydui_ctx *ctx, uint32_t start, uint32_t mid);

/** Draws the bar and releases the clip of the scroll area that is the innermost open container. */
void cydui__scroll_finish(cydui_ui *ui);

#endif /* CYDUI_UI_INTERNAL_H */
