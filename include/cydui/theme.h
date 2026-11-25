#ifndef CYDUI_THEME_H
#define CYDUI_THEME_H

/*
 * Themes. A theme owns everything visual: a palette and metrics (cydui_style), a table of how each
 * widget looks in each state, and optionally its own painting functions. Widgets measure, place and
 * react the same way in every theme; only what they look like changes. A new look is usually just a
 * different table (NFR-43); a painter is for the effects a table cannot express.
 */

#include "cydui/anim.h"
#include "cydui/font.h"
#include "cydui/text.h"
#include "cydui/types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cydui_ui;

/** Palette and metrics shared by all widgets of a theme. */
typedef struct {
    cydui_color bg, panel, text, text_muted, accent, accent_text, border;
    const cydui_font *font;
    uint8_t radius;
    uint8_t pad_x, pad_y;
    uint8_t spacing;
    uint8_t min_target; /**< smallest touch target in pixels (R-04) */
    uint16_t anim_ms;   /**< how long a widget takes to change its look (FR-53); 0 = at once, which is also what a backend without a clock gets */
} cydui_style;

/** A dark style around `font`. */
cydui_style cydui_style_default(const cydui_font *font);

typedef enum {
    CYDUI_WIDGET_BUTTON,
    CYDUI_WIDGET_CHECKBOX,
    CYDUI_WIDGET_TOGGLE,
    CYDUI_WIDGET_SLIDER,
    CYDUI_WIDGET_PROGRESS,
    CYDUI_WIDGET_TAB,
    CYDUI_WIDGET_PANEL,
    CYDUI_WIDGET_SCROLLBAR,
    CYDUI_WIDGET_TEXT_INPUT, /**< the field: `fill` is its body, `border` its outline, `text` the text, `accent` the cursor;
                                  FOCUSED while it has the focus; selected = the last key was refused (a limit) */
    CYDUI_WIDGET_POPUP,      /**< the panel of a popup (the dimming behind it is drawn by the library); also the
                                  background of the keyboard on the screen */
    CYDUI_WIDGET_KEY,        /**< a key of the keyboard on the screen: `fill` the key, `text` its label; selected = a
                                  toggle that is on (Shift) or the page that is shown */
    CYDUI_WIDGET_COUNT
} cydui_widget_kind;

/** The library produces NORMAL, PRESSED, DISABLED and (for text fields) FOCUSED; HOVER has room in the tables. */
typedef enum {
    CYDUI_STATE_NORMAL,
    CYDUI_STATE_HOVER,
    CYDUI_STATE_PRESSED,
    CYDUI_STATE_DISABLED,
    CYDUI_STATE_FOCUSED,
    CYDUI_STATE_COUNT
} cydui_widget_state;

/**
 * How one widget looks in one state. What the colors mean depends on the widget: `fill` is the body
 * (a button, a box, a switch track, a slider track), `border` its outline, `text` the label or the
 * mark drawn on it (the check, the switch knob), `accent` the part that shows the value (the filled
 * part of a slider or progress bar, the thumb of a scroll bar). A radius larger than half the
 * smaller side makes a pill; `shadow` is the strength of the drop shadow, 0 for none.
 */
typedef struct {
    cydui_color fill, border, text, accent;
    uint8_t border_w, radius, shadow;
} cydui_wstyle;

/** Everything a painter needs to know about the widget it draws. */
typedef struct {
    cydui_widget_kind kind;
    cydui_widget_state state;
    bool selected;            /**< checked, on, the active tab */
    cydui_rect rect;          /**< the whole widget */
    const char *text;         /**< label, if the widget has one (not NUL terminated) */
    size_t text_len;
    cydui_text_extent text_extent; /**< its size in the theme's font */
    int32_t value, min, max;  /**< slider and progress value; scroll bar: offset, 0, content - view */
    cydui_rect aux;           /**< widget specific: the scroll bar's viewport */
    uint16_t mix;             /**< 0: settled, draw `state` and `selected`. Else the widget is changing its look: how far it
                                   got from the look of `from_state`/`from_selected` to this one, 1..CYDUI_ANIM_ONE - 1 */
    cydui_widget_state from_state;
    bool from_selected;
} cydui_paint_info;


/** Sizes a slider is built from, so that the widget and every painter agree on where the knob is. */
#define CYDUI_SLIDER_KNOB 20  /**< diameter of the knob */
#define CYDUI_SLIDER_TRACK 6  /**< thickness of the track */

/**
 * The x of the center of the knob for `value` in a slider whose track area is `track`. The knob
 * travels between half a knob in from each end; `value` is clamped to [min, max]. Integer only,
 * safe for the whole int32 range.
 */
int cydui_slider_knob_x(cydui_rect track, int32_t value, int32_t min, int32_t max);

/** The value for a pointer at `x`: the inverse of cydui_slider_knob_x, rounded, clamped. */
int32_t cydui_slider_value_at(cydui_rect track, int x, int32_t min, int32_t max);

typedef struct cydui_theme cydui_theme;

/**
 * A painter draws one widget. It may call cydui_paint_default() for the table driven look and add
 * to it, or draw everything itself with the cydui_draw_* functions of ui->ctx.
 */
typedef void (*cydui_paint_fn)(struct cydui_ui *ui, const cydui_theme *theme,
                               const cydui_paint_info *info);

struct cydui_theme {
    const char *name;
    cydui_style style;
    cydui_wstyle table[CYDUI_WIDGET_COUNT][2][CYDUI_STATE_COUNT]; /**< [widget][selected][state] */
    cydui_paint_fn paint[CYDUI_WIDGET_COUNT];                     /**< NULL: the table driven painter */
};

typedef enum {
    CYDUI_THEME_OK = 0,
    CYDUI_THEME_ERR_NULL, /**< no theme */
    CYDUI_THEME_ERR_FONT  /**< the font is missing or unusable */
} cydui_theme_status;

/**
 * Checks what a theme must have to be used (CT-09): the font. Everything else is safe with any
 * value: painters tolerate every table entry, and a NULL painter means the default.
 */
cydui_theme_status cydui_theme_validate(const cydui_theme *t);

/** Fills `t` with the built-in dark theme around `font`. */
void cydui_theme_default(cydui_theme *t, const cydui_font *font);

/**
 * Builds the table of `t` from its palette, with the same rules as the built-in theme: a pressed
 * widget is its normal color darkened, a disabled one dimmed towards the background. A starting
 * point for a theme that only changes the palette.
 */
void cydui_theme_derive_table(cydui_theme *t);

/** Draws a widget with the table (the default painter). Safe for any `info`. */
void cydui_paint_default(struct cydui_ui *ui, const cydui_theme *theme, const cydui_paint_info *info);

/**
 * The look of the widget in `info` from the table: while it changes (`mix`) the colors are halfway between
 * the two ends and the numbers are interpolated. A painter of a theme should draw with this, not with the
 * table directly, to take part in the transitions.
 */
cydui_wstyle cydui_paint_look(const cydui_theme *theme, const cydui_paint_info *info);

/**
 * How selected the widget is right now, 0 .. CYDUI_ANIM_ONE: a check box, switch or tab that is changing
 * is in between (what a switch needs to place its knob).
 */
int32_t cydui_paint_amount(const cydui_paint_info *info);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_THEME_H */
