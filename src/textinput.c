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

/* The refusal of a key outlines the field for a moment: s->c is "active", s->d the deadline (ms, or frames with no
 * clock). Returns whether the outline is on in this frame, and asks for another frame while it is. */
static bool attention_step(cydui_ui *ui, cydui_state *s, bool full)
{
    const cydui_input *in = &ui->in;
    bool on = false;

    if (full) {
        s->c = 1;
        s->d = in->has_clock ? (int32_t)(in->now + ATTENTION_MS) : ATTENTION_FRAMES;
    }
    if (s->c) {
        if (in->has_clock) {
            on = (int32_t)((uint32_t)s->d - in->now) > 0;
        } else {
            on = s->d > 0;
            s->d -= on ? 1 : 0;
        }
        if (on) {
            ui->relayout = true; /* another frame, to see it end */
        } else {
            s->c = 0;
        }
    }
    return on;
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
    bool disabled, focused, attention, full = false;
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

    attention = attention_step(ui, s, full);

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
    if (focused && !ui->kb_off && (resp.clicked || (result & CYDUI_TEXT_CHANGED))) {
        cydui__scroll_reveal(ui, r, cydui__keyboard_top(ui) - 4); /* not under the keyboard that is coming up */
    }
    return result;
}

/* ---- the text area ---- */

#define AREA_PAD_Y 6
#define AREA_MAX_ROWS 16

/* One line as it is shown. [start, next) is what belongs to it for the cursor: the bytes up to the start of the next
 * line (the newline, or the spaces a wrap skipped, are the end of this one). */
typedef struct {
    size_t start, end, next;
    int width;
} vline;

typedef struct {
    const cydui_font *font;
    const char *buf;
    size_t len, pos;
    int wrap;
    bool tail_done;
} liter;

static void liter_init(liter *l, const cydui_font *font, const char *buf, size_t len, int wrap)
{
    l->font = font;
    l->buf = buf;
    l->len = len;
    l->wrap = wrap;
    l->pos = 0;
    l->tail_done = false;
}

/* Lines of the whole text, however long: each one is found in a window of at most CYDUI_MAX_STR bytes that starts
 * where it does. After a final newline, and for an empty text, there is one more, empty line, where the cursor can be. */
static bool liter_next(liter *l, vline *out)
{
    cydui_text_iter it;
    cydui_text_line ln;
    bool truncated;
    size_t n;

    if (l->pos >= l->len) {
        if (l->tail_done || !(l->len == 0 || l->buf[l->len - 1] == '\n')) {
            return false;
        }
        l->tail_done = true;
        out->start = out->end = l->len;
        out->next = l->len + 1u;
        out->width = 0;
        return true;
    }
    n = cydui_text_clip_len(l->buf + l->pos, l->len - l->pos, &truncated);
    cydui_text_iter_init(&it, l->font, l->buf + l->pos, n, (int16_t)l->wrap);
    if (!cydui_text_next_line(&it, &ln)) { /* cannot happen: there is a byte; but never loop */
        l->pos = l->len;
        return false;
    }
    out->start = l->pos + ln.start;
    out->end = l->pos + ln.end;
    out->width = ln.width;
    l->pos += it.pos;
    out->next = l->pos;
    if (l->pos >= l->len && !(l->buf[l->len - 1] == '\n')) {
        out->next = l->len + 1u; /* the last line owns the end of the text */
    }
    return true;
}

typedef struct {
    size_t total;      /* lines, counting the empty one at the end */
    size_t cursor_line;
} scan_t;

static void scan(const measure *m, const char *buf, size_t len, int wrap, size_t cur, scan_t *out)
{
    liter l;
    vline v;
    size_t i = 0;

    out->total = 0;
    out->cursor_line = 0;
    liter_init(&l, m->font, buf, len, wrap);
    while (liter_next(&l, &v)) {
        if (cur >= v.start && cur < v.next) {
            out->cursor_line = i;
        }
        i++;
    }
    out->total = i;
}

static bool line_at(const measure *m, const char *buf, size_t len, int wrap, size_t idx, vline *out)
{
    liter l;
    size_t i = 0;

    liter_init(&l, m->font, buf, len, wrap);
    while (liter_next(&l, out)) {
        if (i++ == idx) {
            return true;
        }
    }
    return false;
}

unsigned cydui_text_area(cydui_ui *ui, const char *label, char *buf, size_t cap, size_t max_chars, int rows, unsigned flags)
{
    cydui_mods m = cydui_take_mods(ui);
    const cydui_style *st = &ui->theme->style;
    cydui_input *in = &ui->in;
    cydui_state own, *s;
    cydui_id id;
    cydui_response resp;
    cydui_rect r, inner;
    cydui_paint_info pi;
    const cydui_wstyle *w;
    measure ms;
    scan_t sc;
    vline v;
    size_t hint_n, len, cur, i;
    int lh, wrap, top, row, cx = 0;
    bool disabled, focused, attention, full = false;
    unsigned result = 0;

    (void)flags;
    if (!cydui_widget_begin(ui)) {
        return 0;
    }
    label = label ? label : "";
    id = cydui_id_str(ui, label);
    cydui_id_claim(ui, id);
    hint_n = cydui_label_len(label);
    rows = rows < 1 ? 1 : (rows > AREA_MAX_ROWS ? AREA_MAX_ROWS : rows);
    ms.font = st->font;
    ms.password = false;
    ms.bullet_w = 0;
    lh = st->font->line_height ? st->font->line_height : 1;
    r = cydui_alloc(ui, &m, cydui_layout_avail(ui).w, rows * lh + 2 * AREA_PAD_Y);
    inner.x = (int16_t)(r.x + FIELD_PAD);
    inner.y = (int16_t)(r.y + AREA_PAD_Y);
    inner.w = (int16_t)(r.w > 2 * FIELD_PAD ? r.w - 2 * FIELD_PAD : 0);
    inner.h = (int16_t)(rows * lh);
    wrap = inner.w > CURSOR_W ? inner.w - CURSOR_W : 1;

    disabled = m.disabled || !buf || cap == 0;
    s = cydui_state_get(ui, id, true);
    if (!s) {
        memset(&own, 0, sizeof(own));
        s = &own;
    }
    cur = s->a < 0 ? 0 : (size_t)s->a;
    top = s->b < 0 ? 0 : s->b;
    if (buf && cap > 0) {
        cydui_textedit_normalize(buf, cap, &cur);
        len = cydui_textedit_len(buf, cap);
    } else {
        cur = 0;
        len = 0;
    }

    resp = cydui_interact(ui, id, r, CYDUI_SENSE_CLICK | (disabled ? CYDUI_DISABLED : 0u));
    if (resp.pressed) {
        ui->focus_claimed = true;
    }
    if (resp.clicked) {
        size_t idx = (size_t)top + (size_t)((resp.y > inner.y ? resp.y - inner.y : 0) / lh);

        ui->focus = id;
        scan(&ms, buf, len, wrap, cur, &sc);
        if (idx >= sc.total) {
            idx = sc.total ? sc.total - 1u : 0;
        }
        if (line_at(&ms, buf, len, wrap, idx, &v)) {
            cur = v.start + at_of(&ms, buf + v.start, v.end - v.start, resp.x - inner.x);
        }
    }
    focused = !disabled && cydui_id_eq(ui->focus, id);
    if (focused) {
        ui->focus_seen = true;
        for (i = 0; i < in->k_len; i++) {
            const cydui_event *ev = &in->keys[i];
            unsigned res = 0;
            bool stop = false;

            if (ev->type == CYDUI_EVENT_CHAR) {
                res = cydui_textedit_insert(buf, cap, &cur, ev->key, CYDUI_EDIT_MULTILINE, max_chars);
            } else {
                switch ((cydui_key)ev->key) {
                case CYDUI_KEY_ENTER: res = cydui_textedit_insert(buf, cap, &cur, '\n', CYDUI_EDIT_MULTILINE, max_chars); break;
                case CYDUI_KEY_BACKSPACE: res = cydui_textedit_backspace(buf, cap, &cur); break;
                case CYDUI_KEY_DELETE: res = cydui_textedit_delete(buf, cap, &cur); break;
                case CYDUI_KEY_LEFT: res = cydui_textedit_move(buf, cap, &cur, CYDUI_MOVE_LEFT, 0); break;
                case CYDUI_KEY_RIGHT: res = cydui_textedit_move(buf, cap, &cur, CYDUI_MOVE_RIGHT, 0); break;
                case CYDUI_KEY_HOME:
                case CYDUI_KEY_END:
                    len = cydui_textedit_len(buf, cap);
                    scan(&ms, buf, len, wrap, cur, &sc);
                    if (line_at(&ms, buf, len, wrap, sc.cursor_line, &v)) {
                        cur = ev->key == CYDUI_KEY_HOME ? v.start : v.end;
                    }
                    break;
                case CYDUI_KEY_UP:
                case CYDUI_KEY_DOWN: {
                    int x;
                    size_t to;

                    len = cydui_textedit_len(buf, cap);
                    scan(&ms, buf, len, wrap, cur, &sc);
                    if (!line_at(&ms, buf, len, wrap, sc.cursor_line, &v)) {
                        break;
                    }
                    x = x_of(&ms, buf + v.start, cur - v.start, cur - v.start, NULL);
                    to = ev->key == CYDUI_KEY_UP ? sc.cursor_line - 1u : sc.cursor_line + 1u;
                    if ((ev->key == CYDUI_KEY_UP && sc.cursor_line == 0) || to >= sc.total) {
                        break;
                    }
                    if (line_at(&ms, buf, len, wrap, to, &v)) {
                        cur = v.start + at_of(&ms, buf + v.start, v.end - v.start, x);
                    }
                    break;
                }
                case CYDUI_KEY_ESCAPE:
                    stop = true;
                    break;
                default:
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
    }
    len = (buf && cap > 0) ? cydui_textedit_len(buf, cap) : 0;
    attention = attention_step(ui, s, full);

    /* keep the cursor line in view */
    scan(&ms, buf ? buf : "", len, wrap, cur, &sc);
    if ((size_t)top > sc.cursor_line) {
        top = (int)sc.cursor_line;
    }
    if (sc.cursor_line >= (size_t)top + (size_t)rows) {
        top = (int)(sc.cursor_line - (size_t)rows + 1u);
    }
    if (sc.total <= (size_t)rows) {
        top = 0;
    } else if ((size_t)top > sc.total - (size_t)rows) {
        top = (int)(sc.total - (size_t)rows);
    }
    s->a = (int32_t)cur;
    s->b = top;

    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_TEXT_INPUT;
    pi.state = disabled ? CYDUI_STATE_DISABLED : (focused ? CYDUI_STATE_FOCUSED : CYDUI_STATE_NORMAL);
    pi.selected = attention;
    pi.rect = r;
    cydui_paint(ui, &pi);
    w = cydui_wstyle_of(ui, CYDUI_WIDGET_TEXT_INPUT, pi.state, attention);

    if (len == 0) {
        if (hint_n) {
            cydui_draw_text_in(ui, inner, inner.x, inner.y, label, hint_n, st->text_muted);
        }
    } else {
        liter l;
        size_t idx = 0;

        liter_init(&l, st->font, buf, len, wrap);
        while (liter_next(&l, &v)) {
            row = (int)idx - top;
            if (row >= rows) {
                break;
            }
            if (row >= 0 && v.end > v.start) {
                size_t n = v.end - v.start;
                cydui_draw_text_in(ui, inner, inner.x, (int)inner.y + row * lh, buf + v.start, n > CYDUI_MAX_STR ? CYDUI_MAX_STR : n,
                                   w->text);
            }
            if (idx == sc.cursor_line) {
                cx = x_of(&ms, buf + v.start, cur > v.start ? cur - v.start : 0, cur > v.start ? cur - v.start : 0, NULL);
            }
            idx++;
        }
    }
    if (focused) {
        int x = cx > inner.w - CURSOR_W ? inner.w - CURSOR_W : cx;

        row = (int)sc.cursor_line - top;
        if (len == 0) {
            x = 0;
        }
        if (row >= 0 && row < rows) {
            cydui_draw_rect(&ui->ctx, (cydui_rect){(int16_t)(inner.x + x), (int16_t)(inner.y + row * lh), CURSOR_W, (int16_t)lh},
                            w->accent, 255);
        }
    }
    if (focused && !ui->kb_off && (resp.clicked || (result & CYDUI_TEXT_CHANGED))) {
        cydui__scroll_reveal(ui, r, cydui__keyboard_top(ui) - 4);
    }
    return result;
}
