/*
 * The basic widgets. They measure, place and react; what they look like is up to the theme, which
 * they reach through cydui_paint().
 */

#include <string.h>

#include "ui_internal.h"

#define CHECK_BOX 20
#define TOGGLE_W 44
#define TOGGLE_H 24
#define KNOB 18
#define LABEL_GAP 8

/* ---- helpers ---- */

/* Counts a widget against the per-frame limit (AB-07). False means: ignore this call. */
static bool count_widget(cydui_ui *ui)
{
    cydui__check_thread(ui);
    if (ui->widgets >= CYDUI_MAX_WIDGETS) {
        ui->ctx.errors |= CYDUI_ERR_TOO_MANY_WIDGETS;
        return false;
    }
    ui->widgets++;
    return true;
}

/* How much of a label is shown: up to a "##", at most CYDUI_MAX_STR bytes. */
static size_t shown_len(const char *s)
{
    size_t n = 0;

    while (n < CYDUI_MAX_STR && s[n] != '\0') {
        /* the byte after a '#' is only looked at if it is still inside the limit */
        if (s[n] == '#' && n + 1 < CYDUI_MAX_STR && s[n + 1] == '#') {
            break;
        }
        n++;
    }
    return n;
}

/* Draws text inside `box` only, however long it is. */
void cydui__draw_text_in(cydui_ui *ui, cydui_rect box, int x, int y, const char *s, size_t n,
                         cydui_color color)
{
    cydui_rect saved = ui->ctx.clip;

    cydui_set_clip(&ui->ctx, cydui_rect_intersect(saved, box));
    cydui_draw_text(&ui->ctx, ui->theme->style.font, (int16_t)x, (int16_t)y, 0, CYDUI_ALIGN_LEFT, s, n,
                    color, 255);
    ui->ctx.clip = saved;
}

static cydui_widget_state widget_state(bool disabled, bool pressed)
{
    return disabled ? CYDUI_STATE_DISABLED : (pressed ? CYDUI_STATE_PRESSED : CYDUI_STATE_NORMAL);
}

/* ---- widgets ---- */

void cydui_label(cydui_ui *ui, const char *text)
{
    cydui_mods m = cydui_take_mods(ui);
    size_t n;
    cydui_text_extent e;
    cydui_rect r;

    if (!count_widget(ui)) {
        return;
    }
    text = text ? text : "";
    n = shown_len(text);
    e = cydui_text_size(ui->theme->style.font, text, n, 0);
    r = cydui_alloc(ui, &m, e.w, e.h);
    cydui__draw_text_in(ui, r, r.x, r.y, text, n, m.disabled ? ui->theme->style.text_muted : ui->theme->style.text);
}

void cydui_label_wrapped(cydui_ui *ui, const char *text)
{
    cydui_mods m = cydui_take_mods(ui);
    int wrap;
    size_t n;
    cydui_text_extent e;
    cydui_rect r;

    if (!count_widget(ui)) {
        return;
    }
    text = text ? text : "";
    n = shown_len(text);
    wrap = cydui_layout_avail(ui).w;
    if (wrap < 1) {
        wrap = 1;
    }
    e = cydui_text_size(ui->theme->style.font, text, n, (int16_t)wrap);
    r = cydui_alloc(ui, &m, e.w, e.h);
    {
        cydui_rect saved = ui->ctx.clip;
        cydui_set_clip(&ui->ctx, cydui_rect_intersect(saved, r));
        cydui_draw_text(&ui->ctx, ui->theme->style.font, r.x, r.y, (int16_t)wrap, CYDUI_ALIGN_LEFT, text, n,
                        m.disabled ? ui->theme->style.text_muted : ui->theme->style.text, 255);
        ui->ctx.clip = saved;
    }
}

bool cydui_button(cydui_ui *ui, const char *label)
{
    cydui_mods m = cydui_take_mods(ui);
    const cydui_style *st = &ui->theme->style;
    cydui_id id;
    cydui_text_extent e;
    cydui_response resp;
    cydui_rect r;
    size_t n;
    int h;

    if (!count_widget(ui)) {
        return false;
    }
    label = label ? label : "";
    id = cydui_id_str(ui, label);
    cydui_id_claim(ui, id);
    n = shown_len(label);
    e = cydui_text_size(st->font, label, n, 0);
    h = e.h + 2 * st->pad_y;
    r = cydui_alloc(ui, &m, e.w + 2 * st->pad_x, h < st->min_target ? st->min_target : h);
    resp = cydui_interact(ui, id, r, CYDUI_SENSE_CLICK | (m.disabled ? CYDUI_DISABLED : 0u));
    {
        cydui_paint_info pi;
        memset(&pi, 0, sizeof(pi));
        pi.kind = CYDUI_WIDGET_BUTTON;
        pi.state = widget_state(m.disabled, resp.pressed);
        pi.rect = r;
        pi.text = label;
        pi.text_len = n;
        pi.text_extent = e;
        cydui_paint(ui, &pi);
    }
    return resp.clicked;
}

/* Shared by the check box and the switch: a control on the left, the label on the right. */
static bool control_with_label(cydui_ui *ui, const char *label, bool *value, int control_w,
                               int control_h, bool is_toggle)
{
    cydui_mods m = cydui_take_mods(ui);
    const cydui_style *st = &ui->theme->style;
    cydui_id id;
    cydui_text_extent e;
    cydui_response resp;
    cydui_rect r, ctl;
    size_t n;
    int w, h;
    bool checked;

    if (!count_widget(ui)) {
        return false;
    }
    label = label ? label : "";
    id = cydui_id_str(ui, label);
    cydui_id_claim(ui, id);
    n = shown_len(label);
    e = cydui_text_size(st->font, label, n, 0);
    w = control_w + (n ? LABEL_GAP + e.w : 0);
    h = control_h > e.h ? control_h : e.h;
    if (h < st->min_target) {
        h = st->min_target;
    }
    r = cydui_alloc(ui, &m, w, h);

    resp = cydui_interact(ui, id, r, CYDUI_SENSE_CLICK | (m.disabled ? CYDUI_DISABLED : 0u));
    if (resp.clicked && value) {
        *value = !*value;
    }
    checked = value ? *value : false;

    ctl.x = r.x;
    ctl.y = (int16_t)(r.y + (r.h - control_h) / 2);
    ctl.w = (int16_t)control_w;
    ctl.h = (int16_t)control_h;
    {
        cydui_paint_info pi;
        memset(&pi, 0, sizeof(pi));
        pi.kind = is_toggle ? CYDUI_WIDGET_TOGGLE : CYDUI_WIDGET_CHECKBOX;
        pi.state = widget_state(m.disabled, resp.pressed);
        pi.selected = checked;
        pi.rect = ctl;
        cydui_paint(ui, &pi);
    }
    if (n) {
        cydui__draw_text_in(ui, r, r.x + control_w + LABEL_GAP, r.y + (r.h - e.h) / 2, label, n,
                     m.disabled ? st->text_muted : st->text);
    }
    return resp.clicked;
}

bool cydui_checkbox(cydui_ui *ui, const char *label, bool *value)
{
    return control_with_label(ui, label, value, CHECK_BOX, CHECK_BOX, false);
}

bool cydui_toggle(cydui_ui *ui, const char *label, bool *value)
{
    return control_with_label(ui, label, value, TOGGLE_W, TOGGLE_H, true);
}
