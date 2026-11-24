/*
 * libFuzzer harness for the immediate-mode UI (CT-12). The input is read as a sequence of API calls:
 * widgets, containers opened and closed in any order (also unbalanced on purpose), push_id/pop_id,
 * one-shot modifiers, raw cydui_interact, the state table, clip changes, touch events of every kind
 * (and invalid ones), clock jumps (also across the 32-bit wrap) and frame boundaries.
 *
 * Under ASan/UBSan it looks for memory errors. After every frame it checks the structural limits
 * (nesting depth, id stack, event queue, widget count) and at the end it runs the whole session twice,
 * with different strip heights, and requires the identical screen: nothing may depend on how the frame
 * is cut into strips or on anything but the input.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/ui.h"
#include "cydui_mem.h"
#include "cydui_sans_12.h"

#define W 160
#define H 120
#define MAX_OPS 260

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

/*
 * Every region of memory the library is given is a separate object, so that ASan puts a redzone
 * around each: an overflow of one array into the next one would otherwise go unseen.
 */
static cydui_mem g_mem;
static cydui_ui g_ui;
static cydui_theme g_theme;
static cydui_color g_fb[W * H];
static cydui_cmd g_cmds[96];
static cydui_color g_strip[W * H];
static char g_text[512];
static cydui_state g_state[12]; /* few entries: the table is full and evicting all the time */
static uint32_t g_idset[64];

static void check(const cydui_ui *ui)
{
    if (ui->depth > CYDUI_MAX_LAYOUT_DEPTH || ui->id_depth > CYDUI_MAX_ID_DEPTH ||
        ui->in.q_len > CYDUI_EVENT_QUEUE || ui->widgets > CYDUI_MAX_WIDGETS ||
        ui->in.last_processed > CYDUI_EVENTS_PER_FRAME) {
        abort(); /* a structural limit was broken */
    }
}

/*
 * Labels mostly come from a tiny vocabulary, so the same id turns up in different operations (a
 * press, a state entry, a repeated widget); the rest is arbitrary bytes, '#' included, to build
 * "##" suffixes and invalid UTF-8.
 */
static const char *make_text(reader *r, char *buf, size_t cap)
{
    static const char *const vocab[] = {"a", "b", "c", "d", "e", "f", "Done##1", "##", "x#"};
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

static uint64_t run(const uint8_t *data, size_t size, int rows)
{
    reader r;
    cydui_ui_config cfg;
    bool vals[4] = {false, true, false, true};
    char tmp[40];
    int ops, i;

    r.p = data;
    r.n = size;
    memset(g_fb, 0, sizeof(g_fb));
    memset(g_state, 0, sizeof(g_state));
    memset(g_idset, 0, sizeof(g_idset));
    cydui_mem_init(&g_mem, g_fb, W, H);
    cydui_theme_default(&g_theme, &cydui_sans_12);

    memset(&cfg, 0, sizeof(cfg));
    cfg.core.backend = &g_mem.backend;
    cfg.core.cmd_mem = g_cmds;
    cfg.core.cmd_mem_size = sizeof(g_cmds);
    cfg.core.strip_mem = g_strip;
    cfg.core.strip_mem_size = (size_t)rows * W * sizeof(cydui_color);
    cfg.core.text_mem = g_text;
    cfg.core.text_mem_size = sizeof(g_text);
    /* The size of the state table (0 to 12 entries, so it is often full and sometimes absent) and of
     * the collision set (none, 16 or 64 slots) come from the input. */
    cfg.state_mem = g_state;
    cfg.state_mem_size = (size_t)(u8(&r) % 13u) * sizeof(cydui_state);
    cfg.idset_mem = g_idset;
    cfg.idset_mem_size = (size_t)(u8(&r) % 3u == 0 ? 0u : u8(&r) % 2u ? 16u : 64u) * sizeof(uint32_t);
    cfg.theme = &g_theme;
    if (cydui_ui_init(&g_ui, &cfg) != 0) {
        abort();
    }
    cydui_ui_begin(&g_ui, 0);

    for (ops = 0; ops < MAX_OPS && r.n > 0; ops++) {
        switch (u8(&r) % 36u) {
        case 0: cydui_label(&g_ui, make_text(&r, tmp, sizeof(tmp))); break;
        case 1: cydui_button(&g_ui, make_text(&r, tmp, sizeof(tmp))); break;
        case 2: cydui_checkbox(&g_ui, make_text(&r, tmp, sizeof(tmp)), (u8(&r) & 1u) ? &vals[u8(&r) % 4u] : NULL); break;
        case 3: cydui_toggle(&g_ui, make_text(&r, tmp, sizeof(tmp)), (u8(&r) & 1u) ? &vals[u8(&r) % 4u] : NULL); break;
        case 4: cydui_label_wrapped(&g_ui, make_text(&r, tmp, sizeof(tmp))); break;
        case 5: cydui_begin_row(&g_ui); break;
        case 6: cydui_end_row(&g_ui); break;
        case 7: cydui_begin_column(&g_ui); break;
        case 8: cydui_end_column(&g_ui); break;
        case 9: cydui_begin_grid(&g_ui, (int)(int8_t)u8(&r)); break;
        case 10: cydui_end_grid(&g_ui); break;
        case 11: cydui_begin_padding(&g_ui, small(&r, 40)); break;
        case 12: cydui_end_padding(&g_ui); break;
        case 13: cydui_set_spacing(&g_ui, small(&r, 40)); break;
        case 14: cydui_spacer(&g_ui, small(&r, 60)); break;
        case 15: cydui_spacer_fill(&g_ui); break;
        case 16: cydui_next_width(&g_ui, small(&r, 120)); break;
        case 17: cydui_next_height(&g_ui, small(&r, 80)); break;
        case 18: cydui_next_fill(&g_ui); break;
        case 19: cydui_next_stretch(&g_ui); break;
        case 20: cydui_next_disabled(&g_ui); break;
        case 21: cydui_push_id_int(&g_ui, i16(&r)); break;
        case 22: cydui_pop_id(&g_ui); break;
        case 23: cydui_push_id_str(&g_ui, make_text(&r, tmp, sizeof(tmp))); break;
        case 24: {
            int16_t w = small(&r, 80), h = small(&r, 60);
            cydui_alloc_rect(&g_ui, w, h);
            break;
        }
        case 25: { /* a touch event, at times of a type that does not exist */
            uint8_t t = u8(&r) % 6u;
            cydui_event_type type = t == 0 ? CYDUI_EVENT_DOWN : t == 1 ? CYDUI_EVENT_UP
                                    : t == 5 ? (cydui_event_type)(u8(&r) | 0x40u) : CYDUI_EVENT_MOVE;
            int16_t x = small(&r, 200), y = small(&r, 150);
            cydui_mem_push_event(&g_mem, type, x, y);
            break;
        }
        case 26: { /* the clock, including a jump that wraps it */
            uint8_t k = u8(&r);
            g_mem.now += k == 0xFF ? 0xFFFFF000u : (uint32_t)k * 7u;
            break;
        }
        case 27: /* a frame boundary */
            cydui_ui_end(&g_ui);
            check(&g_ui);
            cydui_ui_begin(&g_ui, 0);
            break;
        case 28: { /* raw interaction on a rectangle, with any flags */
            cydui_rect rc = {small(&r, 100), small(&r, 80), small(&r, 90), small(&r, 70)};
            cydui_id id = cydui_id_str(&g_ui, make_text(&r, tmp, sizeof(tmp)));
            cydui_id_claim(&g_ui, id);
            cydui_interact(&g_ui, id, rc, u8(&r));
            break;
        }
        case 29: {
            cydui_rect rc = {small(&r, 100), small(&r, 80), small(&r, 120), small(&r, 90)};
            cydui_set_clip(&g_ui.ctx, rc);
            break;
        }
        case 30: cydui_reset_clip(&g_ui.ctx); break;
        case 31: { /* hammer the state table with ids that all want an entry */
            cydui_id id = cydui_id_str(&g_ui, make_text(&r, tmp, sizeof(tmp)));
            cydui_id active = g_ui.active;
            bool had_active = (active.h || active.c) && cydui_state_get(&g_ui, active, false) != NULL;
            cydui_state *st = cydui_state_get(&g_ui, id, (u8(&r) & 1u) != 0);

            if (st) {
                st->a = (int32_t)u8(&r);
            }
            /* The oracle no sanitizer can be: a press must not lose its state to a newcomer. */
            if (had_active && cydui_state_get(&g_ui, active, false) == NULL) {
                abort();
            }
            break;
        }
        case 32: (void)cydui_layout_avail(&g_ui); break;
        case 33: { /* a burst of identical moves, like a flood */
            int16_t x = small(&r, 160), y = small(&r, 120);
            for (i = 0; i < 80; i++) {
                cydui_mem_push_event(&g_mem, CYDUI_EVENT_MOVE, (int16_t)(x + i % 5), y);
            }
            break;
        }
        case 34: { /* a burst that overflows the queue: press/release pairs, or releases only */
            uint8_t n = (uint8_t)(u8(&r) % 40u);
            bool only_up = (u8(&r) & 1u) != 0; /* orphan releases are cheap: many fit in one frame's budget */
            int16_t x = small(&r, 160), y = small(&r, 120);
            for (i = 0; i < n; i++) {
                cydui_mem_push_event(&g_mem, only_up || (i & 1) ? CYDUI_EVENT_UP : CYDUI_EVENT_DOWN, x, y);
            }
            break;
        }
        default: { /* a whole frame of widgets of one kind, to reach the per-frame limit */
            uint8_t n = u8(&r);
            for (i = 0; i < (n & 1 ? 700 : 40); i++) {
                cydui_push_id_int(&g_ui, i);
                cydui_button(&g_ui, "x");
                cydui_pop_id(&g_ui);
            }
            break;
        }
        }
    }
    cydui_ui_end(&g_ui);
    check(&g_ui);
    for (i = 0; i < 3; i++) { /* a few more frames to let queued events drain and layouts settle */
        cydui_ui_begin(&g_ui, 0);
        cydui_button(&g_ui, "tail");
        cydui_ui_end(&g_ui);
        check(&g_ui);
    }
    return cydui_mem_hash(&g_mem) ^ ((uint64_t)vals[0] | (uint64_t)vals[1] << 1 | (uint64_t)vals[2] << 2 |
                                     (uint64_t)vals[3] << 3);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint64_t a = run(data, size, 8);
    uint64_t b = run(data, size, 3);

    if (a != b) {
        abort(); /* the screen depended on the strip height, or on something other than the input */
    }
    return 0;
}
