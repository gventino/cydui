/*
 * The basic widgets. Their painting is kept together in the paint_* functions below, so that
 * iteration 4 can move it behind the theme without touching how widgets measure, place and react.
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

static cydui_color dim(const cydui_ui *ui, cydui_color c)
{
    return cydui_blend(ui->style.bg, c, 110); /* about 43% of the colour over the background */
}

static cydui_color shade(cydui_color c, uint8_t amount)
{
    return cydui_blend(c, 0, amount); /* towards black */
}

/* Draws text inside `box` only, however long it is. */
static void draw_text_in(cydui_ui *ui, cydui_rect box, int x, int y, const char *s, size_t n,
                         cydui_color color)
{
    cydui_rect saved = ui->ctx.clip;

    cydui_set_clip(&ui->ctx, cydui_rect_intersect(saved, box));
    cydui_draw_text(&ui->ctx, ui->style.font, (int16_t)x, (int16_t)y, 0, CYDUI_ALIGN_LEFT, s, n,
                    color, 255);
    ui->ctx.clip = saved;
}

/* ---- painting ---- */

static void paint_button(cydui_ui *ui, cydui_rect r, const char *s, size_t n, cydui_text_extent e,
                         bool pressed, bool disabled)
{
    const cydui_style *st = &ui->style;
    cydui_color fill = disabled ? st->panel : (pressed ? shade(st->accent, 90) : st->accent);
    cydui_color text = disabled ? st->text_muted : st->accent_text;

    cydui_draw_rrect(&ui->ctx, r, st->radius, fill, 255);
    draw_text_in(ui, r, r.x + (r.w - e.w) / 2, r.y + (r.h - e.h) / 2, s, n, text);
}

static void paint_check_box(cydui_ui *ui, cydui_rect box, bool checked, bool pressed, bool disabled)
{
    const cydui_style *st = &ui->style;
    cydui_color edge = disabled ? dim(ui, st->border) : st->border;

    if (checked) {
        cydui_draw_rrect(&ui->ctx, box, 4, disabled ? dim(ui, st->accent) : st->accent, 255);
        cydui_draw_line(&ui->ctx, (int16_t)(box.x + 5), (int16_t)(box.y + 10), (int16_t)(box.x + 8),
                        (int16_t)(box.y + 13), 2, st->accent_text, 255);
        cydui_draw_line(&ui->ctx, (int16_t)(box.x + 8), (int16_t)(box.y + 13), (int16_t)(box.x + 15),
                        (int16_t)(box.y + 6), 2, st->accent_text, 255);
    } else {
        cydui_draw_rrect(&ui->ctx, box, 4, pressed ? shade(st->panel, 70) : st->panel, 255);
        cydui_draw_rrect_outline(&ui->ctx, box, 4, 2, edge, 255);
    }
}

static void paint_toggle(cydui_ui *ui, cydui_rect track, bool on, bool pressed, bool disabled)
{
    const cydui_style *st = &ui->style;
    cydui_color fill = on ? st->accent : st->border;
    cydui_rect knob;

    if (disabled) {
        fill = dim(ui, fill);
    } else if (pressed) {
        fill = shade(fill, 90);
    }
    cydui_draw_rrect(&ui->ctx, track, track.h / 2, fill, 255);
    knob.w = knob.h = KNOB;
    knob.y = (int16_t)(track.y + (track.h - KNOB) / 2);
    knob.x = (int16_t)(on ? track.x + track.w - KNOB - (track.h - KNOB) / 2 : track.x + (track.h - KNOB) / 2);
    cydui_draw_rrect(&ui->ctx, knob, KNOB / 2, disabled ? dim(ui, st->accent_text) : st->accent_text, 255);
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
    e = cydui_text_size(ui->style.font, text, n, 0);
    r = cydui_alloc(ui, &m, e.w, e.h);
    draw_text_in(ui, r, r.x, r.y, text, n, m.disabled ? ui->style.text_muted : ui->style.text);
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
    e = cydui_text_size(ui->style.font, text, n, (int16_t)wrap);
    r = cydui_alloc(ui, &m, e.w, e.h);
    {
        cydui_rect saved = ui->ctx.clip;
        cydui_set_clip(&ui->ctx, cydui_rect_intersect(saved, r));
        cydui_draw_text(&ui->ctx, ui->style.font, r.x, r.y, (int16_t)wrap, CYDUI_ALIGN_LEFT, text, n,
                        m.disabled ? ui->style.text_muted : ui->style.text, 255);
        ui->ctx.clip = saved;
    }
}

bool cydui_button(cydui_ui *ui, const char *label)
{
    cydui_mods m = cydui_take_mods(ui);
    const cydui_style *st = &ui->style;
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
    paint_button(ui, r, label, n, e, resp.pressed, m.disabled);
    return resp.clicked;
}

/* Shared by the check box and the switch: a control on the left, the label on the right. */
static bool control_with_label(cydui_ui *ui, const char *label, bool *value, int control_w,
                               int control_h, bool is_toggle)
{
    cydui_mods m = cydui_take_mods(ui);
    const cydui_style *st = &ui->style;
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
    if (is_toggle) {
        paint_toggle(ui, ctl, checked, resp.pressed, m.disabled);
    } else {
        paint_check_box(ui, ctl, checked, resp.pressed, m.disabled);
    }
    if (n) {
        draw_text_in(ui, r, r.x + control_w + LABEL_GAP, r.y + (r.h - e.h) / 2, label, n,
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
