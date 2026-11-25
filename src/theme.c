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
    s.anim_ms = 0;
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

    /* sliders: a pill track, the part up to the knob in the accent, a ringed knob */
    set(t, CYDUI_WIDGET_SLIDER, 0, CYDUI_STATE_NORMAL, s->border, s->accent, s->text, s->accent, 2, 255);
    set(t, CYDUI_WIDGET_SLIDER, 0, CYDUI_STATE_PRESSED, s->border, shade(s->accent, 90), s->text, s->accent, 2, 255);
    set(t, CYDUI_WIDGET_SLIDER, 0, CYDUI_STATE_DISABLED, dim(s, s->border), dim(s, s->accent), dim(s, s->text), dim(s, s->accent), 2, 255);

    /* progress bars: a pill trough and a pill bar */
    set(t, CYDUI_WIDGET_PROGRESS, 0, CYDUI_STATE_NORMAL, s->panel, s->border, s->text, s->accent, 0, 255);
    set(t, CYDUI_WIDGET_PROGRESS, 0, CYDUI_STATE_DISABLED, s->panel, s->border, s->text_muted, dim(s, s->accent), 0, 255);

    /* tabs: the active one is lifted onto a panel and underlined in the accent */
    set(t, CYDUI_WIDGET_TAB, 0, CYDUI_STATE_NORMAL, s->bg, s->border, s->text_muted, s->accent, 0, s->radius);
    set(t, CYDUI_WIDGET_TAB, 0, CYDUI_STATE_PRESSED, s->panel, s->border, s->text, s->accent, 0, s->radius);
    set(t, CYDUI_WIDGET_TAB, 0, CYDUI_STATE_DISABLED, s->bg, s->border, dim(s, s->text_muted), s->accent, 0, s->radius);
    set(t, CYDUI_WIDGET_TAB, 1, CYDUI_STATE_NORMAL, s->panel, s->border, s->accent, s->accent, 0, s->radius);
    set(t, CYDUI_WIDGET_TAB, 1, CYDUI_STATE_PRESSED, shade(s->panel, 90), s->border, s->accent, s->accent, 0, s->radius);
    set(t, CYDUI_WIDGET_TAB, 1, CYDUI_STATE_DISABLED, s->panel, s->border, dim(s, s->accent), dim(s, s->accent), 0, s->radius);

    /* cards */
    set(t, CYDUI_WIDGET_PANEL, 0, CYDUI_STATE_NORMAL, s->panel, s->border, s->text, s->accent, 1, 8);
    set(t, CYDUI_WIDGET_PANEL, 1, CYDUI_STATE_NORMAL, s->panel, s->accent, s->text, s->accent, 1, 8);

    /* scroll bars: a thumb only */
    set(t, CYDUI_WIDGET_SCROLLBAR, 0, CYDUI_STATE_NORMAL, s->bg, s->border, s->text, s->border, 0, 255);
    set(t, CYDUI_WIDGET_SCROLLBAR, 0, CYDUI_STATE_PRESSED, s->bg, s->border, s->text, s->text_muted, 0, 255);

    set(t, CYDUI_WIDGET_POPUP, 0, CYDUI_STATE_NORMAL, s->panel, s->border, s->text, s->accent, 1, 12);
    set(t, CYDUI_WIDGET_KEY, 0, CYDUI_STATE_NORMAL, shade(s->panel, 85), s->border, s->text, s->accent, 0, 6);
    set(t, CYDUI_WIDGET_KEY, 0, CYDUI_STATE_PRESSED, shade(s->panel, 60), s->border, s->text, s->accent, 0, 6);
    set(t, CYDUI_WIDGET_KEY, 1, CYDUI_STATE_NORMAL, s->accent, s->border, s->accent_text, s->accent, 0, 6);
    set(t, CYDUI_WIDGET_KEY, 1, CYDUI_STATE_PRESSED, shade(s->accent, 90), s->border, s->accent_text, s->accent, 0, 6);
    /* a text field: the background, a thin outline, a thick accent one with the focus, a red one (selected) when a key was refused */
    set(t, CYDUI_WIDGET_TEXT_INPUT, 0, CYDUI_STATE_NORMAL, s->bg, s->border, s->text, s->accent, 1, s->radius);
    set(t, CYDUI_WIDGET_TEXT_INPUT, 0, CYDUI_STATE_DISABLED, s->panel, s->border, s->text_muted, s->accent, 1, s->radius);
    set(t, CYDUI_WIDGET_TEXT_INPUT, 1, CYDUI_STATE_NORMAL, s->bg, CYDUI_RGB(220, 60, 60), s->text, s->accent, 2, s->radius);

    /* hover and focus look like normal until something produces them */
    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) {
        for (sel = 0; sel < 2; sel++) {
            t->table[k][sel][CYDUI_STATE_HOVER] = t->table[k][sel][CYDUI_STATE_NORMAL];
            t->table[k][sel][CYDUI_STATE_FOCUSED] = t->table[k][sel][CYDUI_STATE_NORMAL];
        }
    }
    set(t, CYDUI_WIDGET_TEXT_INPUT, 0, CYDUI_STATE_FOCUSED, s->bg, s->accent, s->text, s->accent, 2, s->radius);
    set(t, CYDUI_WIDGET_TEXT_INPUT, 1, CYDUI_STATE_FOCUSED, s->bg, CYDUI_RGB(220, 60, 60), s->text, s->accent, 2, s->radius);
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

/* ---- slider geometry ---- */

static void knob_range(cydui_rect track, int *x0, int *x1)
{
    *x0 = track.x + CYDUI_SLIDER_KNOB / 2;
    *x1 = track.x + track.w - CYDUI_SLIDER_KNOB / 2;
    if (*x1 < *x0) {
        *x1 = *x0;
    }
}

int cydui_slider_knob_x(cydui_rect track, int32_t value, int32_t min, int32_t max)
{
    int x0, x1;
    int64_t span;

    knob_range(track, &x0, &x1);
    if (max <= min) {
        return x0;
    }
    if (value < min) value = min;
    if (value > max) value = max;
    span = (int64_t)max - (int64_t)min;
    return x0 + (int)(((int64_t)value - (int64_t)min) * (int64_t)(x1 - x0) / span);
}

int32_t cydui_slider_value_at(cydui_rect track, int x, int32_t min, int32_t max)
{
    int x0, x1, run;
    int64_t span, num, v;

    knob_range(track, &x0, &x1);
    if (max <= min || x1 <= x0) {
        return min;
    }
    run = x1 - x0;
    if (x < x0) x = x0;
    if (x > x1) x = x1;
    span = (int64_t)max - (int64_t)min;
    num = (int64_t)(x - x0) * span;
    v = (int64_t)min + (num * 2 + run) / ((int64_t)run * 2); /* rounded to the nearest */
    return (int32_t)v; /* x is clamped, so v is within [min, max] */
}

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

/* The slider: a pill track, the stretch up to the knob in the accent, a round knob with a ring. */
static void paint_slider(cydui_ui *ui, const cydui_wstyle *w, const cydui_paint_info *p)
{
    const cydui_rect r = p->rect;
    cydui_rect track, done, knob;
    int cx = cydui_slider_knob_x(r, p->value, p->min, p->max);

    track.x = r.x;
    track.w = r.w;
    track.h = CYDUI_SLIDER_TRACK;
    track.y = (int16_t)(r.y + (r.h - CYDUI_SLIDER_TRACK) / 2);
    cydui_draw_rrect(&ui->ctx, track, w->radius, w->fill, 255);

    done = track;
    done.w = (int16_t)(cx - track.x);
    if (done.w > 0) {
        cydui_draw_rrect(&ui->ctx, done, w->radius, w->accent, 255);
    }
    knob.w = knob.h = CYDUI_SLIDER_KNOB;
    knob.x = (int16_t)(cx - CYDUI_SLIDER_KNOB / 2);
    knob.y = (int16_t)(r.y + (r.h - CYDUI_SLIDER_KNOB) / 2);
    if (w->shadow) {
        paint_shadow(ui, knob, CYDUI_SLIDER_KNOB / 2, w->shadow);
    }
    cydui_draw_rrect(&ui->ctx, knob, CYDUI_SLIDER_KNOB / 2, w->text, 255);
    if (w->border_w) {
        cydui_draw_rrect_outline(&ui->ctx, knob, CYDUI_SLIDER_KNOB / 2, w->border_w, w->border, 255);
    }
}

/* A progress bar: a trough and, from its left end, a bar as long as value / max of it. */
static void paint_progress(cydui_ui *ui, const cydui_wstyle *w, const cydui_paint_info *p)
{
    const cydui_rect r = p->rect;
    cydui_rect bar = r;
    int64_t v = p->value, m = p->max;

    cydui_draw_rrect(&ui->ctx, r, w->radius, w->fill, 255);
    if (w->border_w) {
        cydui_draw_rrect_outline(&ui->ctx, r, w->radius, w->border_w, w->border, 255);
    }
    if (m <= 0 || v <= 0 || r.w <= 0) {
        return;
    }
    if (v > m) {
        v = m;
    }
    bar.w = (int16_t)(v * (int64_t)r.w / m);
    if (bar.w > 0) {
        cydui_draw_rrect(&ui->ctx, bar, w->radius, w->accent, 255);
    }
}

/* A tab: a rounded body, the label centered, and for the active one an accent underline. */
static void paint_tab(cydui_ui *ui, const cydui_wstyle *w, const cydui_paint_info *p)
{
    const cydui_rect r = p->rect;
    const cydui_text_extent e = p->text_extent;

    cydui_draw_rrect(&ui->ctx, r, w->radius, w->fill, 255);
    if (cydui_paint_amount(p) > 0) { /* a bar too narrow to exist is simply not drawn; it grows from the middle */
        int full = r.w - 8, grown = (int)((int64_t)full * cydui_paint_amount(p) / CYDUI_ANIM_ONE);
        cydui_rect bar = {(int16_t)(r.x + 4 + (full - grown) / 2), (int16_t)(r.y + r.h - 3), (int16_t)grown, 3};
        cydui_draw_rrect(&ui->ctx, bar, 1, w->accent, 255);
    }
    if (p->text && p->text_len) {
        cydui_draw_text_in(ui, r, r.x + (r.w - e.w) / 2, r.y + (r.h - e.h) / 2, p->text, p->text_len, w->text);
    }
}

/* A scroll bar: only the thumb, as tall as the window is to the content (never under 16 px). */
static void paint_scrollbar(cydui_ui *ui, const cydui_wstyle *w, const cydui_paint_info *p)
{
    const cydui_rect r = p->rect;
    int64_t span = p->max > 0 ? p->max : 0, pos = p->value < 0 ? 0 : p->value;
    int64_t total = span + p->aux.h, room;
    cydui_rect thumb = r;

    if (r.w <= 0 || r.h <= 0 || span == 0 || total <= 0) {
        return;
    }
    pos = pos > span ? span : pos;
    thumb.h = (int16_t)((int64_t)r.h * p->aux.h / total);
    if (thumb.h < 16) {
        thumb.h = r.h < 16 ? r.h : 16;
    }
    room = r.h - thumb.h;
    thumb.y = (int16_t)(r.y + room * pos / span);
    cydui_draw_rrect(&ui->ctx, thumb, w->radius, w->accent, 255);
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
        cydui_draw_text_in(ui, r, r.x + (r.w - e.w) / 2, r.y + (r.h - e.h) / 2, p->text, p->text_len,
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
    if (cydui_paint_amount(p) >= CYDUI_ANIM_ONE / 2) {
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
    knob.x = (int16_t)cydui_lerp(track.x + (track.h - KNOB) / 2, track.x + track.w - KNOB - (track.h - KNOB) / 2,
                                 cydui_paint_amount(p));
    cydui_draw_rrect(&ui->ctx, knob, KNOB / 2, w->text, 255);
}

static uint8_t lerp_u8(uint8_t a, uint8_t b, int32_t k)
{
    int32_t v = cydui_lerp(a, b, k);
    return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
}

cydui_wstyle cydui_paint_look(const cydui_theme *t, const cydui_paint_info *p)
{
    cydui_wstyle w = *entry(t, p->kind, p->state, p->selected);

    if (p->mix > 0 && p->mix < CYDUI_ANIM_ONE) {
        const cydui_wstyle *f = entry(t, p->kind, p->from_state, p->from_selected);
        uint8_t a = (uint8_t)((uint32_t)p->mix * 255u / CYDUI_ANIM_ONE);

        w.fill = cydui_blend(f->fill, w.fill, a);
        w.border = cydui_blend(f->border, w.border, a);
        w.text = cydui_blend(f->text, w.text, a);
        w.accent = cydui_blend(f->accent, w.accent, a);
        w.border_w = lerp_u8(f->border_w, w.border_w, p->mix);
        w.radius = lerp_u8(f->radius, w.radius, p->mix);
        w.shadow = lerp_u8(f->shadow, w.shadow, p->mix);
    }
    return w;
}

int32_t cydui_paint_amount(const cydui_paint_info *p)
{
    if (p->mix == 0 || p->mix >= CYDUI_ANIM_ONE) {
        return p->selected ? CYDUI_ANIM_ONE : 0;
    }
    return p->selected ? p->mix : CYDUI_ANIM_ONE - p->mix;
}

void cydui_paint_default(cydui_ui *ui, const cydui_theme *theme, const cydui_paint_info *info)
{
    cydui_wstyle look;
    const cydui_wstyle *w = &look;

    if (!ui || !theme || !info) {
        return;
    }
    look = cydui_paint_look(theme, info);
    if (w->shadow && info->rect.w > 0 && info->rect.h > 0) {
        paint_shadow(ui, info->rect, w->radius, w->shadow);
    }
    switch (info->kind) {
    case CYDUI_WIDGET_BUTTON:
    case CYDUI_WIDGET_KEY:
        paint_button(ui, w, info);
        break;
    case CYDUI_WIDGET_CHECKBOX:
        paint_check_box(ui, w, info);
        break;
    case CYDUI_WIDGET_TOGGLE:
        paint_toggle(ui, w, info);
        break;
    case CYDUI_WIDGET_SLIDER:
        paint_slider(ui, w, info);
        break;
    case CYDUI_WIDGET_PROGRESS:
        paint_progress(ui, w, info);
        break;
    case CYDUI_WIDGET_TAB:
        paint_tab(ui, w, info);
        break;
    case CYDUI_WIDGET_PANEL:
    case CYDUI_WIDGET_POPUP:
    case CYDUI_WIDGET_TEXT_INPUT:
        paint_panel(ui, w, info);
        break;
    case CYDUI_WIDGET_SCROLLBAR:
        paint_scrollbar(ui, w, info);
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
    cydui__install_post_effect(ui);
    ui->relayout = true; /* fonts and metrics may have changed the size of everything */
    return 0;
}
