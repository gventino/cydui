#include <string.h>

#include "ui_internal.h"

#define KNOB 18 /* the diameter of a switch knob */

/* ---- the built-in theme ---- */

cydui_style cydui_style_default(const cydui_font *font)
{
    cydui_style s;
    s.bg = CYDUI_RGB(16, 20, 32);
    s.panel = CYDUI_RGB(38, 46, 70);
    s.text = CYDUI_RGB(235, 238, 250);
    s.text_muted = CYDUI_RGB(128, 136, 160);
    s.accent = CYDUI_RGB(70, 120, 230);
    s.accent_text = CYDUI_RGB(255, 255, 255);
    s.border = CYDUI_RGB(92, 104, 150);
    s.font = font;
    s.radius = 6;
    s.pad_x = 10;
    s.pad_y = 6;
    s.spacing = 8;
    s.min_target = 32;
    return s;
}

static cydui_color shade(cydui_color c, uint8_t amount)
{
    return cydui_blend(c, 0, amount); /* towards black */
}

static cydui_color dim(const cydui_style *s, cydui_color c)
{
    return cydui_blend(s->bg, c, 110); /* about 43% of the colour over the background */
}

static void set(cydui_theme *t, cydui_widget_kind k, int sel, cydui_widget_state st, cydui_color fill,
                cydui_color border, cydui_color text, cydui_color accent, uint8_t border_w,
                uint8_t radius)
{
    cydui_wstyle *w = &t->table[k][sel][st];
    w->fill = fill;
    w->border = border;
    w->text = text;
    w->accent = accent;
    w->border_w = border_w;
    w->radius = radius;
    w->shadow = 0;
}

void cydui_theme_derive_table(cydui_theme *t)
{
    const cydui_style *s = &t->style;
    int k, sel, st;

    /* everything starts as a plain panel; the widgets below override what they need */
    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) {
        for (sel = 0; sel < 2; sel++) {
            for (st = 0; st < CYDUI_STATE_COUNT; st++) {
                set(t, (cydui_widget_kind)k, sel, (cydui_widget_state)st, s->panel, s->border, s->text,
                    s->accent, 0, s->radius);
            }
        }
    }

    /* buttons: accent, darker when pressed, a flat panel when disabled */
    for (sel = 0; sel < 2; sel++) {
        set(t, CYDUI_WIDGET_BUTTON, sel, CYDUI_STATE_NORMAL, s->accent, s->border, s->accent_text, s->accent, 0, s->radius);
        set(t, CYDUI_WIDGET_BUTTON, sel, CYDUI_STATE_PRESSED, shade(s->accent, 90), s->border, s->accent_text, s->accent, 0, s->radius);
        set(t, CYDUI_WIDGET_BUTTON, sel, CYDUI_STATE_DISABLED, s->panel, s->border, s->text_muted, s->accent, 0, s->radius);
    }

    /* check boxes: an outlined box, filled with the accent and a check mark when checked */
    set(t, CYDUI_WIDGET_CHECKBOX, 0, CYDUI_STATE_NORMAL, s->panel, s->border, s->accent_text, s->accent, 2, 4);
    set(t, CYDUI_WIDGET_CHECKBOX, 0, CYDUI_STATE_PRESSED, shade(s->panel, 70), s->border, s->accent_text, s->accent, 2, 4);
    set(t, CYDUI_WIDGET_CHECKBOX, 0, CYDUI_STATE_DISABLED, s->panel, dim(s, s->border), s->accent_text, s->accent, 2, 4);
    set(t, CYDUI_WIDGET_CHECKBOX, 1, CYDUI_STATE_NORMAL, s->accent, s->border, s->accent_text, s->accent, 0, 4);
    set(t, CYDUI_WIDGET_CHECKBOX, 1, CYDUI_STATE_PRESSED, s->accent, s->border, s->accent_text, s->accent, 0, 4);
    set(t, CYDUI_WIDGET_CHECKBOX, 1, CYDUI_STATE_DISABLED, dim(s, s->accent), s->border, s->accent_text, s->accent, 0, 4);

    /* switches: a pill track (the border color when off, the accent when on) and a round knob */
    set(t, CYDUI_WIDGET_TOGGLE, 0, CYDUI_STATE_NORMAL, s->border, s->border, s->accent_text, s->accent, 0, 255);
    set(t, CYDUI_WIDGET_TOGGLE, 0, CYDUI_STATE_PRESSED, shade(s->border, 90), s->border, s->accent_text, s->accent, 0, 255);
    set(t, CYDUI_WIDGET_TOGGLE, 0, CYDUI_STATE_DISABLED, dim(s, s->border), s->border, dim(s, s->accent_text), s->accent, 0, 255);
    set(t, CYDUI_WIDGET_TOGGLE, 1, CYDUI_STATE_NORMAL, s->accent, s->border, s->accent_text, s->accent, 0, 255);
    set(t, CYDUI_WIDGET_TOGGLE, 1, CYDUI_STATE_PRESSED, shade(s->accent, 90), s->border, s->accent_text, s->accent, 0, 255);
    set(t, CYDUI_WIDGET_TOGGLE, 1, CYDUI_STATE_DISABLED, dim(s, s->accent), s->border, dim(s, s->accent_text), s->accent, 0, 255);

    /* hover and focus look like normal until something produces them */
    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) {
        for (sel = 0; sel < 2; sel++) {
            t->table[k][sel][CYDUI_STATE_HOVER] = t->table[k][sel][CYDUI_STATE_NORMAL];
            t->table[k][sel][CYDUI_STATE_FOCUSED] = t->table[k][sel][CYDUI_STATE_NORMAL];
        }
    }
}

void cydui_theme_default(cydui_theme *t, const cydui_font *font)
{
    memset(t, 0, sizeof(*t));
    t->name = "default";
    t->style = cydui_style_default(font);
    cydui_theme_derive_table(t);
}

cydui_theme_status cydui_theme_validate(const cydui_theme *t)
{
    if (!t) {
        return CYDUI_THEME_ERR_NULL;
    }
    if (!cydui_font_fallback(t->style.font)) {
        return CYDUI_THEME_ERR_FONT;
    }
    return CYDUI_THEME_OK;
}

/* ---- painting ---- */

static const cydui_wstyle *entry(const cydui_theme *t, cydui_widget_kind kind, cydui_widget_state st,
                                 bool selected)
{
    unsigned k = (unsigned)kind < CYDUI_WIDGET_COUNT ? (unsigned)kind : 0u;
    unsigned s = (unsigned)st < CYDUI_STATE_COUNT ? (unsigned)st : (unsigned)CYDUI_STATE_NORMAL;
    return &t->table[k][selected ? 1 : 0][s];
}

const cydui_wstyle *cydui_wstyle_of(const cydui_ui *ui, cydui_widget_kind kind, cydui_widget_state state,
                                    bool selected)
{
    return entry(ui->theme, kind, state, selected);
}

const cydui_theme *cydui_theme_of(const cydui_ui *ui)
{
    return ui->theme;
}

/*
 * A soft drop shadow from stacked translucent rounded rectangles, each a little larger than the one
 * inside it and all lowered a little: where they overlap the shadow gets darker, so the edge fades.
 * `strength` is how far it spreads, in pixels (at most 8).
 */
static void paint_shadow(cydui_ui *ui, cydui_rect r, int radius, int strength)
{
    int layers = strength > 8 ? 8 : strength, i;

    for (i = layers; i >= 1; i--) {
        cydui_rect q;
        q.x = (int16_t)(r.x - i);
        q.y = (int16_t)(r.y - i + (layers + 1) / 2);
        q.w = (int16_t)(r.w + 2 * i);
        q.h = (int16_t)(r.h + 2 * i);
        cydui_draw_rrect(&ui->ctx, q, (uint8_t)(radius + i > 255 ? 255 : radius + i), 0, 8);
    }
}

static void paint_panel(cydui_ui *ui, const cydui_wstyle *w, const cydui_paint_info *p)
{
    cydui_draw_rrect(&ui->ctx, p->rect, w->radius, w->fill, 255);
    if (w->border_w) {
        cydui_draw_rrect_outline(&ui->ctx, p->rect, w->radius, w->border_w, w->border, 255);
    }
}

static void paint_button(cydui_ui *ui, const cydui_wstyle *w, const cydui_paint_info *p)
{
    const cydui_rect r = p->rect;
    const cydui_text_extent e = p->text_extent;

    cydui_draw_rrect(&ui->ctx, r, w->radius, w->fill, 255);
    if (w->border_w) {
        cydui_draw_rrect_outline(&ui->ctx, r, w->radius, w->border_w, w->border, 255);
    }
    if (p->text && p->text_len) {
        cydui__draw_text_in(ui, r, r.x + (r.w - e.w) / 2, r.y + (r.h - e.h) / 2, p->text, p->text_len,
                            w->text);
    }
}

static void paint_check_box(cydui_ui *ui, const cydui_wstyle *w, const cydui_paint_info *p)
{
    const cydui_rect box = p->rect;

    cydui_draw_rrect(&ui->ctx, box, w->radius, w->fill, 255);
    if (w->border_w) {
        cydui_draw_rrect_outline(&ui->ctx, box, w->radius, w->border_w, w->border, 255);
    }
    if (p->selected) {
        cydui_draw_line(&ui->ctx, (int16_t)(box.x + 5), (int16_t)(box.y + 10), (int16_t)(box.x + 8),
                        (int16_t)(box.y + 13), 2, w->text, 255);
        cydui_draw_line(&ui->ctx, (int16_t)(box.x + 8), (int16_t)(box.y + 13), (int16_t)(box.x + 15),
                        (int16_t)(box.y + 6), 2, w->text, 255);
    }
}

static void paint_toggle(cydui_ui *ui, const cydui_wstyle *w, const cydui_paint_info *p)
{
    const cydui_rect track = p->rect;
    cydui_rect knob;

    cydui_draw_rrect(&ui->ctx, track, w->radius, w->fill, 255);
    knob.w = knob.h = KNOB;
    knob.y = (int16_t)(track.y + (track.h - KNOB) / 2);
    knob.x = (int16_t)(p->selected ? track.x + track.w - KNOB - (track.h - KNOB) / 2
                                   : track.x + (track.h - KNOB) / 2);
    cydui_draw_rrect(&ui->ctx, knob, KNOB / 2, w->text, 255);
}

void cydui_paint_default(cydui_ui *ui, const cydui_theme *theme, const cydui_paint_info *info)
{
    const cydui_wstyle *w;

    if (!ui || !theme || !info) {
        return;
    }
    w = entry(theme, info->kind, info->state, info->selected);
    if (w->shadow && info->rect.w > 0 && info->rect.h > 0) {
        paint_shadow(ui, info->rect, w->radius, w->shadow);
    }
    switch (info->kind) {
    case CYDUI_WIDGET_BUTTON:
        paint_button(ui, w, info);
        break;
    case CYDUI_WIDGET_CHECKBOX:
        paint_check_box(ui, w, info);
        break;
    case CYDUI_WIDGET_TOGGLE:
        paint_toggle(ui, w, info);
        break;
    case CYDUI_WIDGET_PANEL:
        paint_panel(ui, w, info);
        break;
    default: /* the other widgets get their painting with the widgets */
        break;
    }
}

void cydui_paint(cydui_ui *ui, const cydui_paint_info *info)
{
    const cydui_theme *t = ui->theme;

    cydui__check_thread(ui);
    if (!info) {
        return;
    }
    if ((unsigned)info->kind < CYDUI_WIDGET_COUNT && t->paint[info->kind]) {
        t->paint[info->kind](ui, t, info);
    } else {
        cydui_paint_default(ui, t, info);
    }
}

uint32_t cydui_set_theme(cydui_ui *ui, const cydui_theme *t)
{
    cydui__check_thread(ui);
    if (cydui_theme_validate(t) != CYDUI_THEME_OK) {
        ui->ctx.errors |= CYDUI_ERR_BAD_ARG;
        return CYDUI_ERR_BAD_ARG; /* the theme in use stays */
    }
    ui->theme = t;
    ui->relayout = true; /* fonts and metrics may have changed the size of everything */
    return 0;
}
