/*
 * libFuzzer harness for the immediate-mode UI (CT-12). The input is read as a sequence of API calls:
 * widgets, containers opened and closed in any order (also unbalanced on purpose), push_id/pop_id,
 * one-shot modifiers, raw cydui_interact, the state table, clip changes, touch events of every kind
 * (and invalid ones), clock jumps (also across the 32-bit wrap) and frame boundaries; and since the
 * themes, the value widgets, the scroll areas and lists, the alignment, the cards and the widget of
 * the example (a custom one) as well, with themes built from the input and invalid ones.
 *
 * Under ASan/UBSan it looks for memory errors. Besides, it checks (and aborts on):
 *   - the structural limits (nesting depth, id stack, event queue, widget count) after every frame;
 *   - that every command in the list is well formed: a known type, a size that is not negative, a
 *     clip inside the screen;
 *   - that a card only ever moves its own commands: what came before it stays, what was drawn
 *     inside it keeps its order as a block, and nothing is written past the end of the list;
 *   - that a list says it needs no more items than its window can show;
 *   - that switching themes does not touch the state table, and that an invalid theme is refused
 *     and changes nothing;
 *   - that a screen of any structure settles: with the touch released and the clock running, the
 *     frame the UI asks for next is not asked for forever (no layout that chases itself, no coast
 *     that does not end);
 *   - that the whole session, run with different strip heights, gives the identical screen and the
 *     identical values in the widgets: nothing may depend on how the frame is cut into strips. The third
 *     run also uses two strip buffers and dirty regions (strips that did not change are not drawn again),
 *     so a wrong hash of a command, or a strip skipped by mistake, shows as a different screen.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/ui.h"
#include "cydui_mem.h"
#include "cydui_sans_12.h"
#include "cydui_sans_16.h"
#include "cydui_themes.h"
#include "rating.h"

#define W 160
#define H 120
#define MAX_OPS 260
#define SETTLE_FRAMES 400
#define STATE_BIG 64

typedef struct {
    const uint8_t *p;
    size_t n;
} reader;

static uint8_t u8(reader *r)
{
    if (r->n == 0) {
        return 0;
    }
    r->n--;
    return *r->p++;
}

static int16_t i16(reader *r)
{
    unsigned lo = u8(r);
    unsigned hi = u8(r);
    return (int16_t)(lo | (hi << 8));
}

/* Mostly small, with an escape to the whole int16 range, so that sizes and positions land on the
 * 160x120 screen yet the extremes still get tried. */
static int16_t small(reader *r, int range)
{
    uint8_t b = u8(r);
    return b == 0xFF ? i16(r) : (int16_t)((int)b % range - range / 4);
}

/* A 32-bit value that likes the edges: 0, 1, -1, the extremes, or something in between. */
static int32_t ext32(reader *r)
{
    uint8_t k = u8(r);

    switch (k % 8u) {
    case 0: return 0;
    case 1: return 1;
    case 2: return -1;
    case 3: return INT32_MIN;
    case 4: return INT32_MAX;
    case 5: return (int32_t)((unsigned)u8(r) | ((unsigned)u8(r) << 8) | ((unsigned)u8(r) << 16) | ((unsigned)u8(r) << 24));
    case 6: return (int32_t)small(r, 200);
    default: return (int32_t)u8(r);
    }
}

/*
 * Every region of memory the library is given is a separate object, so that ASan puts a redzone
 * around each: an overflow of one array into the next one would otherwise go unseen.
 */
static cydui_mem g_mem;
static cydui_ui g_ui;
static cydui_theme g_th_default, g_th_clean, g_th_dark, g_th_fuzz, g_th_bad;
static cydui_color g_fb[W * H];
static cydui_cmd g_cmds[96];
static cydui_color g_strip[W * H];
static char g_text[512];
static cydui_state g_state[STATE_BIG]; /* a few entries at times: the table is full and evicting all the time */
static uint32_t g_idset[64];
static uint64_t g_dirty[64]; /* the dirty regions: one entry per strip */

typedef struct {
    bool vals[4];
    int32_t iv[4];
    int sel[2];
    int rate[2];
    int32_t first, last;
    char text[3][40]; /* what the text fields edit: arbitrary bytes at times, with no NUL */
    bool popup_open[2];
} vars;

/* ---- painters for the themes built from the input ---- */

static void paint_noop(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *pi)
{
    (void)ui;
    (void)t;
    (void)pi;
}

static void paint_extra(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *pi)
{
    cydui_paint_default(ui, t, pi);
    cydui_draw_rrect_outline(&ui->ctx, pi->rect, 255, 7, CYDUI_RGB(200, 0, 200), 90);
    if (pi->text && pi->text_len) {
        cydui_draw_text(&ui->ctx, t->style.font, pi->rect.x, pi->rect.y, 0, CYDUI_ALIGN_LEFT, pi->text,
                        pi->text_len, CYDUI_RGB(0, 200, 0), 255);
    }
}

static void paint_huge(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *pi)
{
    (void)t;
    (void)pi;
    cydui_draw_rect(&ui->ctx, (cydui_rect){-32768, -32768, 32767, 32767}, CYDUI_RGB(9, 9, 9), 40);
    cydui_draw_rrect(&ui->ctx, (cydui_rect){-100, -100, 32000, 32000}, 255, CYDUI_RGB(30, 0, 0), 20);
}

static cydui_paint_fn const painters[5] = {NULL, cydui_paint_default, paint_extra, paint_huge, paint_noop};

static uint32_t xorshift(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *s = x;
}

/* A theme whose every number comes from a seed: colors, radii, border widths, shadows (0..255),
 * metrics up to the extremes, and one of five painters per kind. */
static void make_fuzz_theme(cydui_theme *t, uint32_t seed, bool big_font)
{
    uint32_t s = seed | 1u;
    int k, sel, st;

    cydui_theme_default(t, big_font ? &cydui_sans_16 : &cydui_sans_12);
    t->name = "fuzz";
    t->style.bg = (cydui_color)xorshift(&s);
    t->style.panel = (cydui_color)xorshift(&s);
    t->style.text = (cydui_color)xorshift(&s);
    t->style.accent = (cydui_color)xorshift(&s);
    t->style.border = (cydui_color)xorshift(&s);
    t->style.radius = (uint8_t)xorshift(&s);
    t->style.pad_x = (uint8_t)(xorshift(&s) % 3 ? xorshift(&s) % 24 : xorshift(&s));
    t->style.pad_y = (uint8_t)(xorshift(&s) % 3 ? xorshift(&s) % 24 : xorshift(&s));
    t->style.spacing = (uint8_t)(xorshift(&s) % 3 ? xorshift(&s) % 16 : xorshift(&s));
    t->style.min_target = (uint8_t)(xorshift(&s) % 3 ? xorshift(&s) % 40 : xorshift(&s));
    t->style.anim_ms = (uint16_t)(xorshift(&s) % 2 ? 0u : xorshift(&s) % 600u); /* widgets that change their look over time */
    for (k = 0; k < CYDUI_WIDGET_COUNT; k++) {
        for (sel = 0; sel < 2; sel++) {
            for (st = 0; st < CYDUI_STATE_COUNT; st++) {
                cydui_wstyle *w = &t->table[k][sel][st];
                w->fill = (cydui_color)xorshift(&s);
                w->border = (cydui_color)xorshift(&s);
                w->text = (cydui_color)xorshift(&s);
                w->accent = (cydui_color)xorshift(&s);
                w->border_w = (uint8_t)(xorshift(&s) % 4 ? xorshift(&s) % 5 : xorshift(&s));
                w->radius = (uint8_t)xorshift(&s);
                w->shadow = (uint8_t)(xorshift(&s) % 2 ? 0 : xorshift(&s));
            }
        }
        t->paint[k] = painters[xorshift(&s) % 5u];
    }
}

/* ---- the oracles ---- */

static uint64_t cmds_hash(uint32_t from, uint32_t to)
{
    uint64_t h = 1469598103934665603ull;
    const unsigned char *b = (const unsigned char *)&g_ui.ctx.cmds[from];
    size_t n = (size_t)(to - from) * sizeof(cydui_cmd), i;

    for (i = 0; i < n; i++) {
        h = (h ^ b[i]) * 1099511628211ull;
    }
    return h;
}

static uint32_t states_used(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < g_ui.state_cap; i++) {
        n += g_ui.state[i].used ? 1u : 0u;
    }
    return n;
}

static void check_cmds(const cydui_ctx *c)
{
    uint32_t i;

    if (c->cmd_len > c->cmd_cap) {
        abort();
    }
    for (i = 0; i < c->cmd_len; i++) {
        const cydui_cmd *m = &c->cmds[i];
        if (m->type > CYDUI_CMD_TEXT || m->rect.w < 0 || m->rect.h < 0 || m->clip.w < 0 || m->clip.h < 0 ||
            m->clip.x < 0 || m->clip.y < 0 || m->clip.x + m->clip.w > W || m->clip.y + m->clip.h > H) {
            abort(); /* a malformed command */
        }
    }
}

static void check(const cydui_ui *ui)
{
    if (ui->depth > CYDUI_MAX_LAYOUT_DEPTH || ui->id_depth > CYDUI_MAX_ID_DEPTH ||
        ui->in.q_len > CYDUI_EVENT_QUEUE || ui->widgets > CYDUI_MAX_WIDGETS ||
        ui->in.last_processed > CYDUI_EVENTS_PER_FRAME || ui->in.k_len > CYDUI_KEY_QUEUE) {
        abort(); /* a structural limit was broken */
    }
    check_cmds(&ui->ctx);
}

/*
 * Labels mostly come from a tiny vocabulary, so the same id turns up in different operations (a
 * press, a state entry, a repeated widget); the rest is arbitrary bytes, '#' included, to build
 * "##" suffixes and invalid UTF-8.
 */
static const char *const vocab[] = {"a", "b", "c", "d", "e", "f", "Done##1", "##", "x#"};

static const char *make_text(reader *r, char *buf, size_t cap)
{
    uint8_t k = u8(r);
    size_t n, i;

    if (k < 200) {
        return vocab[k % (sizeof(vocab) / sizeof(vocab[0]))];
    }
    n = u8(r) % (cap - 1);
    for (i = 0; i < n; i++) {
        buf[i] = (char)u8(r);
        if (buf[i] == '\0') {
            buf[i] = '#';
        }
    }
    buf[n] = '\0';
    return buf;
}

/* ---- one operation ---- */

enum { OP_COUNT = 61 };
enum { OP_DONE, OP_NEW_FRAME, OP_STOP }; /* what do_op says: nothing special, a frame began, the replay is over */

/* Returns OP_STOP when the operation was a frame boundary and `replay` is on (the frame is over). In a
 * replay the operations that feed the UI from outside (touch, clock, jumps, theme switches, bursts)
 * are read but not run, so that the same screen is built again and again with nothing changing it. */
static int do_op(reader *r, vars *v, bool replay)
{
    char tmp[40];
    int i;

    switch (u8(r) % OP_COUNT) {
    case 0: cydui_label(&g_ui, make_text(r, tmp, sizeof(tmp))); break;
    case 1: cydui_button(&g_ui, make_text(r, tmp, sizeof(tmp))); break;
    case 2: cydui_checkbox(&g_ui, make_text(r, tmp, sizeof(tmp)), (u8(r) & 1u) ? &v->vals[u8(r) % 4u] : NULL); break;
    case 3: cydui_toggle(&g_ui, make_text(r, tmp, sizeof(tmp)), (u8(r) & 1u) ? &v->vals[u8(r) % 4u] : NULL); break;
    case 4: cydui_label_wrapped(&g_ui, make_text(r, tmp, sizeof(tmp))); break;
    case 5: cydui_begin_row(&g_ui); break;
    case 6: cydui_end_row(&g_ui); break;
    case 7: cydui_begin_column(&g_ui); break;
    case 8: cydui_end_column(&g_ui); break;
    case 9: cydui_begin_grid(&g_ui, (int)(int8_t)u8(r)); break;
    case 10: cydui_end_grid(&g_ui); break;
    case 11: cydui_begin_padding(&g_ui, small(r, 40)); break;
    case 12: cydui_end_padding(&g_ui); break;
    case 13: cydui_set_spacing(&g_ui, small(r, 40)); break;
    case 14: cydui_spacer(&g_ui, small(r, 60)); break;
    case 15: cydui_spacer_fill(&g_ui); break;
    case 16: cydui_next_width(&g_ui, small(r, 120)); break;
    case 17: cydui_next_height(&g_ui, small(r, 80)); break;
    case 18: cydui_next_fill(&g_ui); break;
    case 19: cydui_next_stretch(&g_ui); break;
    case 20: cydui_next_disabled(&g_ui); break;
    case 21: cydui_push_id_int(&g_ui, i16(r)); break;
    case 22: cydui_pop_id(&g_ui); break;
    case 23: cydui_push_id_str(&g_ui, make_text(r, tmp, sizeof(tmp))); break;
    case 24: {
        int16_t w = small(r, 80), h = small(r, 60);
        cydui_alloc_rect(&g_ui, w, h);
        break;
    }
    case 25: { /* a touch event, at times of a type that does not exist */
        uint8_t t = u8(r) % 6u;
        cydui_event_type type = t == 0 ? CYDUI_EVENT_DOWN : t == 1 ? CYDUI_EVENT_UP
                                : t == 5 ? (cydui_event_type)(u8(r) | 0x40u) : CYDUI_EVENT_MOVE;
        int16_t x = small(r, 200), y = small(r, 150);
        if (!replay) {
            cydui_mem_push_event(&g_mem, type, x, y);
        }
        break;
    }
    case 26: { /* the clock, including a jump that wraps it */
        uint8_t k = u8(r);
        if (!replay) {
            g_mem.now += k == 0xFF ? 0xFFFFF000u : (uint32_t)k * 7u;
        }
        break;
    }
    case 27: /* a frame boundary */
        if (replay) {
            return OP_STOP;
        }
        cydui_ui_end(&g_ui);
        check(&g_ui);
        cydui_ui_begin(&g_ui, 0);
        return OP_NEW_FRAME;
    case 28: { /* raw interaction on a rectangle, with any flags */
        cydui_rect rc = {small(r, 100), small(r, 80), small(r, 90), small(r, 70)};
        cydui_id id = cydui_id_str(&g_ui, make_text(r, tmp, sizeof(tmp)));
        cydui_id_claim(&g_ui, id);
        cydui_interact(&g_ui, id, rc, u8(r));
        break;
    }
    case 29: {
        cydui_rect rc = {small(r, 100), small(r, 80), small(r, 120), small(r, 90)};
        cydui_set_clip(&g_ui.ctx, rc);
        break;
    }
    case 30: cydui_reset_clip(&g_ui.ctx); break;
    case 31: { /* hammer the state table with ids that all want an entry */
        cydui_id id = cydui_id_str(&g_ui, make_text(r, tmp, sizeof(tmp)));
        cydui_id active = g_ui.active;
        bool had_active = (active.h || active.c) && cydui_state_get(&g_ui, active, false) != NULL;
        cydui_state *st = cydui_state_get(&g_ui, id, (u8(r) & 1u) != 0);

        if (st) {
            st->a = (int32_t)u8(r);
        }
        /* The oracle no sanitizer can be: a press must not lose its state to a newcomer. */
        if (had_active && cydui_state_get(&g_ui, active, false) == NULL) {
            abort();
        }
        break;
    }
    case 32: (void)cydui_layout_avail(&g_ui); break;
    case 33: { /* a burst of identical moves, like a flood */
        int16_t x = small(r, 160), y = small(r, 120);
        if (!replay) {
            for (i = 0; i < 80; i++) {
                cydui_mem_push_event(&g_mem, CYDUI_EVENT_MOVE, (int16_t)(x + i % 5), y);
            }
        }
        break;
    }
    case 34: { /* a burst that overflows the queue: press/release pairs, or releases only */
        uint8_t n = (uint8_t)(u8(r) % 40u);
        bool only_up = (u8(r) & 1u) != 0; /* orphan releases are cheap: many fit in one frame's budget */
        int16_t x = small(r, 160), y = small(r, 120);
        if (!replay) {
            for (i = 0; i < n; i++) {
                cydui_mem_push_event(&g_mem, only_up || (i & 1) ? CYDUI_EVENT_UP : CYDUI_EVENT_DOWN, x, y);
            }
        }
        break;
    }
    case 35: { /* the value widgets, with values at the edges of the type (arguments read in order) */
        const char *label = make_text(r, tmp, sizeof(tmp));
        int32_t *val = (u8(r) & 1u) ? &v->iv[u8(r) % 4u] : NULL;
        int32_t lo = ext32(r), hi = ext32(r);
        cydui_slider(&g_ui, label, val, lo, hi);
        break;
    }
    case 36: {
        int32_t value, max;
        if (u8(r) & 1u) {
            v->iv[u8(r) % 4u] = ext32(r);
        }
        value = ext32(r);
        max = ext32(r);
        cydui_progress(&g_ui, value, max);
        break;
    }
    case 37: { /* tabs: exactly 8 labels, however many are asked for */
        const char *labels[8];
        int count = (int)(u8(r) % 26u) - 5;
        int *sel = (u8(r) & 1u) ? &v->sel[u8(r) % 2u] : NULL;
        uint8_t m = u8(r);

        for (i = 0; i < 8; i++) {
            labels[i] = (m >> i) & 1u ? NULL : vocab[(unsigned)(i + m) % (sizeof(vocab) / sizeof(vocab[0]))];
        }
        if (sel && (u8(r) & 1u)) {
            *sel = (int)ext32(r);
        }
        cydui_tabs(&g_ui, labels, count, sel);
        break;
    }
    case 38: { /* the widget of the example, written with the public API only */
        const char *label = make_text(r, tmp, sizeof(tmp));
        int *val = (u8(r) & 1u) ? &v->rate[u8(r) % 2u] : NULL;
        if (val && (u8(r) & 1u)) {
            *val = (int)ext32(r);
        }
        rating(&g_ui, label, val);
        break;
    }
    case 39: cydui_scroll_begin(&g_ui, make_text(r, tmp, sizeof(tmp)), small(r, 150)); break;
    case 40: cydui_scroll_end(&g_ui); break;
    case 41: {
        const char *label = make_text(r, tmp, sizeof(tmp));
        int32_t y = ext32(r);
        if (!replay) {
            cydui_scroll_to(&g_ui, label, y);
        }
        break;
    }
    case 42: { /* a list: what it asks the application to build must fit its window */
        const char *label = make_text(r, tmp, sizeof(tmp));
        int32_t count = ext32(r);
        int16_t item_h = small(r, 80), view_h = small(r, 150);
        bool outputs = (u8(r) & 1u) != 0;
        bool plain = g_ui.mods.h < 0 && !g_ui.mods.fill;

        v->first = v->last = -77;
        cydui_list_begin(&g_ui, label, count, item_h, view_h, outputs ? &v->first : NULL, outputs ? &v->last : NULL);
        if (outputs) {
            if (v->first < 0 || v->last < v->first || (count > 0 && v->last > count) ||
                (count <= 0 && (v->first || v->last))) {
                abort();
            }
            if (plain && item_h > 0 && view_h >= 0 && (int64_t)(v->last - v->first) > view_h / item_h + 2) {
                abort(); /* more rows than the window can show */
            }
        }
        break;
    }
    case 43: {
        int32_t index = (u8(r) & 1u) ? v->first + (int32_t)(u8(r) % 5u) : ext32(r);
        cydui_begin_list_item(&g_ui, index);
        break;
    }
    case 44: cydui_end_list_item(&g_ui); break;
    case 45: cydui_list_end(&g_ui); break;
    case 46: cydui_set_cross_align(&g_ui, (cydui_place)(int8_t)u8(r)); break;
    case 47: cydui_set_main_align(&g_ui, (cydui_place)(int8_t)u8(r)); break;
    case 48: cydui_begin_card(&g_ui); break;
    case 49: { /* a card moves its own commands and nobody else's */
        const cydui_region *rg = &g_ui.regions[g_ui.depth];
        bool real = g_ui.skipped == 0 && g_ui.depth > 0 && rg->kind == CYDUI_LAYOUT_COLUMN && rg->card;
        uint32_t start = real ? rg->cmd_start : 0, len = g_ui.ctx.cmd_len, cap = g_ui.ctx.cmd_cap;
        uint64_t before = real ? cmds_hash(0, start) : 0, kids = real ? cmds_hash(start, len) : 0;
        static cydui_cmd free_slots[96];
        uint32_t len2;

        if (len < cap) { /* what lies beyond the list now, to see that nobody touches it */
            memcpy(free_slots, &g_ui.ctx.cmds[len], (size_t)(cap - len) * sizeof(cydui_cmd));
        }
        cydui_end_card(&g_ui);
        len2 = g_ui.ctx.cmd_len;
        if (real && (len2 < len || cmds_hash(0, start) != before || cmds_hash(len2 - (len - start), len2) != kids)) {
            abort();
        }
        if (len2 < cap && len2 >= len &&
            memcmp(&g_ui.ctx.cmds[len2], &free_slots[len2 - len], (size_t)(cap - len2) * sizeof(cydui_cmd)) != 0) {
            abort(); /* something wrote past the end of the command list */
        }
        break;
    }
    case 50: { /* switch themes: four good ones and two bad ones */
        uint8_t which = u8(r) % 7u;
        uint32_t seed = (uint32_t)u8(r) | ((uint32_t)u8(r) << 8) | ((uint32_t)u8(r) << 16) | ((uint32_t)u8(r) << 24);
        bool big_font = (u8(r) & 1u) != 0;

        if (!replay) {
            const cydui_theme *want = which == 0 ? &g_th_default : which == 1 ? &g_th_clean : which == 2 ? &g_th_dark
                                      : which == 3 ? NULL : which == 4 ? &g_th_bad : &g_th_fuzz;
            const cydui_theme *was = g_ui.theme;
            uint32_t used = states_used(), rc;

            if (want == &g_th_fuzz && was != &g_th_fuzz) { /* it may be rebuilt now: nobody is using it */
                make_fuzz_theme(&g_th_fuzz, seed, big_font);
            } else if (want == &g_th_fuzz) {
                want = &g_th_default; /* not while it is the theme in use: widgets may hold its pointers */
            }
            rc = cydui_set_theme(&g_ui, want);
            if (want == NULL || want == &g_th_bad) {
                if (rc == 0 || g_ui.theme != was) {
                    abort(); /* an invalid theme must be refused and change nothing */
                }
            } else if (rc != 0 || g_ui.theme != want) {
                abort();
            }
            if (states_used() != used) {
                abort(); /* a theme switch does not touch the state table */
            }
        }
        break;
    }
    case 51: { /* time passes and a frame ends */
        uint8_t k = u8(r);
        if (!replay) {
            g_mem.now += (uint32_t)k % 64u + 1u;
            cydui_ui_end(&g_ui);
            check(&g_ui);
            cydui_ui_begin(&g_ui, 0);
            return OP_NEW_FRAME;
        }
        break;
    }
    case 53: { /* a single line field: a buffer of any size up to the real one (or none), limits, password */
        static const size_t caps[] = {0, 1, 2, 5, 40};
        uint8_t b = u8(r);
        size_t cap = caps[u8(r) % 5u];
        unsigned fl = (u8(r) & 1u) ? CYDUI_TEXT_PASSWORD : 0u;
        cydui_push_id_str(&g_ui, "T"); /* the ids of the fields are apart from the ones of the scroll areas (one entry, two owners) */
        cydui_text_input(&g_ui, make_text(r, tmp, sizeof(tmp)), (b & 8u) ? NULL : v->text[b % 3u], cap, u8(r) % 6u, fl);
        cydui_pop_id(&g_ui);
        break;
    }
    case 54: { /* a text area */
        static const size_t caps[] = {0, 1, 3, 12, 40};
        uint8_t b = u8(r);
        size_t cap = caps[u8(r) % 5u];
        cydui_push_id_str(&g_ui, "T");
        cydui_text_area(&g_ui, make_text(r, tmp, sizeof(tmp)), (b & 8u) ? NULL : v->text[b % 3u], cap, u8(r) % 6u,
                        (int)(int8_t)u8(r), u8(r));
        cydui_pop_id(&g_ui);
        break;
    }
    case 55: { /* the application fills a buffer with bytes of its own, and no NUL */
        uint8_t b = u8(r);
        for (i = 0; i < 40; i++) {
            v->text[b % 3u][i] = (char)u8(r);
        }
        break;
    }
    case 56: { /* a typed character: any code point, valid or not */
        uint8_t k = u8(r);
        uint32_t cp = k < 64 ? (uint32_t)u8(r) : k < 128 ? (uint32_t)u8(r) << 8 | u8(r)
                      : k < 192 ? (uint32_t)u8(r) << 16 | (uint32_t)u8(r) << 8 | u8(r) : 0xFFFFFFFFu - (uint32_t)u8(r);
        if (!replay) {
            cydui_mem_push_key(&g_mem, CYDUI_EVENT_CHAR, cp);
        }
        break;
    }
    case 57: { /* an editing key, at times one that does not exist */
        uint8_t k = u8(r);
        if (!replay) {
            cydui_mem_push_key(&g_mem, CYDUI_EVENT_KEY, k < 220 ? (uint32_t)(k % 12u) : (uint32_t)k << 24);
        }
        break;
    }
    case 58: { /* a popup, with or without its end, with content that needs the touch */
        uint8_t b = u8(r);
        static const int sizes[] = {-3, 0, 40, 120, 240, 100000};
        int w = sizes[u8(r) % 6u], h = sizes[u8(r) % 6u];
        if (cydui_popup_begin(&g_ui, make_text(r, tmp, sizeof(tmp)), &v->popup_open[b & 1u], w, h, (b & 2u) ? CYDUI_POPUP_MODAL : 0u)) {
            cydui_button(&g_ui, "ok");
            if (b & 4u) {
                cydui_push_id_str(&g_ui, "T");
                cydui_text_input(&g_ui, "p", v->text[b % 3u], 40, 0, 0);
                cydui_pop_id(&g_ui);
            }
            if (!(b & 8u)) {
                cydui_popup_end(&g_ui);
            }
        }
        if (!replay) { /* what opens and closes it comes from outside, like the touch: in a replay nothing changes */
            v->popup_open[b & 1u] = (b & 16u) != 0 ? !v->popup_open[b & 1u] : v->popup_open[b & 1u] || (b & 32u) != 0;
        }
        break;
    }
    case 59: { /* the focus, and the keyboard */
        uint8_t b = u8(r);
        if (b % 4u == 0) {
            cydui_focus_set(&g_ui, make_text(r, tmp, sizeof(tmp)));
        } else if (b % 4u == 1) {
            cydui_focus_clear(&g_ui);
        } else if (b % 4u == 2) {
            cydui_keyboard_enable(&g_ui, (u8(r) & 1u) != 0);
        } else {
            (void)cydui_keyboard_top(&g_ui);
            (void)cydui_has_focus(&g_ui);
        }
        break;
    }
    case 60: { /* a tween: everything about it comes from its name, so that two calls with one name agree */
        static const int32_t ends[] = {INT32_MIN, INT32_MAX, 0, -1, 1, 255, 4096, 100000};
        const char *name = vocab[u8(r) % 6u]; /* few names: tweens share the state table with the layout, and a table that is too small makes the layout fight for it */
        const char *c;
        uint32_t h = 2166136261u;
        int32_t from, to, val;
        uint16_t d;
        cydui_ease e;

        for (c = name; *c; c++) {
            h = (h ^ (uint8_t)*c) * 16777619u;
        }
        from = (h & 1u) ? ends[(h >> 1) % 8u] : (int32_t)(h * 2654435761u);
        to = (h & 0x10u) ? ends[(h >> 5) % 8u] : (int32_t)(h * 40503u);
        d = (uint16_t)((h >> 8) % 5u == 0 ? 0u : ((h >> 11) & 0xFFu) * 8u); /* up to two seconds */
        e = (cydui_ease)((h >> 19) % 12u); /* three of them unknown */
        val = cydui_animate(&g_ui, cydui_id_str(&g_ui, name), from, to, d, e);
        if ((d == 0 || (unsigned)e >= CYDUI_EASE_COUNT || !g_ui.in.has_clock) && val != to) {
            abort(); /* with nothing to move it by, the value is the target at once */
        }
        break;
    }
    default: { /* a whole frame of widgets of one kind, to reach the per-frame limit */
        uint8_t n = u8(r);
        for (i = 0; i < (n & 1 ? 700 : 40); i++) {
            cydui_push_id_int(&g_ui, i);
            cydui_button(&g_ui, "x");
            cydui_pop_id(&g_ui);
        }
        break;
    }
    }
    return OP_DONE;
}

static uint64_t run(const uint8_t *data, size_t size, int rows, bool fancy)
{
    reader r, frame_start;
    vars v;
    cydui_ui_config cfg;
    uint32_t state_entries, cmd_entries;
    bool big_state, has_clock;
    int ops, frame_ops = 0, k;

    r.p = data;
    r.n = size;
    memset(&v, 0, sizeof(v));
    memset(g_fb, 0, sizeof(g_fb));
    memset(g_state, 0, sizeof(g_state));
    memset(g_idset, 0, sizeof(g_idset));
    cydui_mem_init(&g_mem, g_fb, W, H);
    cydui_theme_default(&g_th_default, &cydui_sans_12);
    cydui_theme_init_clean(&g_th_clean);
    cydui_theme_init_clean_dark(&g_th_dark);
    g_th_dark.style.anim_ms = 150; /* the one theme of the fuzz that animates its widgets */
    g_th_bad = g_th_default;
    g_th_bad.style.font = NULL;
    make_fuzz_theme(&g_th_fuzz, 1, false);

    /* The size of the state table (0 to 12 entries, so it is often full and sometimes absent, or a
     * roomy one), of the collision set (none, 16 or 64 slots), of the command list (4 to 96) and
     * whether there is a clock all come from the input. */
    memset(&cfg, 0, sizeof(cfg));
    cfg.core.backend = &g_mem.backend;
    cfg.core.cmd_mem = g_cmds;
    cfg.core.strip_mem = g_strip;
    cfg.core.strip_mem_size = (size_t)rows * W * sizeof(cydui_color) * (fancy ? 2u : 1u);
    cfg.core.strip_buffers = fancy ? 2 : 1;
    cfg.core.dirty_mem = fancy ? (void *)g_dirty : NULL;
    cfg.core.dirty_mem_size = fancy ? sizeof(g_dirty) : 0;
    cfg.core.text_mem = g_text;
    cfg.core.text_mem_size = sizeof(g_text);
    cfg.state_mem = g_state;
    state_entries = u8(&r) % 13u;
    cfg.idset_mem = g_idset;
    cfg.idset_mem_size = (size_t)(u8(&r) % 3u == 0 ? 0u : u8(&r) % 2u ? 16u : 64u) * sizeof(uint32_t);
    cmd_entries = 4u + u8(&r) % 93u;
    cfg.core.cmd_mem_size = (size_t)cmd_entries * sizeof(cydui_cmd);
    k = u8(&r);
    big_state = (k & 3) == 0;
    has_clock = (k & 4) == 0;
    cfg.state_mem_size = (size_t)(big_state ? STATE_BIG : state_entries) * sizeof(cydui_state);
    cfg.theme = &g_th_default;
    cfg.frame_budget_ms = (uint16_t)((k >> 3) % 4u == 2 ? 5u : (k >> 3) % 4u == 3 ? 40u : 0u); /* a time budget, now and then */
    if (!has_clock) {
        g_mem.backend.now_ms = NULL;
    }
    if (cydui_ui_init(&g_ui, &cfg) != 0) {
        abort();
    }
    cydui_ui_begin(&g_ui, 0);
    frame_start = r;

    for (ops = 0; ops < MAX_OPS && r.n > 0; ops++) {
        if (do_op(&r, &v, false) == OP_NEW_FRAME) {
            frame_start = r; /* the frame that is now open starts here */
            frame_ops = 0;
        } else {
            frame_ops++;
        }
    }
    cydui_ui_end(&g_ui);
    check(&g_ui);

    if (big_state
#ifdef FZ_NO_SETTLE
        && false
#endif
    ) {
        /* Settling: let go of the touch, let the clock run and build the last frame again and again
         * (nothing but the clock changes it): the UI may ask for another frame while a layout settles
         * or something coasts, but it has to stop asking. The state table is roomy in this mode, so
         * that an entry evicted every frame does not count as a layout that never settles. */
        bool more = true;

        /* the queue of the backend may be full of what the input threw at it: make room for the release */
        for (k = 0; k < 64 && !cydui_mem_push_event(&g_mem, CYDUI_EVENT_UP, 0, 0); k++) {
            cydui_ui_begin(&g_ui, 0);
            cydui_ui_end(&g_ui);
        }
        for (k = 0; k < SETTLE_FRAMES && more; k++) {
            reader again = frame_start;
            int n;

            if (g_ui.in.has_clock) {
                g_mem.now += 16;
            }
            cydui_ui_begin(&g_ui, 0);
            for (n = 0; n < frame_ops; n++) {
                if (do_op(&again, &v, true) == OP_STOP) {
                    break;
                }
            }
            more = cydui_ui_end(&g_ui);
            check(&g_ui);
        }
        if (more && !(cydui_ui_errors(&g_ui) & CYDUI_ERR_ID_COLLISION)) { /* two widgets with one id (an application error) can fight over their state for ever */
            abort(); /* a screen that never settles */
        }
    }
    for (k = 0; k < 3; k++) { /* a few more frames to let queued events drain and layouts settle */
        cydui_ui_begin(&g_ui, 0);
        cydui_button(&g_ui, "tail");
        cydui_ui_end(&g_ui);
        check(&g_ui);
    }
    {
        uint64_t h = cydui_mem_hash(&g_mem);
        int i;

        for (i = 0; i < 4; i++) { /* the values the widgets wrote count as much as the screen */
            h = (h ^ (uint64_t)v.vals[i]) * 1099511628211ull;
            h = (h ^ (uint64_t)(uint32_t)v.iv[i]) * 1099511628211ull;
        }
        for (i = 0; i < 2; i++) {
            h = (h ^ (uint64_t)(uint32_t)v.sel[i]) * 1099511628211ull;
            h = (h ^ (uint64_t)(uint32_t)v.rate[i]) * 1099511628211ull;
        }
        return h;
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint64_t a = run(data, size, 8, false);
    uint64_t b = run(data, size, 3, false);
    uint64_t c = run(data, size, 5, true);

    if (a != b || a != c) {
        abort(); /* the screen depended on the strip height, or on something other than the input */
    }
    return 0;
}
