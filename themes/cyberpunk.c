/*
 * The cyberpunk theme: a near black page, neon cyan and magenta, thin bright outlines, and light that
 * spills: a pressed, selected or focused control glows (translucent rings around it). What a table cannot
 * say is done by a post-effect (FR-07) that follows the clock: scanlines on every other row, a short
 * glitch every few seconds (a few bands of rows jump sideways), and a sweep that draws the screen from the
 * top when the theme comes in. The look moves over `anim_ms` (the glow fades in and out), and
 * cydui_set_effects_level turns the light show down: LOW keeps the scanlines, NONE only the colors.
 *
 * Integer only and a pure function of the row and the clock, so a frame is the same on the PC and on
 * the board (NFR-30); the effect names the rows it changes (its key), so dirty regions keep sending
 * only those.
 */

#include <string.h>

#include "cydui/ui.h"
#include "cydui_sans_16.h"
#include "cydui_themes.h"

#define BG CYDUI_RGB(7, 5, 16)
#define PANEL CYDUI_RGB(15, 11, 30)
#define TILE CYDUI_RGB(22, 16, 44)
#define TEXT CYDUI_RGB(222, 230, 255)
#define MUTED CYDUI_RGB(112, 104, 154)
#define CYAN CYDUI_RGB(0, 238, 255)
#define CYAN_DIM CYDUI_RGB(0, 120, 140)
#define MAGENTA CYDUI_RGB(255, 40, 150)
#define VIOLET CYDUI_RGB(70, 40, 120)
#define INK CYDUI_RGB(7, 5, 16)

static void entry(cydui_theme *t, cydui_widget_kind k, int sel, cydui_widget_state st, cydui_color fill,
                  cydui_color border, cydui_color text, cydui_color accent, uint8_t bw, uint8_t radius)
{
    cydui_wstyle *w = &t->table[k][sel][st];
    w->fill = fill;
    w->border = border;
    w->text = text;
    w->accent = accent;
    w->border_w = bw;
    w->radius = radius;
    w->shadow = 0;
}

/* ---- light: rings that fade away from a widget ---- */

#define RING_ALPHA_1 80
#define RING_ALPHA_2 42
#define RING_ALPHA_3 18

/* `amount` 0 .. CYDUI_ANIM_ONE of the full glow, in `color`, around `r` with corners of `radius`. */
static void glow(cydui_ui *ui, cydui_rect r, int radius, cydui_color color, int32_t amount)
{
    static const uint8_t alpha[3] = {RING_ALPHA_1, RING_ALPHA_2, RING_ALPHA_3};
    int i;

    if (amount <= 0 || cydui_effects_level(ui) < CYDUI_FX_FULL || r.w <= 0 || r.h <= 0) {
        return;
    }
    for (i = 1; i <= 3; i++) {
        cydui_rect q = {(int16_t)(r.x - i), (int16_t)(r.y - i), (int16_t)(r.w + 2 * i), (int16_t)(r.h + 2 * i)};
        uint8_t a = (uint8_t)((int64_t)alpha[i - 1] * (amount > CYDUI_ANIM_ONE ? CYDUI_ANIM_ONE : amount) / CYDUI_ANIM_ONE);
        cydui_draw_rrect_outline(&ui->ctx, q, (uint8_t)(radius + i > 255 ? 255 : radius + i), 1, color, a);
    }
}

/* How pressed the widget is, 0 .. ONE: a press fades in over anim_ms and a release fades out. */
static int32_t pressed_amount(const cydui_paint_info *p)
{
    bool pressed = p->state == CYDUI_STATE_PRESSED;

    if (p->mix == 0 || p->mix >= CYDUI_ANIM_ONE) {
        return pressed ? CYDUI_ANIM_ONE : 0;
    }
    return pressed ? p->mix : CYDUI_ANIM_ONE - p->mix;
}

/* A control with a bright outline that glows when it is pressed, selected or focused. */
static void paint_neon(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    const cydui_wstyle w = cydui_paint_look(t, p);
    int32_t amount = pressed_amount(p), sel = cydui_paint_amount(p);

    cydui_paint_default(ui, t, p);
    switch (p->kind) {
    case CYDUI_WIDGET_BUTTON:
        break;
    case CYDUI_WIDGET_KEY: /* Shift and the current page glow too, but less than a key being pressed */
        amount = amount > sel / 2 ? amount : sel / 2;
        break;
    case CYDUI_WIDGET_TEXT_INPUT:
        amount = p->state == CYDUI_STATE_FOCUSED ? CYDUI_ANIM_ONE : 0;
        glow(ui, p->rect, w.radius, w.border, amount);
        return;
    case CYDUI_WIDGET_POPUP:
        amount = CYDUI_ANIM_ONE;
        break;
    case CYDUI_WIDGET_PANEL:
        amount = CYDUI_ANIM_ONE / 4;
        break;
    default: /* check box, switch, tab: the selected look */
        amount = sel;
        break;
    }
    glow(ui, p->rect, w.radius, p->kind == CYDUI_WIDGET_PANEL ? w.border : w.accent, amount);
}

/* The slider: the table look and a halo round the knob while it is held. */
static void paint_slider(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    cydui_rect knob;
    int cx = cydui_slider_knob_x(p->rect, p->value, p->min, p->max);

    cydui_paint_default(ui, t, p);
    knob.w = knob.h = CYDUI_SLIDER_KNOB;
    knob.x = (int16_t)(cx - CYDUI_SLIDER_KNOB / 2);
    knob.y = (int16_t)(p->rect.y + (p->rect.h - CYDUI_SLIDER_KNOB) / 2);
    glow(ui, knob, CYDUI_SLIDER_KNOB / 2, cydui_paint_look(t, p).accent, p->state == CYDUI_STATE_PRESSED ? CYDUI_ANIM_ONE : CYDUI_ANIM_ONE / 3);
}

/* A progress bar: a dark trough and a bar that goes from cyan to magenta along its length. */
static void paint_progress(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    const cydui_wstyle w = cydui_paint_look(t, p);
    cydui_rect bar = p->rect;
    int64_t v = p->value, m = p->max;

    cydui_draw_rrect(&ui->ctx, p->rect, w.radius, w.fill, 255);
    if (m <= 0 || v <= 0 || p->rect.w <= 0) {
        return;
    }
    bar.w = (int16_t)((v > m ? m : v) * p->rect.w / m);
    if (bar.w > 0) {
        cydui_draw_gradient(&ui->ctx, bar, w.accent, p->state == CYDUI_STATE_DISABLED ? w.accent : MAGENTA, false, 255);
    }
}

/* ---- the post-effect ---- */

#define BOOT_MS 360   /* the sweep that draws the screen when the theme comes in */
#define BOOT_BAND 12  /* rows of the bright line of the sweep */
#define GLITCH_PERIOD 2400
#define GLITCH_MS 120
#define GLITCH_BANDS 3

static uint32_t mix32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

/* A quarter off: each field of a 565 pixel loses a quarter of itself (no borrow between fields). */
static cydui_color dark(cydui_color c)
{
    return (cydui_color)(c - ((c >> 2) & 0x39E7u));
}

/* Where the sweep of the entrance is, in rows, or -1 when it is over (or the effects are off). */
static int sweep_row(const cydui_ui *ui)
{
    uint32_t age = cydui_theme_age(ui);

    if (cydui_effects_level(ui) < CYDUI_FX_FULL || !ui->in.has_clock || age >= BOOT_MS) {
        return -1;
    }
    return (int)((uint64_t)age * (uint64_t)(ui->ctx.backend->height + BOOT_BAND) / BOOT_MS);
}

typedef struct {
    int y, h, shift;
} band_t;

/* The bands of rows that jump sideways at this moment, or 0 when it is not the time of a glitch. */
static int glitch_bands(const cydui_ui *ui, band_t b[GLITCH_BANDS])
{
    uint32_t now = cydui_now(ui), epoch = now / GLITCH_PERIOD, phase = now % GLITCH_PERIOD, seed;
    int i, height = ui->ctx.backend->height;

    if (cydui_effects_level(ui) < CYDUI_FX_FULL || !ui->in.has_clock || epoch == 0 || phase >= GLITCH_MS ||
        height < 32) {
        return 0;
    }
    seed = mix32(epoch * 0x9E3779B9u + phase / 40u); /* it flickers: another set of bands every 40 ms */
    for (i = 0; i < GLITCH_BANDS; i++) {
        uint32_t h = mix32(seed + (uint32_t)i * 0x85EBCA6Bu);
        b[i].h = 3 + (int)(h % 9u);
        b[i].y = (int)((h >> 8) % (uint32_t)(height - b[i].h));
        b[i].shift = (int)((h >> 20) % 17u) - 8;
        if (b[i].shift == 0) {
            b[i].shift = 7;
        }
    }
    return GLITCH_BANDS;
}

static uint32_t fx_key(void *user, int16_t y0, int16_t h)
{
    const cydui_ui *ui = (const cydui_ui *)user;
    unsigned level = cydui_effects_level(ui);
    uint32_t key = level;
    int sweep = sweep_row(ui), n, i;
    band_t b[GLITCH_BANDS];

    if (level == CYDUI_FX_NONE) {
        return 0;
    }
    if (sweep >= 0) { /* above the line: as it will stay; below it: dark; on it: moving */
        if (y0 + h <= sweep - BOOT_BAND) {
            return key | 0x10u;
        }
        if (y0 >= sweep) {
            return key | 0x20u;
        }
        return key | 0x30u | ((uint32_t)sweep << 8);
    }
    n = glitch_bands(ui, b);
    for (i = 0; i < n; i++) {
        if (y0 < b[i].y + b[i].h && y0 + h > b[i].y) {
            key = mix32(key + (uint32_t)(b[i].y * 31 + b[i].h * 7 + b[i].shift + 1000) * 0x9E3779B9u);
        }
    }
    return key;
}

/* Rotates `n` pixels left by `s` (0 < s < n) in place: three reversals, no buffer. */
static void reverse(cydui_color *p, int n)
{
    int i;
    for (i = 0; i < n / 2; i++) {
        cydui_color t = p[i];
        p[i] = p[n - 1 - i];
        p[n - 1 - i] = t;
    }
}

static void rotate_left(cydui_color *p, int n, int s)
{
    s %= n;
    if (s < 0) {
        s += n;
    }
    if (s == 0) {
        return;
    }
    reverse(p, s);
    reverse(p + s, n - s);
    reverse(p, n);
}

static void fx_apply(void *user, cydui_color *buf, int16_t width, int16_t y0, int16_t h)
{
    const cydui_ui *ui = (const cydui_ui *)user;
    int sweep = sweep_row(ui), row, x, n, i;
    band_t b[GLITCH_BANDS];

    if (cydui_effects_level(ui) == CYDUI_FX_NONE) {
        return;
    }
    for (row = 0; row < h; row++) {
        cydui_color *px = buf + (size_t)row * (size_t)width;
        int y = y0 + row;

        if (sweep >= 0 && y >= sweep - BOOT_BAND) { /* the screen is not drawn there yet, and the line is */
            for (x = 0; x < width; x++) {
                px[x] = y < sweep ? cydui_blend(px[x], CYAN, (uint8_t)(255 - 255 * (sweep - 1 - y) / BOOT_BAND)) : INK;
            }
            continue;
        }
        if (y & 1) { /* scanlines */
            for (x = 0; x < width; x++) {
                px[x] = dark(px[x]);
            }
        }
    }
    n = glitch_bands(ui, b);
    for (i = 0; i < n; i++) {
        for (row = 0; row < h; row++) {
            int y = y0 + row;
            if (width > 1 && y >= b[i].y && y < b[i].y + b[i].h) {
                rotate_left(buf + (size_t)row * (size_t)width, width, b[i].shift);
            }
        }
    }
}

static const cydui_post_effect fx = {fx_key, fx_apply, NULL};

/* ---- the theme ---- */

void cydui_theme_init_cyberpunk(cydui_theme *t)
{
    cydui_style *s;
    int k, sel;

    memset(t, 0, sizeof(*t));
    t->name = "cyberpunk";
    s = &t->style;
    s->bg = BG;
    s->panel = PANEL;
    s->text = TEXT;
    s->text_muted = MUTED;
    s->accent = CYAN;
    s->accent_text = INK;
    s->border = VIOLET;
    s->font = &cydui_sans_16;
    s->radius = 4;
    s->pad_x = 14;
    s->pad_y = 8;
    s->spacing = 8;
    s->min_target = 32;
    s->anim_ms = 160;
    cydui_theme_derive_table(t);

    /* buttons: a dark tile with a cyan outline; pressed, it floods with the accent */
    entry(t, CYDUI_WIDGET_BUTTON, 0, CYDUI_STATE_NORMAL, TILE, CYAN, CYAN, CYAN, 2, 4);
    entry(t, CYDUI_WIDGET_BUTTON, 0, CYDUI_STATE_PRESSED, CYDUI_RGB(0, 70, 82), MAGENTA, TEXT, MAGENTA, 2, 4);
    entry(t, CYDUI_WIDGET_BUTTON, 0, CYDUI_STATE_DISABLED, PANEL, VIOLET, MUTED, VIOLET, 2, 4);

    /* check boxes: an outlined box that fills with cyan, the tick in ink */
    entry(t, CYDUI_WIDGET_CHECKBOX, 0, CYDUI_STATE_NORMAL, TILE, CYAN_DIM, INK, CYAN, 2, 3);
    entry(t, CYDUI_WIDGET_CHECKBOX, 0, CYDUI_STATE_PRESSED, CYDUI_RGB(0, 70, 82), CYAN, INK, CYAN, 2, 3);
    entry(t, CYDUI_WIDGET_CHECKBOX, 0, CYDUI_STATE_DISABLED, PANEL, VIOLET, INK, CYAN, 2, 3);
    entry(t, CYDUI_WIDGET_CHECKBOX, 1, CYDUI_STATE_NORMAL, CYAN, CYAN, INK, CYAN, 2, 3);
    entry(t, CYDUI_WIDGET_CHECKBOX, 1, CYDUI_STATE_PRESSED, CYDUI_RGB(0, 170, 190), CYAN, INK, CYAN, 2, 3);
    entry(t, CYDUI_WIDGET_CHECKBOX, 1, CYDUI_STATE_DISABLED, CYAN_DIM, VIOLET, INK, CYAN, 2, 3);

    /* switches: a dark pill with a dim outline; on, it lights up magenta with a bright knob */
    entry(t, CYDUI_WIDGET_TOGGLE, 0, CYDUI_STATE_NORMAL, TILE, CYAN_DIM, MUTED, CYAN, 2, 255);
    entry(t, CYDUI_WIDGET_TOGGLE, 0, CYDUI_STATE_PRESSED, TILE, CYAN, MUTED, CYAN, 2, 255);
    entry(t, CYDUI_WIDGET_TOGGLE, 0, CYDUI_STATE_DISABLED, PANEL, VIOLET, VIOLET, CYAN, 2, 255);
    entry(t, CYDUI_WIDGET_TOGGLE, 1, CYDUI_STATE_NORMAL, MAGENTA, MAGENTA, TEXT, MAGENTA, 2, 255);
    entry(t, CYDUI_WIDGET_TOGGLE, 1, CYDUI_STATE_PRESSED, CYDUI_RGB(200, 20, 120), MAGENTA, TEXT, MAGENTA, 2, 255);
    entry(t, CYDUI_WIDGET_TOGGLE, 1, CYDUI_STATE_DISABLED, VIOLET, VIOLET, MUTED, MAGENTA, 2, 255);

    /* sliders: a thin dark track, the value in cyan, a magenta knob */
    entry(t, CYDUI_WIDGET_SLIDER, 0, CYDUI_STATE_NORMAL, VIOLET, MAGENTA, TEXT, CYAN, 2, 255);
    entry(t, CYDUI_WIDGET_SLIDER, 0, CYDUI_STATE_PRESSED, VIOLET, CYAN, TEXT, CYAN, 2, 255);
    entry(t, CYDUI_WIDGET_SLIDER, 0, CYDUI_STATE_DISABLED, PANEL, VIOLET, MUTED, VIOLET, 2, 255);

    /* progress: a dark trough, a bar from cyan to magenta (the painter) */
    entry(t, CYDUI_WIDGET_PROGRESS, 0, CYDUI_STATE_NORMAL, TILE, VIOLET, TEXT, CYAN, 0, 255);
    entry(t, CYDUI_WIDGET_PROGRESS, 0, CYDUI_STATE_DISABLED, PANEL, VIOLET, MUTED, VIOLET, 0, 255);

    /* tabs: plain and dim, the active one is cyan and lit */
    entry(t, CYDUI_WIDGET_TAB, 0, CYDUI_STATE_NORMAL, BG, VIOLET, MUTED, CYAN, 0, 4);
    entry(t, CYDUI_WIDGET_TAB, 0, CYDUI_STATE_PRESSED, TILE, VIOLET, TEXT, CYAN, 0, 4);
    entry(t, CYDUI_WIDGET_TAB, 0, CYDUI_STATE_DISABLED, BG, VIOLET, VIOLET, CYAN, 0, 4);
    entry(t, CYDUI_WIDGET_TAB, 1, CYDUI_STATE_NORMAL, TILE, VIOLET, CYAN, MAGENTA, 0, 4);
    entry(t, CYDUI_WIDGET_TAB, 1, CYDUI_STATE_PRESSED, PANEL, VIOLET, CYAN, MAGENTA, 0, 4);
    entry(t, CYDUI_WIDGET_TAB, 1, CYDUI_STATE_DISABLED, TILE, VIOLET, VIOLET, VIOLET, 0, 4);

    /* cards and popups: a dark slab with a thin violet outline; the popup also carries a halo */
    entry(t, CYDUI_WIDGET_PANEL, 0, CYDUI_STATE_NORMAL, PANEL, MAGENTA, TEXT, CYAN, 1, 6);
    entry(t, CYDUI_WIDGET_PANEL, 1, CYDUI_STATE_NORMAL, PANEL, CYAN, TEXT, CYAN, 1, 6);
    entry(t, CYDUI_WIDGET_POPUP, 0, CYDUI_STATE_NORMAL, PANEL, CYAN, TEXT, CYAN, 2, 6);

    /* the thumb of a scroll bar */
    entry(t, CYDUI_WIDGET_SCROLLBAR, 0, CYDUI_STATE_NORMAL, BG, VIOLET, TEXT, MAGENTA, 0, 255);
    entry(t, CYDUI_WIDGET_SCROLLBAR, 0, CYDUI_STATE_PRESSED, BG, VIOLET, TEXT, CYAN, 0, 255);

    /* the keys of the keyboard on the screen */
    entry(t, CYDUI_WIDGET_KEY, 0, CYDUI_STATE_NORMAL, TILE, VIOLET, TEXT, CYAN, 1, 4);
    entry(t, CYDUI_WIDGET_KEY, 0, CYDUI_STATE_PRESSED, CYDUI_RGB(0, 70, 82), CYAN, TEXT, CYAN, 1, 4);
    entry(t, CYDUI_WIDGET_KEY, 1, CYDUI_STATE_NORMAL, CYDUI_RGB(0, 70, 82), CYAN, CYAN, CYAN, 1, 4);
    entry(t, CYDUI_WIDGET_KEY, 1, CYDUI_STATE_PRESSED, CYDUI_RGB(0, 110, 126), CYAN, TEXT, CYAN, 1, 4);

    /* a field: dark, with a dim outline; the focus lights it, a refused key turns it magenta */
    entry(t, CYDUI_WIDGET_TEXT_INPUT, 0, CYDUI_STATE_NORMAL, BG, CYAN_DIM, TEXT, CYAN, 1, 4);
    entry(t, CYDUI_WIDGET_TEXT_INPUT, 0, CYDUI_STATE_DISABLED, PANEL, VIOLET, MUTED, CYAN, 1, 4);
    entry(t, CYDUI_WIDGET_TEXT_INPUT, 1, CYDUI_STATE_NORMAL, BG, MAGENTA, TEXT, CYAN, 2, 4);

    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) {
        for (sel = 0; sel < 2; sel++) {
            t->table[k][sel][CYDUI_STATE_HOVER] = t->table[k][sel][CYDUI_STATE_NORMAL];
            t->table[k][sel][CYDUI_STATE_FOCUSED] = t->table[k][sel][CYDUI_STATE_NORMAL];
        }
    }
    entry(t, CYDUI_WIDGET_TEXT_INPUT, 0, CYDUI_STATE_FOCUSED, BG, CYAN, TEXT, CYAN, 2, 4);
    entry(t, CYDUI_WIDGET_TEXT_INPUT, 1, CYDUI_STATE_FOCUSED, BG, MAGENTA, TEXT, CYAN, 2, 4);
    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) { /* kinds with one look: the selected one is the same */
        if (k == CYDUI_WIDGET_CHECKBOX || k == CYDUI_WIDGET_TOGGLE || k == CYDUI_WIDGET_TAB ||
            k == CYDUI_WIDGET_PANEL || k == CYDUI_WIDGET_TEXT_INPUT || k == CYDUI_WIDGET_POPUP ||
            k == CYDUI_WIDGET_KEY) {
            continue;
        }
        for (sel = 0; sel < CYDUI_STATE_COUNT; sel++) {
            t->table[k][1][sel] = t->table[k][0][sel];
        }
    }

    t->paint[CYDUI_WIDGET_BUTTON] = paint_neon;
    t->paint[CYDUI_WIDGET_KEY] = paint_neon;
    t->paint[CYDUI_WIDGET_TAB] = paint_neon;
    t->paint[CYDUI_WIDGET_CHECKBOX] = paint_neon;
    t->paint[CYDUI_WIDGET_TOGGLE] = paint_neon;
    t->paint[CYDUI_WIDGET_PANEL] = paint_neon;
    t->paint[CYDUI_WIDGET_POPUP] = paint_neon;
    t->paint[CYDUI_WIDGET_TEXT_INPUT] = paint_neon;
    t->paint[CYDUI_WIDGET_SLIDER] = paint_slider;
    t->paint[CYDUI_WIDGET_PROGRESS] = paint_progress;
    t->post = &fx;
}
