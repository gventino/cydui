/*
 * The retro theme: the windows of the 1990s. A silver page, square corners, a monospaced 1-bit font, a
 * navy accent, and every control a slab with a 3D edge: raised to push, sunken to type into or to show a
 * value. The colors come from a small fixed palette (NFR-43: a theme is a table, and painters for what a
 * table cannot say: here the two-tone edges, which need four colors per widget).
 *
 * An edge is a stack of rectangles, each one a pixel smaller than the one below, so a slab costs five
 * commands (the keyboard on the screen has forty of them).
 */

#include <string.h>

#include "cydui/ui.h"
#include "cydui_mono_16.h"
#include "cydui_themes.h"

#define BLACK CYDUI_RGB(0, 0, 0)
#define WHITE CYDUI_RGB(255, 255, 255)
#define SILVER CYDUI_RGB(192, 192, 192)
#define LIGHT CYDUI_RGB(223, 223, 223)
#define GRAY CYDUI_RGB(128, 128, 128)
#define NAVY CYDUI_RGB(0, 0, 128)
#define RED CYDUI_RGB(192, 0, 0)

static void entry(cydui_theme *t, cydui_widget_kind k, int sel, cydui_widget_state st, cydui_color fill,
                  cydui_color border, cydui_color text, cydui_color accent, uint8_t bw)
{
    cydui_wstyle *w = &t->table[k][sel][st];
    w->fill = fill;
    w->border = border;
    w->text = text;
    w->accent = accent;
    w->border_w = bw;
    w->radius = 0;
    w->shadow = 0;
}

/* ---- the edges ---- */

static void rect(cydui_ui *ui, int x, int y, int w, int h, cydui_color c)
{
    cydui_draw_rect(&ui->ctx, (cydui_rect){(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h}, c, 255);
}

/*
 * The frame of a slab `bw` pixels thick (1 or 2); the face goes inside. Raised: light on the top and the
 * left, dark on the bottom and the right. Sunken: the other way round.
 */
static void frame(cydui_ui *ui, cydui_rect r, bool raised, int bw)
{
    const int x = r.x, y = r.y, w = r.w, h = r.h;

    if (w < 2 * bw || h < 2 * bw) {
        rect(ui, x, y, w, h, SILVER); /* too small for an edge */
        return;
    }
    if (bw == 1) {
        rect(ui, x, y, w, h, raised ? GRAY : WHITE);
        rect(ui, x, y, w - 1, h - 1, raised ? WHITE : GRAY);
        return;
    }
    if (raised) {
        rect(ui, x, y, w, h, BLACK);
        rect(ui, x, y, w - 1, h - 1, WHITE);
        rect(ui, x + 1, y + 1, w - 2, h - 2, GRAY);
        rect(ui, x + 1, y + 1, w - 3, h - 3, LIGHT);
    } else {
        rect(ui, x, y, w, h, WHITE);
        rect(ui, x, y, w - 1, h - 1, GRAY);
        rect(ui, x + 1, y + 1, w - 2, h - 2, LIGHT);
        rect(ui, x + 1, y + 1, w - 3, h - 3, BLACK);
    }
}

static cydui_rect inset(cydui_rect r, int d)
{
    r.x = (int16_t)(r.x + d);
    r.y = (int16_t)(r.y + d);
    r.w = (int16_t)(r.w > 2 * d ? r.w - 2 * d : 0);
    r.h = (int16_t)(r.h > 2 * d ? r.h - 2 * d : 0);
    return r;
}

/* A frame of 2 px and, inside it, the face and the content the table painter draws. */
static void slab(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p, bool raised)
{
    cydui_paint_info face = *p;

    frame(ui, p->rect, raised, 2);
    face.rect = inset(p->rect, 2);
    cydui_paint_default(ui, t, &face);
}

/* ---- the painters ---- */

static void paint_push(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    slab(ui, t, p, p->state != CYDUI_STATE_PRESSED && cydui_paint_amount(p) == 0);
}

static void paint_sunken(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    slab(ui, t, p, false);
}

static void paint_raised(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    slab(ui, t, p, true);
}

/* A tab that is the current one is pressed in; the others stand out. */
static void paint_tab(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    slab(ui, t, p, cydui_paint_amount(p) < CYDUI_ANIM_ONE / 2 && p->state != CYDUI_STATE_PRESSED);
}

/* A field: sunken, with a one pixel outline inside the edge for the focus (and red for a refused key). */
static void paint_field(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    const cydui_wstyle w = cydui_paint_look(t, p);

    slab(ui, t, p, false);
    if (w.border_w) {
        cydui_draw_rrect_outline(&ui->ctx, inset(p->rect, 2), 0, w.border_w, w.border, 255);
    }
}

/* A switch: a sunken slot with a raised slider in it, on the left when off. */
static void paint_toggle(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    const cydui_wstyle w = cydui_paint_look(t, p);
    const cydui_rect r = p->rect;
    cydui_rect inner = inset(r, 2), knob;
    int room, amount = cydui_paint_amount(p);

    frame(ui, r, false, 2);
    cydui_draw_rect(&ui->ctx, inner, w.fill, 255);
    knob.w = (int16_t)(inner.w / 2);
    knob.h = inner.h;
    room = inner.w - knob.w;
    knob.x = (int16_t)(inner.x + cydui_lerp(0, room, amount));
    knob.y = inner.y;
    frame(ui, knob, true, 2);
    cydui_draw_rect(&ui->ctx, inset(knob, 2), SILVER, 255);
}

/* A slider: a sunken groove, the part up to the knob in the accent, a raised knob. */
static void paint_slider(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    const cydui_wstyle w = cydui_paint_look(t, p);
    const cydui_rect r = p->rect;
    int cx = cydui_slider_knob_x(r, p->value, p->min, p->max);
    cydui_rect groove = {r.x, (int16_t)(r.y + (r.h - CYDUI_SLIDER_TRACK) / 2), r.w, CYDUI_SLIDER_TRACK}, done, knob;

    frame(ui, groove, false, 2);
    done = inset(groove, 2);
    cydui_draw_rect(&ui->ctx, done, w.fill, 255);
    done.w = (int16_t)(cx - done.x > 0 ? (cx - done.x < done.w ? cx - done.x : done.w) : 0);
    if (done.w > 0) {
        cydui_draw_rect(&ui->ctx, done, w.accent, 255);
    }
    knob.w = 12;
    knob.h = CYDUI_SLIDER_KNOB;
    knob.x = (int16_t)(cx - knob.w / 2);
    knob.y = (int16_t)(r.y + (r.h - CYDUI_SLIDER_KNOB) / 2);
    frame(ui, knob, true, 2);
    cydui_draw_rect(&ui->ctx, inset(knob, 2), w.text, 255);
}

/* A progress bar: a sunken trough and a navy bar. */
static void paint_progress(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    const cydui_wstyle w = cydui_paint_look(t, p);
    cydui_rect inner = inset(p->rect, 1), bar = inner;
    int64_t v = p->value, m = p->max;

    frame(ui, p->rect, false, 1);
    cydui_draw_rect(&ui->ctx, inner, w.fill, 255);
    if (m <= 0 || v <= 0 || inner.w <= 0) {
        return;
    }
    bar.w = (int16_t)((v > m ? m : v) * inner.w / m);
    if (bar.w > 0) {
        cydui_draw_rect(&ui->ctx, bar, w.accent, 255);
    }
}

/* The thumb of a scroll bar: a raised slab, as tall as the window is to the content. */
static void paint_scrollbar(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    const cydui_wstyle w = cydui_paint_look(t, p);
    int64_t span = p->max > 0 ? p->max : 0, pos = p->value < 0 ? 0 : p->value, total = span + p->aux.h;
    cydui_rect thumb = p->rect;

    if (p->rect.w <= 0 || p->rect.h <= 0 || span == 0 || total <= 0) {
        return;
    }
    pos = pos > span ? span : pos;
    thumb.h = (int16_t)((int64_t)p->rect.h * p->aux.h / total);
    if (thumb.h < 16) {
        thumb.h = p->rect.h < 16 ? p->rect.h : 16;
    }
    thumb.y = (int16_t)(p->rect.y + (p->rect.h - thumb.h) * pos / span);
    frame(ui, thumb, w.accent != 0 && p->state != CYDUI_STATE_PRESSED, 1);
    cydui_draw_rect(&ui->ctx, inset(thumb, 1), w.accent, 255);
}

/* ---- the theme ---- */

void cydui_theme_init_retro(cydui_theme *t)
{
    cydui_style *s;
    int k, sel;

    memset(t, 0, sizeof(*t));
    t->name = "retro";
    s = &t->style;
    s->bg = SILVER;
    s->panel = SILVER;
    s->text = BLACK;
    s->text_muted = GRAY;
    s->accent = NAVY;
    s->accent_text = WHITE;
    s->border = GRAY;
    s->font = &cydui_mono_16;
    s->radius = 0;
    s->pad_x = 10;
    s->pad_y = 6;
    s->spacing = 8;
    s->min_target = 32;
    s->anim_ms = 0;
    cydui_theme_derive_table(t);

    /* slabs that push */
    for (sel = 0; sel < 2; sel++) {
        entry(t, CYDUI_WIDGET_BUTTON, sel, CYDUI_STATE_NORMAL, SILVER, GRAY, BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_BUTTON, sel, CYDUI_STATE_PRESSED, SILVER, GRAY, BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_BUTTON, sel, CYDUI_STATE_DISABLED, SILVER, GRAY, GRAY, NAVY, 0);
        entry(t, CYDUI_WIDGET_KEY, sel, CYDUI_STATE_NORMAL, sel ? LIGHT : SILVER, GRAY, sel ? NAVY : BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_KEY, sel, CYDUI_STATE_PRESSED, sel ? LIGHT : SILVER, GRAY, sel ? NAVY : BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_TAB, sel, CYDUI_STATE_NORMAL, sel ? LIGHT : SILVER, GRAY, sel ? NAVY : BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_TAB, sel, CYDUI_STATE_PRESSED, sel ? LIGHT : SILVER, GRAY, sel ? NAVY : BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_TAB, sel, CYDUI_STATE_DISABLED, SILVER, GRAY, GRAY, NAVY, 0);
        entry(t, CYDUI_WIDGET_PANEL, sel, CYDUI_STATE_NORMAL, SILVER, GRAY, BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_POPUP, sel, CYDUI_STATE_NORMAL, SILVER, GRAY, BLACK, NAVY, 0);
        /* check boxes: a white sunken box with a black tick */
        entry(t, CYDUI_WIDGET_CHECKBOX, sel, CYDUI_STATE_NORMAL, WHITE, GRAY, BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_CHECKBOX, sel, CYDUI_STATE_PRESSED, LIGHT, GRAY, BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_CHECKBOX, sel, CYDUI_STATE_DISABLED, SILVER, GRAY, GRAY, NAVY, 0);
        /* switches: the slot is gray when off and navy when on */
        entry(t, CYDUI_WIDGET_TOGGLE, sel, CYDUI_STATE_NORMAL, sel ? NAVY : GRAY, GRAY, WHITE, NAVY, 0);
        entry(t, CYDUI_WIDGET_TOGGLE, sel, CYDUI_STATE_PRESSED, sel ? NAVY : GRAY, GRAY, WHITE, NAVY, 0);
        entry(t, CYDUI_WIDGET_TOGGLE, sel, CYDUI_STATE_DISABLED, SILVER, GRAY, WHITE, NAVY, 0);
        /* the groove of a slider, the trough of a progress bar: white, with the value in navy */
        entry(t, CYDUI_WIDGET_SLIDER, sel, CYDUI_STATE_NORMAL, WHITE, GRAY, SILVER, NAVY, 0);
        entry(t, CYDUI_WIDGET_SLIDER, sel, CYDUI_STATE_PRESSED, WHITE, GRAY, SILVER, NAVY, 0);
        entry(t, CYDUI_WIDGET_SLIDER, sel, CYDUI_STATE_DISABLED, SILVER, GRAY, SILVER, GRAY, 0);
        entry(t, CYDUI_WIDGET_PROGRESS, sel, CYDUI_STATE_NORMAL, WHITE, GRAY, BLACK, NAVY, 0);
        entry(t, CYDUI_WIDGET_PROGRESS, sel, CYDUI_STATE_DISABLED, SILVER, GRAY, GRAY, GRAY, 0);
        entry(t, CYDUI_WIDGET_SCROLLBAR, sel, CYDUI_STATE_NORMAL, SILVER, GRAY, BLACK, SILVER, 0);
        entry(t, CYDUI_WIDGET_SCROLLBAR, sel, CYDUI_STATE_PRESSED, SILVER, GRAY, BLACK, LIGHT, 0);
        /* a field: white inside; a navy outline with the focus, a red one when a key was refused */
        entry(t, CYDUI_WIDGET_TEXT_INPUT, sel, CYDUI_STATE_NORMAL, WHITE, sel ? RED : GRAY, BLACK, BLACK, sel ? 1 : 0);
        entry(t, CYDUI_WIDGET_TEXT_INPUT, sel, CYDUI_STATE_DISABLED, SILVER, GRAY, GRAY, BLACK, 0);
        entry(t, CYDUI_WIDGET_TEXT_INPUT, sel, CYDUI_STATE_FOCUSED, WHITE, sel ? RED : NAVY, BLACK, BLACK, 1);
    }
    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) {
        for (sel = 0; sel < 2; sel++) {
            t->table[k][sel][CYDUI_STATE_HOVER] = t->table[k][sel][CYDUI_STATE_NORMAL];
            if (k != CYDUI_WIDGET_TEXT_INPUT) {
                t->table[k][sel][CYDUI_STATE_FOCUSED] = t->table[k][sel][CYDUI_STATE_NORMAL];
            }
        }
    }
    t->paint[CYDUI_WIDGET_BUTTON] = paint_push;
    t->paint[CYDUI_WIDGET_KEY] = paint_push;
    t->paint[CYDUI_WIDGET_TAB] = paint_tab;
    t->paint[CYDUI_WIDGET_CHECKBOX] = paint_sunken;
    t->paint[CYDUI_WIDGET_TOGGLE] = paint_toggle;
    t->paint[CYDUI_WIDGET_SLIDER] = paint_slider;
    t->paint[CYDUI_WIDGET_PROGRESS] = paint_progress;
    t->paint[CYDUI_WIDGET_PANEL] = paint_raised;
    t->paint[CYDUI_WIDGET_POPUP] = paint_raised;
    t->paint[CYDUI_WIDGET_TEXT_INPUT] = paint_field;
    t->paint[CYDUI_WIDGET_SCROLLBAR] = paint_scrollbar;
}
