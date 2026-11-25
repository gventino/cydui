/*
 * The single line text field. The text is drawn from the first visible character on, so a long text never
 * depends on the limit of what one text command may hold, and the cursor and the taps are worked out from the
 * advance of every character (whole pixels, no kerning: the width of a text is the sum of its characters).
 */

#include <string.h>

#include "cydui/textedit.h"
#include "cydui/ui.h"
#include "cydui/utf8.h"
#include "ui_internal.h"

#define FIELD_PAD 8       /* between the outline and the text */
#define CURSOR_W 2
#define ATTENTION_MS 400  /* how long a refused key outlines the field */
#define ATTENTION_FRAMES 8 /* ... with no clock, in frames */
#define BULLET "\xE2\x80\xA2" /* U+2022 */
#define BULLET_CHUNK 32    /* bullets drawn by one text command */

typedef struct {
    const cydui_font *font;
    bool password;
    int bullet_w;
} measure;

/* How far the pen moves for the character at `s`: the advance of its glyph, as the drawing does (the width of a text
 * does not count the spaces at its end, so it cannot be used for one character). */
static int advance(const measure *m, const char *s, size_t n)
{
    uint32_t cp;
    const cydui_glyph *g;

    if (m->password) {
        return m->bullet_w;
    }
    (void)cydui_utf8_decode(s, n, &cp);
    g = cydui_text_resolve(m->font, cp);
    return g ? g->advance : 0;
}

/* The x (from the start of the text) of the character boundary `at`, and of the end of the text. */
static int x_of(const measure *m, const char *buf, size_t len, size_t at, int *total)
{
    size_t i = 0;
    int x = 0, at_x = 0;
    uint32_t cp;

    for (;;) {
        if (i == at) {
            at_x = x;
        }
        if (i >= len) {
            break;
        }
        {
            size_t step = cydui_utf8_decode(buf + i, len - i, &cp);
            x += advance(m, buf + i, step);
            i += step;
        }
    }
    if (total) {
        *total = x;
    }
    return at_x;
}

/* The boundary nearest to `x` (from the start of the text). */
static size_t at_of(const measure *m, const char *buf, size_t len, int x)
{
    size_t i = 0;
    int px = 0;
    uint32_t cp;

    while (i < len) {
        size_t step = cydui_utf8_decode(buf + i, len - i, &cp);
        int w = advance(m, buf + i, step);
        if (2 * (x - px) < w) { /* nearer to this boundary than to the next */
            break;
        }
        px += w;
        i += step;
    }
    return i;
}

static void draw_bullets(cydui_ui *ui, cydui_rect box, int x, int y, size_t count, cydui_color color, int bullet_w)
{
    char s[BULLET_CHUNK * 3 + 1];
    size_t i;

    for (i = 0; i < BULLET_CHUNK; i++) {
        memcpy(s + i * 3, BULLET, 3);
    }
    while (count > 0 && x < box.x + box.w) {
        size_t n = count > BULLET_CHUNK ? BULLET_CHUNK : count;
        cydui_draw_text_in(ui, box, x, y, s, n * 3, color);
        x += (int)n * bullet_w;
        count -= n;
    }
}

unsigned cydui_text_input(cydui_ui *ui, const char *label, char *buf, size_t cap, size_t max_chars, unsigned flags)
{
    cydui_mods m = cydui_take_mods(ui);
    const cydui_style *st = &ui->theme->style;
    cydui_input *in = &ui->in;
    cydui_state own, *s;
    cydui_id id;
    cydui_response resp;
    cydui_rect r, inner;
    cydui_paint_info pi;
    cydui_text_extent line;
    const cydui_wstyle *w;
    measure ms;
    size_t hint_n, len, cur, i, first = 0, start = 0;
    int h, total, cursor_x, scroll, x0 = 0, ty;
    bool disabled, focused, attention = false, full = false;
    unsigned result = 0;

    if (!cydui_widget_begin(ui)) {
        return 0;
    }
    label = label ? label : "";
    id = cydui_id_str(ui, label);
    cydui_id_claim(ui, id);
    hint_n = cydui_label_len(label);
    ms.font = st->font;
    ms.password = (flags & CYDUI_TEXT_PASSWORD) != 0;
    ms.bullet_w = advance(&(measure){st->font, false, 0}, BULLET, 3);
    line = cydui_text_size(st->font, "A", 1, 0);
    h = line.h + 2 * st->pad_y;
    if (h < st->min_target) {
        h = st->min_target;
    }
    r = cydui_alloc(ui, &m, cydui_layout_avail(ui).w, h);
    inner.x = (int16_t)(r.x + FIELD_PAD);
    inner.y = r.y;
    inner.w = (int16_t)(r.w > 2 * FIELD_PAD ? r.w - 2 * FIELD_PAD : 0);
    inner.h = r.h;

    disabled = m.disabled || !buf || cap == 0;
    s = cydui_state_get(ui, id, true);
    if (!s) { /* no room in the state table: the field still works, but forgets its cursor between frames */
        memset(&own, 0, sizeof(own));
        s = &own;
    }
    cur = s->a < 0 ? 0 : (size_t)s->a;
    scroll = s->b < 0 ? 0 : s->b;
    if (buf && cap > 0) { /* a disabled field still shows its text */
        cydui_textedit_normalize(buf, cap, &cur);
        len = cydui_textedit_len(buf, cap);
    } else {
        cur = 0;
        len = 0;
    }

    resp = cydui_interact(ui, id, r, CYDUI_SENSE_CLICK | (disabled ? CYDUI_DISABLED : 0u));
    if (resp.pressed) {
        ui->focus_claimed = true; /* a press on a field keeps the focus where it is going */
    }
    if (resp.clicked) {
        ui->focus = id;
        cur = at_of(&ms, buf, len, resp.x - inner.x + scroll);
    }
    focused = !disabled && cydui_id_eq(ui->focus, id);
    if (focused) {
        ui->focus_seen = true;
        for (i = 0; i < in->k_len; i++) {
            const cydui_event *ev = &in->keys[i];
            unsigned res = 0;
            bool stop = false;

            if (ev->type == CYDUI_EVENT_CHAR) {
                res = cydui_textedit_insert(buf, cap, &cur, ev->key, 0, max_chars);
            } else {
                switch ((cydui_key)ev->key) {
                case CYDUI_KEY_BACKSPACE: res = cydui_textedit_backspace(buf, cap, &cur); break;
                case CYDUI_KEY_DELETE: res = cydui_textedit_delete(buf, cap, &cur); break;
                case CYDUI_KEY_LEFT: res = cydui_textedit_move(buf, cap, &cur, CYDUI_MOVE_LEFT, 0); break;
                case CYDUI_KEY_RIGHT: res = cydui_textedit_move(buf, cap, &cur, CYDUI_MOVE_RIGHT, 0); break;
                case CYDUI_KEY_HOME: res = cydui_textedit_move(buf, cap, &cur, CYDUI_MOVE_HOME, 0); break;
                case CYDUI_KEY_END: res = cydui_textedit_move(buf, cap, &cur, CYDUI_MOVE_END, 0); break;
                case CYDUI_KEY_ENTER:
                    result |= CYDUI_TEXT_CONFIRMED;
                    stop = true;
                    break;
                case CYDUI_KEY_ESCAPE:
                    stop = true;
                    break;
                default: /* up and down are for the text area */
                    break;
                }
            }
            if (res & CYDUI_EDIT_TEXT) {
                result |= CYDUI_TEXT_CHANGED;
            }
            if (res & CYDUI_EDIT_FULL) {
                full = true;
            }
            if (stop) {
                memset(&ui->focus, 0, sizeof(ui->focus));
                focused = false;
                break;
            }
        }
        in->k_len = 0;
        len = cydui_textedit_len(buf, cap);
    }

    /* the refusal of a key outlines the field for a moment: s->c is "active", s->d the deadline (ms or frames) */
    if (full) {
        s->c = 1;
        s->d = in->has_clock ? (int32_t)(in->now + ATTENTION_MS) : ATTENTION_FRAMES;
    }
    if (s->c) {
        if (in->has_clock) {
            attention = (int32_t)((uint32_t)s->d - in->now) > 0;
        } else {
            attention = s->d > 0;
            s->d -= attention ? 1 : 0;
        }
        if (attention) {
            ui->relayout = true; /* another frame, to see it end */
        } else {
            s->c = 0;
        }
    }

    /* where the cursor is, and what part of the text shows */
    cursor_x = x_of(&ms, buf, len, cur, &total);
    if (cursor_x < scroll) {
        scroll = cursor_x;
    }
    if (cursor_x + CURSOR_W > scroll + inner.w) {
        scroll = cursor_x + CURSOR_W - inner.w;
    }
    if (total + CURSOR_W <= inner.w || scroll < 0) {
        scroll = 0;
    }
    s->a = (int32_t)cur;
    s->b = scroll;
    {
        size_t k = 0;
        int px = 0;
        uint32_t cp;

        while (k < len) { /* the first character that is not all behind the left edge */
            size_t step = cydui_utf8_decode(buf + k, len - k, &cp);
            int cw = advance(&ms, buf + k, step);
            if (px + cw > scroll) {
                break;
            }
            px += cw;
            k += step;
            first++;
        }
        x0 = px;
        start = k; /* the byte where the drawing starts */
    }

    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_TEXT_INPUT;
    pi.state = disabled ? CYDUI_STATE_DISABLED : (focused ? CYDUI_STATE_FOCUSED : CYDUI_STATE_NORMAL);
    pi.selected = attention;
    pi.rect = r;
    cydui_paint(ui, &pi);
    w = cydui_wstyle_of(ui, CYDUI_WIDGET_TEXT_INPUT, pi.state, attention);

    ty = r.y + (r.h - line.h) / 2;
    if (len == 0) {
        if (hint_n) {
            cydui_draw_text_in(ui, inner, inner.x, ty, label, hint_n, st->text_muted);
        }
    } else if (ms.password) {
        size_t chars = cydui_textedit_chars(buf, cap);
        draw_bullets(ui, inner, inner.x + x0 - scroll, ty, chars - first, w->text, ms.bullet_w);
    } else {
        size_t n = len - start;
        cydui_draw_text_in(ui, inner, inner.x + x0 - scroll, ty, buf + start, n > CYDUI_MAX_STR ? CYDUI_MAX_STR : n, w->text);
    }
    if (focused) {
        cydui_draw_rect(&ui->ctx, (cydui_rect){(int16_t)(inner.x + cursor_x - scroll), (int16_t)(r.y + 6), CURSOR_W,
                                               (int16_t)(r.h > 12 ? r.h - 12 : r.h)},
                        w->accent, 255);
    }
    return result;
}
