#ifndef CYDUI_UI_INTERNAL_H
#define CYDUI_UI_INTERNAL_H

/* Private to the library: shared by ui.c, layout.c and widgets.c. */

#include "cydui/ui.h"

/** Mixes `n` bytes into `parent`: the hash behind every widget and container id. */
cydui_id cydui__id_mix(cydui_id parent, const void *bytes, size_t n);

/** Debug check that the UI is used from the task that created it (CT-11). */
void cydui__check_thread(cydui_ui *ui);

/** Draws text that never leaves `box` (the clip is narrowed for the call). */
void cydui__draw_text_in(cydui_ui *ui, cydui_rect box, int x, int y, const char *s, size_t n,
                         cydui_color color);

/** Start and end of a frame, for the layout part. */
void cydui__layout_begin_frame(cydui_ui *ui);
void cydui__layout_end_frame(cydui_ui *ui);

/** Containers, for the scroll areas: open one (false if too deep, the end is then skipped), open one
 * at a given place with a given size, end one of `kind`. */
bool cydui__open_region(cydui_ui *ui, cydui_layout_kind kind, int cols, int pad);
bool cydui__open_region_at(cydui_ui *ui, cydui_layout_kind kind, cydui_rect rect);
void cydui__end_kind(cydui_ui *ui, cydui_layout_kind kind);

/** Draws the bar and releases the clip of the scroll area that is the innermost open container. */
void cydui__scroll_finish(cydui_ui *ui);

#endif /* CYDUI_UI_INTERNAL_H */
