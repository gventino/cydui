/*
 * Popups (FR-35). The content is built where the application calls it, but its commands are moved behind the last one of
 * the frame, so they are drawn above everything; the touch is taken from what is outside by the state of the previous
 * frame (cydui_interact), because the widgets that come before the popup in the frame have already been called when it
 * is begun.
 */

#include <string.h>

#include "cydui/ui.h"
#include "ui_internal.h"

#define POPUP_MARGIN 8   /* the panel never gets closer than this to the edge of the screen */
#define POPUP_MIN 40
#define DIM_ALPHA 140    /* black over what is behind, out of 255 */

static bool inside(cydui_rect r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

bool cydui_popup_begin(cydui_ui *ui, const char *label, bool *open, int w, int h, unsigned flags)
{
    const cydui_backend *be = ui->ctx.backend;
    const cydui_style *st = &ui->theme->style;
    cydui_input *in = &ui->in;
    cydui_paint_info pi;
    cydui_rect panel, inner;
    unsigned i;

    cydui__check_thread(ui);
    if (!open || !*open) {
        return false;
    }
    if (ui->popup_now || ui->in_popup) { /* one at a time */
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        return false;
    }
    w = w < POPUP_MIN ? POPUP_MIN : (w > be->width - 2 * POPUP_MARGIN ? be->width - 2 * POPUP_MARGIN : w);
    h = h < POPUP_MIN ? POPUP_MIN : (h > be->height - 2 * POPUP_MARGIN ? be->height - 2 * POPUP_MARGIN : h);
    panel.w = (int16_t)w;
    panel.h = (int16_t)h;
    panel.x = (int16_t)((be->width - w) / 2);
    panel.y = (int16_t)((be->height - h) / 2);

    if (!(flags & CYDUI_POPUP_MODAL)) { /* a press outside the panel, or Escape, closes it */
        bool close = in->frame_down && !inside(panel, in->press_x, in->press_y);

        for (i = 0; i < in->k_len && !close; i++) {
            close = in->keys[i].type == CYDUI_EVENT_KEY && in->keys[i].key == CYDUI_KEY_ESCAPE && !cydui_has_focus(ui);
        }
        if (close) {
            *open = false;
            return false;
        }
    }

    inner.x = (int16_t)(panel.x + st->pad_x);
    inner.y = (int16_t)(panel.y + st->pad_y);
    inner.w = (int16_t)(panel.w > 2 * st->pad_x ? panel.w - 2 * st->pad_x : 0);
    inner.h = (int16_t)(panel.h > 2 * st->pad_y ? panel.h - 2 * st->pad_y : 0);
    ui->popup_saved_clip = ui->ctx.clip;
    cydui_reset_clip(&ui->ctx); /* not clipped by the container the application called it from */
    ui->popup_start = ui->ctx.cmd_len;
    {
        cydui_id fade = cydui_id_str(ui, label ? label : "");
        int32_t dim = DIM_ALPHA;

        if (st->anim_ms) { /* the dimming comes in over the first frames after the popup opens */
            if (!ui->popup_prev) {
                cydui__animate_reset(ui, fade);
            }
            dim = cydui_animate(ui, fade, 0, DIM_ALPHA, st->anim_ms, CYDUI_EASE_OUT_QUAD);
        }
        cydui_draw_rect(&ui->ctx, (cydui_rect){0, 0, be->width, be->height}, 0, (uint8_t)dim);
    }
    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_POPUP;
    pi.state = CYDUI_STATE_NORMAL;
    pi.rect = panel;
    cydui_paint(ui, &pi);
    cydui_push_id_str(ui, label ? label : "");
    if (!cydui__open_overlay(ui, inner)) {
        cydui_pop_id(ui);
        ui->ctx.clip = ui->popup_saved_clip;
        ui->popup_end = ui->popup_start; /* nothing to move: the commands drawn stay where they are */
        return false;
    }
    ui->popup_now = true;
    ui->in_popup = true;
    return true;
}

void cydui_popup_end(cydui_ui *ui)
{
    cydui__check_thread(ui);
    if (!ui->in_popup) {
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        return;
    }
    if (!cydui__close_overlay(ui)) {
        return;
    }
    cydui_pop_id(ui);
    ui->in_popup = false;
    ui->popup_end = ui->ctx.cmd_len;
    ui->ctx.clip = ui->popup_saved_clip;
}
