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

/* ---- the helpers a widget of your own needs too (see "writing a widget" in ui.h) ---- */

bool cydui_widget_begin(cydui_ui *ui)
{
    cydui__check_thread(ui);
    if (ui->widgets >= CYDUI_MAX_WIDGETS) {
        ui->ctx.errors |= CYDUI_ERR_TOO_MANY_WIDGETS;
        return false;
    }
    if (ui->frame_budget_ms && ui->in.has_clock) {
        const cydui_backend *be = ui->ctx.backend;
        if ((uint32_t)(be->now_ms(be->user) - ui->frame_start) > ui->frame_budget_ms) {
            ui->ctx.errors |= CYDUI_ERR_FRAME_BUDGET;
            return false;
        }
    }
    ui->widgets++;
    return true;
}

size_t cydui_label_len(const char *s)
{
    size_t n = 0;

    if (!s) {
        return 0;
    }
    while (n < CYDUI_MAX_STR && s[n] != '\0') {
        /* the byte after a '#' is only looked at if it is still inside the limit */
        if (s[n] == '#' && n + 1 < CYDUI_MAX_STR && s[n + 1] == '#') {
            break;
        }
        n++;
    }
    return n;
}

void cydui_draw_text_in(cydui_ui *ui, cydui_rect box, int x, int y, const char *s, size_t n, cydui_color color)
{
    cydui_rect saved = ui->ctx.clip;

    cydui_set_clip(&ui->ctx, cydui_rect_intersect(saved, box));
    cydui_draw_text(&ui->ctx, ui->theme->style.font, (int16_t)x, (int16_t)y, 0, CYDUI_ALIGN_LEFT, s, n,
                    color, 255);
    ui->ctx.clip = saved;
}

cydui_widget_state cydui_state_of(bool disabled, bool pressed)
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

    if (!cydui_widget_begin(ui)) {
        return;
    }
    text = text ? text : "";
    n = cydui_label_len(text);
    e = cydui_text_size(ui->theme->style.font, text, n, 0);
    r = cydui_alloc(ui, &m, e.w, e.h);
    cydui_draw_text_in(ui, r, r.x, r.y, text, n, m.disabled ? ui->theme->style.text_muted : ui->theme->style.text);
}

void cydui_label_wrapped(cydui_ui *ui, const char *text)
{
    cydui_mods m = cydui_take_mods(ui);
    int wrap;
    size_t n;
    cydui_text_extent e;
    cydui_rect r;

    if (!cydui_widget_begin(ui)) {
        return;
    }
    text = text ? text : "";
    n = cydui_label_len(text);
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

    if (!cydui_widget_begin(ui)) {
        return false;
    }
    label = label ? label : "";
    id = cydui_id_str(ui, label);
    cydui_id_claim(ui, id);
    n = cydui_label_len(label);
    e = cydui_text_size(st->font, label, n, 0);
    h = e.h + 2 * st->pad_y;
    r = cydui_alloc(ui, &m, e.w + 2 * st->pad_x, h < st->min_target ? st->min_target : h);
    resp = cydui_interact(ui, id, r, CYDUI_SENSE_CLICK | (m.disabled ? CYDUI_DISABLED : 0u));
    {
        cydui_paint_info pi;
        memset(&pi, 0, sizeof(pi));
        pi.kind = CYDUI_WIDGET_BUTTON;
        pi.state = cydui_state_of(m.disabled, resp.pressed);
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

    if (!cydui_widget_begin(ui)) {
        return false;
    }
    label = label ? label : "";
    id = cydui_id_str(ui, label);
    cydui_id_claim(ui, id);
    n = cydui_label_len(label);
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
        pi.state = cydui_state_of(m.disabled, resp.pressed);
        pi.selected = checked;
        pi.rect = ctl;
        cydui_paint(ui, &pi);
    }
    if (n) {
        cydui_draw_text_in(ui, r, r.x + control_w + LABEL_GAP, r.y + (r.h - e.h) / 2, label, n,
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

/* ---- slider, progress, tabs ---- */

#define SLIDER_DEFAULT_W 160
#define PROGRESS_H 8

bool cydui_slider(cydui_ui *ui, const char *label, int32_t *value, int32_t min, int32_t max)
{
    cydui_mods m = cydui_take_mods(ui);
    const cydui_style *st = &ui->theme->style;
    cydui_id id;
    cydui_text_extent e;
    cydui_response resp;
    cydui_rect r, track, hit;
    cydui_paint_info pi;
    size_t n;
    int h, label_w;
    bool usable = max > min, disabled, holding, changed = false;
    int32_t v;

    if (!cydui_widget_begin(ui)) {
        return false;
    }
    label = label ? label : "";
    id = cydui_id_str(ui, label);
    cydui_id_claim(ui, id);
    n = cydui_label_len(label);
    e = cydui_text_size(st->font, label, n, 0);
    label_w = n ? e.w + LABEL_GAP : 0;
    h = e.h > CYDUI_SLIDER_KNOB ? e.h : CYDUI_SLIDER_KNOB;
    if (h < st->min_target) {
        h = st->min_target;
    }
    r = cydui_alloc(ui, &m, label_w + SLIDER_DEFAULT_W, h);

    disabled = m.disabled || !usable;
    v = value ? *value : min;
    if (usable) {
        v = v < min ? min : (v > max ? max : v); /* a value out of range is clamped quietly */
    }
    track.x = (int16_t)(r.x + label_w);
    track.y = r.y;
    track.w = (int16_t)(r.w > label_w ? r.w - label_w : 0);
    track.h = r.h;
    hit = track; /* a little wider than the track, so that the end positions are easy to hit */
    hit.x = (int16_t)(hit.x - 4);
    hit.w = (int16_t)(hit.w + 8);

    resp = cydui_interact(ui, id, hit, CYDUI_SENSE_CLICK | CYDUI_SENSE_DRAG | CYDUI_CAPTURE |
                                           (disabled ? CYDUI_DISABLED : 0u));
    /* While the press is ours the knob follows the finger, also when it is outside the track. */
    holding = (ui->in.down && cydui_id_eq(ui->active, id)) || resp.released;
    if (holding && !disabled) {
        int32_t nv = cydui_slider_value_at(track, resp.x, min, max);
        if (nv != v) {
            v = nv;
            changed = true;
        }
    }
    if (value && usable) {
        *value = v;
    }

    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_SLIDER;
    pi.state = cydui_state_of(disabled, holding && !disabled);
    pi.rect = track;
    pi.value = v;
    pi.min = min;
    pi.max = max;
    cydui_paint(ui, &pi);
    if (n) {
        cydui_draw_text_in(ui, r, r.x, r.y + (r.h - e.h) / 2, label, n,
                            disabled ? st->text_muted : st->text);
    }
    return changed;
}

void cydui_progress(cydui_ui *ui, int32_t value, int32_t max)
{
    cydui_mods m = cydui_take_mods(ui);
    cydui_paint_info pi;
    cydui_rect r;

    if (!cydui_widget_begin(ui)) {
        return;
    }
    r = cydui_alloc(ui, &m, SLIDER_DEFAULT_W, PROGRESS_H);
    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_PROGRESS;
    pi.state = cydui_state_of(m.disabled, false);
    pi.rect = r;
    pi.value = value;
    pi.min = 0;
    pi.max = max;
    cydui_paint(ui, &pi);
}

bool cydui_tabs(cydui_ui *ui, const char *const *labels, int count, int *selected)
{
    cydui_mods m = cydui_take_mods(ui);
    const cydui_style *st = &ui->theme->style;
    cydui_response resp[CYDUI_MAX_TABS];
    cydui_rect tab[CYDUI_MAX_TABS];
    size_t len[CYDUI_MAX_TABS];
    cydui_text_extent ext[CYDUI_MAX_TABS];
    cydui_rect bar;
    int i, sel, chosen = -1, h, tab_w;
    bool changed = false;

    if (!cydui_widget_begin(ui)) {
        return false;
    }
    count = count > CYDUI_MAX_TABS ? CYDUI_MAX_TABS : count;
    if (count <= 0 || !labels) {
        return false;
    }
    sel = selected ? *selected : 0;
    sel = sel < 0 ? 0 : (sel >= count ? count - 1 : sel);

    h = st->min_target;
    bar = cydui_alloc(ui, &m, cydui_layout_avail(ui).w, h);
    tab_w = bar.w / count;

    for (i = 0; i < count; i++) { /* first all the touches, so that the painting can show the new choice */
        const char *label = labels[i] ? labels[i] : "";
        cydui_id id;

        tab[i].x = (int16_t)(bar.x + i * tab_w);
        tab[i].y = bar.y;
        tab[i].w = (int16_t)(i == count - 1 ? bar.w - i * tab_w : tab_w);
        tab[i].h = bar.h;
        len[i] = cydui_label_len(label);
        ext[i] = cydui_text_size(st->font, label, len[i], 0);
        cydui_push_id_int(ui, i); /* equal labels are fine: every tab has its own id */
        id = cydui_id_str(ui, label);
        cydui_id_claim(ui, id);
        resp[i] = cydui_interact(ui, id, tab[i], CYDUI_SENSE_CLICK | (m.disabled ? CYDUI_DISABLED : 0u));
        cydui_pop_id(ui);
        if (resp[i].clicked) {
            chosen = i;
        }
    }
    if (chosen >= 0 && chosen != sel) {
        sel = chosen;
        changed = true;
    }
    if (selected) {
        *selected = sel;
    }
    for (i = 0; i < count; i++) {
        cydui_paint_info pi;
        memset(&pi, 0, sizeof(pi));
        pi.kind = CYDUI_WIDGET_TAB;
        pi.state = cydui_state_of(m.disabled, resp[i].pressed);
        pi.selected = i == sel;
        pi.rect = tab[i];
        pi.text = labels[i] ? labels[i] : "";
        pi.text_len = len[i];
        pi.text_extent = ext[i];
        cydui_paint(ui, &pi);
    }
    return changed;
}
