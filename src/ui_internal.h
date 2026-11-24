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

#endif /* CYDUI_UI_INTERNAL_H */
