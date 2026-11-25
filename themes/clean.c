/*
 * The clean themes: one language (soft corners, flat controls, a single accent, white or slate cards)
 * in a light and a dark variant. Both are the same function over a palette, so a variant costs a
 * few colors and nothing else (NFR-43).
 */

#include <string.h>

#include "cydui_sans_16.h"
#include "cydui_themes.h"

typedef struct {
    const char *name;
    cydui_color bg, panel, text, muted, accent, accent_text, border;
    cydui_color disabled;  /* the fill of something that does not react */
    cydui_color knob;      /* the knob of a slider */
    cydui_color thumb;     /* the thumb of a scroll bar */
    uint8_t card_shadow;   /* how far the shadow of a card spreads: 0 on dark, where it would not show */
} palette_t;

static cydui_color mix(cydui_color under, cydui_color over, uint8_t alpha)
{
    return cydui_blend(under, over, alpha);
}

static void entry(cydui_theme *t, cydui_widget_kind k, int sel, cydui_widget_state st, cydui_color fill,
                  cydui_color border, cydui_color text, cydui_color accent, uint8_t bw, uint8_t radius,
                  uint8_t shadow)
{
    cydui_wstyle *w = &t->table[k][sel][st];
    w->fill = fill;
    w->border = border;
    w->text = text;
    w->accent = accent;
    w->border_w = bw;
    w->radius = radius;
    w->shadow = shadow;
}

static void init(cydui_theme *t, const palette_t *p)
{
    cydui_style *s;
    cydui_color accent_dark, accent_dim;
    int k, sel, st;

    memset(t, 0, sizeof(*t));
    t->name = p->name;
    s = &t->style;
    s->bg = p->bg;
    s->panel = p->panel;
    s->text = p->text;
    s->text_muted = p->muted;
    s->accent = p->accent;
    s->accent_text = p->accent_text;
    s->border = p->border;
    s->font = &cydui_sans_16;
    s->radius = 8;
    s->pad_x = 14;
    s->pad_y = 8;
    s->spacing = 8;
    s->min_target = 32;
    cydui_theme_derive_table(t); /* a sane starting point for every widget and state */

    accent_dark = mix(p->accent, 0, 80);
    accent_dim = mix(p->bg, p->accent, 110);

    /* buttons: flat, accent colored */
    entry(t, CYDUI_WIDGET_BUTTON, 0, CYDUI_STATE_NORMAL, p->accent, p->border, p->accent_text, p->accent, 0, 8, 0);
    entry(t, CYDUI_WIDGET_BUTTON, 0, CYDUI_STATE_PRESSED, accent_dark, p->border, p->accent_text, p->accent, 0, 8, 0);
    entry(t, CYDUI_WIDGET_BUTTON, 0, CYDUI_STATE_DISABLED, p->disabled, p->border, p->muted, p->accent, 0, 8, 0);

    /* check boxes: an outlined box that fills with the accent */
    entry(t, CYDUI_WIDGET_CHECKBOX, 0, CYDUI_STATE_NORMAL, p->panel, p->border, p->accent_text, p->accent, 2, 5, 0);
    entry(t, CYDUI_WIDGET_CHECKBOX, 0, CYDUI_STATE_PRESSED, mix(p->panel, p->border, 90), p->border, p->accent_text, p->accent, 2, 5, 0);
    entry(t, CYDUI_WIDGET_CHECKBOX, 0, CYDUI_STATE_DISABLED, p->disabled, mix(p->bg, p->border, 150), p->accent_text, p->accent, 2, 5, 0);
    entry(t, CYDUI_WIDGET_CHECKBOX, 1, CYDUI_STATE_NORMAL, p->accent, p->border, p->accent_text, p->accent, 0, 5, 0);
    entry(t, CYDUI_WIDGET_CHECKBOX, 1, CYDUI_STATE_PRESSED, accent_dark, p->border, p->accent_text, p->accent, 0, 5, 0);
    entry(t, CYDUI_WIDGET_CHECKBOX, 1, CYDUI_STATE_DISABLED, accent_dim, p->border, p->accent_text, p->accent, 0, 5, 0);

    /* switches: a pill that turns accent colored, a white knob */
    entry(t, CYDUI_WIDGET_TOGGLE, 0, CYDUI_STATE_NORMAL, p->border, p->border, p->accent_text, p->accent, 0, 255, 0);
    entry(t, CYDUI_WIDGET_TOGGLE, 0, CYDUI_STATE_PRESSED, mix(p->border, 0, 70), p->border, p->accent_text, p->accent, 0, 255, 0);
    entry(t, CYDUI_WIDGET_TOGGLE, 0, CYDUI_STATE_DISABLED, p->disabled, p->border, mix(p->bg, p->accent_text, 120), p->accent, 0, 255, 0);
    entry(t, CYDUI_WIDGET_TOGGLE, 1, CYDUI_STATE_NORMAL, p->accent, p->border, p->accent_text, p->accent, 0, 255, 0);
    entry(t, CYDUI_WIDGET_TOGGLE, 1, CYDUI_STATE_PRESSED, accent_dark, p->border, p->accent_text, p->accent, 0, 255, 0);
    entry(t, CYDUI_WIDGET_TOGGLE, 1, CYDUI_STATE_DISABLED, accent_dim, p->border, mix(p->bg, p->accent_text, 120), p->accent, 0, 255, 0);

    /* sliders: a thin pill track, the part up to the knob in the accent, a ringed knob */
    entry(t, CYDUI_WIDGET_SLIDER, 0, CYDUI_STATE_NORMAL, p->border, p->accent, p->knob, p->accent, 2, 255, 0);
    entry(t, CYDUI_WIDGET_SLIDER, 0, CYDUI_STATE_PRESSED, p->border, accent_dark, p->knob, p->accent, 2, 255, 0);
    entry(t, CYDUI_WIDGET_SLIDER, 0, CYDUI_STATE_DISABLED, p->disabled, accent_dim, mix(p->bg, p->knob, 150), accent_dim, 2, 255, 0);

    /* progress: the same pill, no knob */
    entry(t, CYDUI_WIDGET_PROGRESS, 0, CYDUI_STATE_NORMAL, p->disabled, p->border, p->text, p->accent, 0, 255, 0);
    entry(t, CYDUI_WIDGET_PROGRESS, 0, CYDUI_STATE_DISABLED, p->disabled, p->border, p->muted, accent_dim, 0, 255, 0);

    /* tabs: the active one is a card with an accent underline, the others are plain */
    entry(t, CYDUI_WIDGET_TAB, 0, CYDUI_STATE_NORMAL, p->bg, p->border, p->muted, p->accent, 0, 8, 0);
    entry(t, CYDUI_WIDGET_TAB, 0, CYDUI_STATE_PRESSED, mix(p->bg, p->border, 80), p->border, p->text, p->accent, 0, 8, 0);
    entry(t, CYDUI_WIDGET_TAB, 0, CYDUI_STATE_DISABLED, p->bg, p->border, mix(p->bg, p->muted, 110), p->accent, 0, 8, 0);
    entry(t, CYDUI_WIDGET_TAB, 1, CYDUI_STATE_NORMAL, p->panel, p->border, p->accent, p->accent, 0, 8, 0);
    entry(t, CYDUI_WIDGET_TAB, 1, CYDUI_STATE_PRESSED, mix(p->panel, p->border, 60), p->border, p->accent, p->accent, 0, 8, 0);
    entry(t, CYDUI_WIDGET_TAB, 1, CYDUI_STATE_DISABLED, p->panel, p->border, mix(p->bg, p->accent, 110), accent_dim, 0, 8, 0);

    /* cards */
    entry(t, CYDUI_WIDGET_PANEL, 0, CYDUI_STATE_NORMAL, p->panel, mix(p->bg, p->border, 130), p->text, p->accent, 1, 12, p->card_shadow);
    entry(t, CYDUI_WIDGET_PANEL, 1, CYDUI_STATE_NORMAL, p->panel, p->accent, p->text, p->accent, 1, 12, p->card_shadow);

    /* the scroll bar: a thumb only */
    entry(t, CYDUI_WIDGET_SCROLLBAR, 0, CYDUI_STATE_NORMAL, p->bg, p->border, p->text, p->thumb, 0, 255, 0);
    entry(t, CYDUI_WIDGET_SCROLLBAR, 0, CYDUI_STATE_PRESSED, p->bg, p->border, p->text, mix(p->thumb, p->accent, 120), 0, 255, 0);

    /* disabled looks do not depend on selected/pressed for the kinds that only have one look */
    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) {
        for (sel = 0; sel < 2; sel++) {
            t->table[k][sel][CYDUI_STATE_HOVER] = t->table[k][sel][CYDUI_STATE_NORMAL];
            t->table[k][sel][CYDUI_STATE_FOCUSED] = t->table[k][sel][CYDUI_STATE_NORMAL];
        }
    }
    /* what was not given for a selected button/slider/progress/scroll bar is its unselected look */
    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) {
        if (k == CYDUI_WIDGET_CHECKBOX || k == CYDUI_WIDGET_TOGGLE || k == CYDUI_WIDGET_TAB || k == CYDUI_WIDGET_PANEL) {
            continue;
        }
        for (st = 0; st < CYDUI_STATE_COUNT; st++) {
            t->table[k][1][st] = t->table[k][0][st];
        }
    }
}

void cydui_theme_init_clean(cydui_theme *t)
{
    static const palette_t light = {"clean",
                                    CYDUI_RGB(243, 245, 249), CYDUI_RGB(255, 255, 255), CYDUI_RGB(24, 31, 46),
                                    CYDUI_RGB(125, 134, 152), CYDUI_RGB(37, 99, 235), CYDUI_RGB(255, 255, 255),
                                    CYDUI_RGB(200, 207, 221), CYDUI_RGB(228, 232, 241), CYDUI_RGB(255, 255, 255),
                                    CYDUI_RGB(176, 185, 204), 5};
    init(t, &light);
}

void cydui_theme_init_clean_dark(cydui_theme *t)
{
    static const palette_t dark = {"clean_dark",
                                   CYDUI_RGB(15, 19, 28), CYDUI_RGB(27, 33, 48), CYDUI_RGB(230, 235, 245),
                                   CYDUI_RGB(120, 130, 152), CYDUI_RGB(74, 128, 246), CYDUI_RGB(255, 255, 255),
                                   CYDUI_RGB(58, 67, 92), CYDUI_RGB(36, 43, 60), CYDUI_RGB(235, 240, 250),
                                   CYDUI_RGB(84, 96, 128), 0};
    init(t, &dark);
}
