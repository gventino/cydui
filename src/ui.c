#include "ui_internal.h"

#include <string.h>

#define FNV_OFFSET 0x811C9DC5u
#define FNV_PRIME 0x01000193u

/* ---- hashing: the id uses FNV-1a, the check a different (sdbm style) function ---- */

static cydui_id id_root(void)
{
    cydui_id id;
    id.h = FNV_OFFSET;
    id.c = 0x1D;
    return id;
}

cydui_id cydui__id_mix(cydui_id parent, const void *bytes, size_t n)
{
    const uint8_t *p = (const uint8_t *)bytes;
    uint32_t h = parent.h, c = parent.c;
    size_t i;
    cydui_id id;

    for (i = 0; i < n; i++) {
        h = (h ^ p[i]) * FNV_PRIME;
        c = p[i] + (c << 6) + (c << 16) - c;
    }
    id.h = h;
    id.c = (uint16_t)(c ^ (c >> 16));
    if (id.h == 0 && id.c == 0) { /* all zero means "no widget" */
        id.c = 1;
    }
    return id;
}

static cydui_id id_top(const cydui_ui *ui)
{
    return ui->id_depth ? ui->id_stack[ui->id_depth - 1] : id_root();
}

/* Length of a NUL-terminated label, never reading more than CYDUI_MAX_STR bytes. */
static size_t label_len(const char *s)
{
    size_t n = 0;
    while (n < CYDUI_MAX_STR && s[n] != '\0') {
        n++;
    }
    return n;
}

/* ---- setup ---- */

static uint32_t floor_pow2(size_t n)
{
    uint32_t p = 1;
    if (n == 0) {
        return 0;
    }
    while ((size_t)p * 2u <= n && p < 0x40000000u) {
        p *= 2u;
    }
    return p;
}

/* The post-effect of the theme in use, with the ui as its user (or none). */
void cydui__install_post_effect(cydui_ui *ui)
{
    const cydui_post_effect *p = ui->theme->post;

    if (p && p->apply) {
        cydui_post_effect fx = *p;
        fx.user = ui;
        cydui_set_post_effect(&ui->ctx, &fx);
    } else {
        cydui_set_post_effect(&ui->ctx, NULL);
    }
}

uint32_t cydui_ui_init(cydui_ui *ui, const cydui_ui_config *cfg)
{
    cydui_arena a;
    uint32_t err;

    memset(ui, 0, sizeof(*ui));
    if (!cfg || cydui_theme_validate(cfg->theme) != CYDUI_THEME_OK) {
        return CYDUI_ERR_BAD_ARG;
    }
    err = cydui_init(&ui->ctx, &cfg->core);
    if (err) {
        return err;
    }
    ui->theme = cfg->theme;
    ui->fx_level = CYDUI_FX_DEFAULT;
    ui->theme_fresh = true;
    cydui__install_post_effect(ui);

    cydui_arena_init(&a, cfg->state_mem, cfg->state_mem_size);
    ui->state_cap = (uint32_t)(a.cap / sizeof(cydui_state));
    if (ui->state_cap) {
        ui->state = (cydui_state *)cydui_arena_alloc(&a, ui->state_cap * sizeof(cydui_state), 4);
        if (!ui->state) { /* alignment padding ate one entry */
            ui->state_cap--;
            cydui_arena_reset(&a);
            ui->state = ui->state_cap ? (cydui_state *)cydui_arena_alloc(
                                            &a, ui->state_cap * sizeof(cydui_state), 4)
                                      : NULL;
            if (!ui->state) {
                ui->state_cap = 0;
            }
        }
        if (ui->state) { /* memory that was used before (a second init over the same storage) must not hand old state to the new ui */
            memset(ui->state, 0, ui->state_cap * sizeof(cydui_state));
        }
    }

    cydui_arena_init(&a, cfg->idset_mem, cfg->idset_mem_size);
    ui->idset_cap = floor_pow2(a.cap / sizeof(uint32_t));
    if (ui->idset_cap) {
        ui->idset = (uint32_t *)cydui_arena_alloc(&a, ui->idset_cap * sizeof(uint32_t), 4);
        if (!ui->idset) {
            ui->idset_cap = 0;
        }
    }

    ui->tap_slop = cfg->tap_slop > 0 ? (uint16_t)cfg->tap_slop : 10u;
    ui->long_press_ms = cfg->long_press_ms ? cfg->long_press_ms : 500u;
    ui->frame_budget_ms = cfg->frame_budget_ms;
    ui->thread_id = cfg->thread_id;
    ui->thread = cfg->thread_id ? cfg->thread_id() : 0;
    return 0;
}

/* CT-11: in debug builds, flag a call from the wrong task. The call itself still goes through. */
void cydui__check_thread(cydui_ui *ui)
{
#if CYDUI_DEBUG_THREAD
    if (ui->thread_id && ui->thread_id() != ui->thread) {
        ui->ctx.errors |= CYDUI_ERR_THREAD;
    }
#else
    (void)ui;
#endif
}

/* ---- input ---- */

static int16_t clamp16(int v, int lo, int hi)
{
    return (int16_t)(v < lo ? lo : (v > hi ? hi : v));
}

static bool known_event(cydui_event_type t)
{
    return t == CYDUI_EVENT_DOWN || t == CYDUI_EVENT_MOVE || t == CYDUI_EVENT_UP;
}

/* Typed characters and keys wait for the focused widget in this frame; when there are too many the oldest go. */
static void k_push(cydui_input *in, cydui_event ev)
{
    if (in->k_len >= CYDUI_KEY_QUEUE) {
        memmove(&in->keys[0], &in->keys[1], (size_t)(CYDUI_KEY_QUEUE - 1u) * sizeof(in->keys[0]));
        in->k_len--;
    }
    in->keys[in->k_len++] = ev;
}

/* Adds an event to the queue. MOVEs merge; when full, the oldest MOVE goes first, then the oldest event. */
static void q_push(cydui_ui *ui, cydui_event ev)
{
    cydui_input *in = &ui->in;
    const cydui_backend *be = ui->ctx.backend;

    ev.x = clamp16(ev.x, 0, be->width - 1);
    ev.y = clamp16(ev.y, 0, be->height - 1);
    if (in->q_len > 0 && ev.type == CYDUI_EVENT_MOVE &&
        in->queue[in->q_len - 1].type == CYDUI_EVENT_MOVE) {
        in->queue[in->q_len - 1] = ev;
        return;
    }
    if (in->q_len >= CYDUI_EVENT_QUEUE) {
        uint8_t i, drop = 0;
        for (i = 0; i < in->q_len; i++) {
            if (in->queue[i].type == CYDUI_EVENT_MOVE) {
                drop = i;
                break;
            }
        }
        memmove(&in->queue[drop], &in->queue[drop + 1], (size_t)(in->q_len - drop - 1u) * sizeof(in->queue[0]));
        in->q_len--;
    }
    in->queue[in->q_len++] = ev;
}

static void q_pop(cydui_input *in)
{
    memmove(&in->queue[0], &in->queue[1], (size_t)(in->q_len - 1u) * sizeof(in->queue[0]));
    in->q_len--;
}

static void poll_backend(cydui_ui *ui)
{
    const cydui_backend *be = ui->ctx.backend;
    int i;

    if (!be->poll_event) {
        return;
    }
    for (i = 0; i < CYDUI_POLL_PER_FRAME; i++) {
        cydui_event ev;
        if (!be->poll_event(be->user, &ev)) {
            break;
        }
        if (ev.type == CYDUI_EVENT_QUIT) {
            ui->in.quit = true;
        } else if (known_event(ev.type)) {
            q_push(ui, ev);
        } else if (ev.type == CYDUI_EVENT_CHAR || ev.type == CYDUI_EVENT_KEY) {
            k_push(&ui->in, ev);
        }
    }
}

/* True if (x, y) is farther from where the press started than the tap slop. */
static bool beyond_slop(const cydui_ui *ui, int x, int y)
{
    long dx = (long)x - ui->in.press_x, dy = (long)y - ui->in.press_y;
    return dx * dx + dy * dy > (long)ui->tap_slop * ui->tap_slop;
}

/* A press held still for long enough becomes a long press, once. Unsigned subtraction survives the clock wrapping. */
static void check_long(cydui_ui *ui)
{
    cydui_input *in = &ui->in;
    if (in->down && in->has_clock && !in->long_fired && !in->moved_far &&
        (uint32_t)(in->now - in->press_ms) >= ui->long_press_ms) {
        in->long_fired = true;
        in->frame_long = true;
    }
}

/* Takes events off the queue and updates the pointer. At most one press edge and one release edge per frame. */
static void process_events(cydui_ui *ui)
{
    cydui_input *in = &ui->in;
    unsigned n = 0;

    in->frame_down = in->frame_up = in->frame_long = in->claimed = false;
    in->frame_x = in->x;
    in->frame_y = in->y;

    while (in->q_len > 0 && n < CYDUI_EVENTS_PER_FRAME) {
        cydui_event ev = in->queue[0];

        if (ev.type == CYDUI_EVENT_DOWN) {
            if (in->frame_down || in->frame_up) {
                break; /* a second press edge waits for the next frame */
            }
            if (in->down) { /* a press that never ended: cancel it, and start the new one next frame */
                in->moved_far = true;
                in->frame_up = true;
                in->up_x = in->x;
                in->up_y = in->y;
                in->down = false;
                break;
            }
            q_pop(in);
            in->down = true;
            in->x = in->press_x = in->frame_x = ev.x; /* the travel of this frame counts from the press */
            in->y = in->press_y = in->frame_y = ev.y;
            in->press_ms = in->now;
            in->long_fired = false;
            in->moved_far = false;
            in->frame_down = true;
        } else if (ev.type == CYDUI_EVENT_MOVE) {
            q_pop(in);
            if (in->down) {
                in->x = ev.x;
                in->y = ev.y;
                in->moved_far = in->moved_far || beyond_slop(ui, ev.x, ev.y);
            }
        } else { /* UP */
            q_pop(in);
            if (in->down) {
                in->x = ev.x;
                in->y = ev.y;
                in->moved_far = in->moved_far || beyond_slop(ui, ev.x, ev.y);
                check_long(ui);
                in->frame_up = true;
                in->up_x = ev.x;
                in->up_y = ev.y;
                in->down = false;
            } /* an UP without a press is ignored */
        }
        n++;
    }
    in->last_processed = (uint8_t)n;
    check_long(ui);
}

static bool hit(const cydui_ui *ui, cydui_rect r, int x, int y)
{
    cydui_rect c = cydui_rect_intersect(r, ui->ctx.clip);
    return x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h;
}

cydui_response cydui_interact(cydui_ui *ui, cydui_id id, cydui_rect r, unsigned flags)
{
    cydui_response resp;
    cydui_input *in = &ui->in;

    memset(&resp, 0, sizeof(resp));
    cydui__check_thread(ui);
    if ((flags & CYDUI_DISABLED) || (ui->popup_prev && !ui->in_popup)) { /* under a popup nothing reacts */
        return resp;
    }
    if (in->frame_down && !in->claimed && ui->active.h == 0 && ui->active.c == 0 &&
        hit(ui, r, in->press_x, in->press_y) &&
        (ui->in_kb || !cydui__keyboard_hit(ui, in->press_x, in->press_y))) { /* under the keyboard nothing is pressed */
        ui->active = id;
        ui->active_capture = (flags & CYDUI_CAPTURE) != 0;
        in->claimed = true; /* a press has one owner, even if it ends in this same frame */
    }
    if (!cydui_id_eq(ui->active, id)) {
        return resp;
    }
    resp.x = in->x;
    resp.y = in->y;
    resp.pressed = (in->down || in->frame_up) && hit(ui, r, in->x, in->y);
    resp.dragging = in->moved_far;
    resp.long_pressed = in->frame_long;
    if (flags & CYDUI_SENSE_DRAG) {
        resp.dx = (int16_t)(in->x - in->frame_x);
        resp.dy = (int16_t)(in->y - in->frame_y);
    }
    if (in->frame_up) {
        resp.released = true;
        resp.clicked = (flags & CYDUI_SENSE_CLICK) && hit(ui, r, in->up_x, in->up_y) &&
                       !in->moved_far && !in->long_fired;
        memset(&ui->active, 0, sizeof(ui->active));
    }
    return resp;
}

/* ---- frame ---- */

bool cydui_ui_begin(cydui_ui *ui, cydui_color clear)
{
    const cydui_backend *be = ui->ctx.backend;

    cydui__check_thread(ui);
    ui->in.has_clock = be->now_ms != NULL;
    ui->in.now = ui->in.has_clock ? be->now_ms(be->user) : 0;
    ui->frame_start = ui->in.now;
    poll_backend(ui);
    process_events(ui);
    cydui_begin_frame(&ui->ctx, clear);
    ui->widgets = 0;
    ui->tweens_now = 0;
    if (ui->theme_fresh) {
        ui->theme_since = ui->in.now;
        ui->theme_fresh = false;
    }
    ui->id_depth = 0;
    cydui__layout_begin_frame(ui);
    if (ui->idset_cap) {
        memset(ui->idset, 0, (size_t)ui->idset_cap * sizeof(uint32_t));
    }
    return !ui->in.quit;
}

/* Frees up to a few expired entries per frame, so the work stays bounded however big the table is. */
static void sweep_state(cydui_ui *ui)
{
    uint32_t i, n = ui->state_cap < 8u ? ui->state_cap : 8u;

    for (i = 0; i < n; i++) {
        cydui_state *s = &ui->state[ui->sweep];
        if (s->used && ui->frame - s->last_frame > CYDUI_STATE_TTL) {
            s->used = 0;
        }
        ui->sweep = (ui->sweep + 1u) % ui->state_cap;
    }
}

bool cydui_ui_end(cydui_ui *ui)
{
    cydui_event kb_out[8];
    unsigned kb_n = 0, i;

    cydui__check_thread(ui);
    ui->id_depth = 0;
    cydui__layout_end_frame(ui);
    if (ui->state_cap) {
        sweep_state(ui);
    }
    if (ui->in_popup) { /* a popup that was not ended */
        ui->in_popup = false;
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
    }
    if (ui->popup_now && ui->popup_start < ui->popup_end) { /* the popup is above everything the frame drew */
        cydui__cmd_move_tail(&ui->ctx, ui->popup_start, ui->popup_end);
    }
    ui->popup_prev = ui->popup_now;
    ui->popup_now = false;
    ui->popup_start = ui->popup_end = 0;

    /* a press on the keyboard keeps the focus; a press anywhere else, or the focused widget gone, drops it */
    if (ui->in.frame_down && cydui__keyboard_hit(ui, ui->in.press_x, ui->in.press_y)) {
        ui->focus_claimed = true;
    }
    if ((ui->in.frame_down && !ui->focus_claimed) || !ui->focus_seen) {
        memset(&ui->focus, 0, sizeof(ui->focus));
    }
    if (cydui_has_focus(ui) && !ui->kb_off) { /* above everything, and last: its keys have the press */
        kb_n = cydui__keyboard_run(ui, kb_out);
        ui->kb_prev = true;
    } else {
        ui->kb_prev = false;
        ui->kb_page = 0;
        ui->kb_shift = false;
    }
    if (!ui->in.down) { /* nobody can still own a press that is over */
        memset(&ui->active, 0, sizeof(ui->active));
    }
    ui->focus_claimed = ui->focus_seen = false;
    ui->in.k_len = 0; /* what the focused widget did not take is not kept for later */
    for (i = 0; i < kb_n; i++) { /* what the keys of the keyboard typed is for the field in the next frame */
        k_push(&ui->in, kb_out[i]);
    }
    ui->peak_cmds = ui->ctx.cmd_len > ui->peak_cmds ? ui->ctx.cmd_len : ui->peak_cmds;
    ui->peak_text = ui->ctx.text_arena.used > ui->peak_text ? (uint32_t)ui->ctx.text_arena.used : ui->peak_text;
    ui->peak_tweens = ui->tweens_now > ui->peak_tweens ? ui->tweens_now : ui->peak_tweens;
    ui->peak_widgets = ui->widgets > ui->peak_widgets ? ui->widgets : ui->peak_widgets;
    cydui_end_frame(&ui->ctx);
    ui->frame++;
    {
        bool more = ui->in.down || ui->in.q_len > 0 || ui->in.k_len > 0 || ui->relayout || ui->tweens_now > 0;
        ui->relayout = false;
        return more;
    }
}

void cydui_ui_stats_get(const cydui_ui *ui, cydui_ui_stats *out)
{
    uint32_t i, used = 0;

    if (!ui || !out) {
        return;
    }
    for (i = 0; i < ui->state_cap; i++) {
        used += ui->state[i].used ? 1u : 0u;
    }
    out->cmds_peak = ui->peak_cmds;
    out->cmds_cap = ui->ctx.cmd_cap;
    out->text_peak = ui->peak_text;
    out->text_cap = (uint32_t)ui->ctx.text_arena.cap;
    out->state_used = used;
    out->state_cap = ui->state_cap;
    out->tweens_peak = ui->peak_tweens;
    out->tweens_cap = CYDUI_MAX_TWEENS;
    out->widgets_peak = ui->peak_widgets;
}

void cydui_ui_stats_reset(cydui_ui *ui)
{
    if (ui) {
        ui->peak_cmds = ui->peak_text = ui->peak_tweens = ui->peak_widgets = 0;
    }
}

void cydui_focus_set(cydui_ui *ui, const char *label)
{
    cydui__check_thread(ui);
    if (label) {
        ui->focus = cydui_id_str(ui, label);
        ui->focus_seen = true; /* it is called later in this frame, or the focus is dropped at the end of the next */
    } else {
        memset(&ui->focus, 0, sizeof(ui->focus));
    }
}

void cydui_focus_clear(cydui_ui *ui)
{
    cydui__check_thread(ui);
    memset(&ui->focus, 0, sizeof(ui->focus));
}

bool cydui_has_focus(const cydui_ui *ui)
{
    return ui->focus.h != 0 || ui->focus.c != 0;
}

uint32_t cydui_ui_errors(const cydui_ui *ui)
{
    return cydui_errors(&ui->ctx);
}

void cydui_ui_clear_errors(cydui_ui *ui)
{
    cydui_clear_errors(&ui->ctx);
}

uint32_t cydui_ui_frame(const cydui_ui *ui)
{
    return ui->frame;
}

/* ---- ids ---- */

cydui_id cydui_id_str(const cydui_ui *ui, const char *label)
{
    return cydui__id_mix(id_top(ui), label ? label : "", label ? label_len(label) : 0);
}

static void push_id(cydui_ui *ui, cydui_id id)
{
    cydui__check_thread(ui);
    if (ui->id_depth >= CYDUI_MAX_ID_DEPTH) {
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        return;
    }
    ui->id_stack[ui->id_depth++] = id;
}

void cydui_push_id_str(cydui_ui *ui, const char *s)
{
    push_id(ui, cydui__id_mix(id_top(ui), s ? s : "", s ? label_len(s) : 0));
}

void cydui_push_id_int(cydui_ui *ui, int32_t v)
{
    uint8_t b[4]; /* explicit little endian, so the id does not depend on the machine */
    uint32_t u = (uint32_t)v;
    b[0] = (uint8_t)u;
    b[1] = (uint8_t)(u >> 8);
    b[2] = (uint8_t)(u >> 16);
    b[3] = (uint8_t)(u >> 24);
    push_id(ui, cydui__id_mix(id_top(ui), b, sizeof(b)));
}

void cydui_push_id_ptr(cydui_ui *ui, const void *p)
{
    uint8_t b[8];
    uint64_t u = (uint64_t)(uintptr_t)p;
    size_t i;
    for (i = 0; i < sizeof(b); i++) {
        b[i] = (uint8_t)(u >> (8u * i));
    }
    push_id(ui, cydui__id_mix(id_top(ui), b, sizeof(b)));
}

void cydui_pop_id(cydui_ui *ui)
{
    cydui__check_thread(ui);
    if (ui->id_depth == 0) {
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        return;
    }
    ui->id_depth--;
}

bool cydui_id_claim(cydui_ui *ui, cydui_id id)
{
#if CYDUI_DEBUG_IDS
    uint32_t key, mask, i, n;

    if (!ui->idset_cap) {
        return true;
    }
    key = id.h ? id.h : 1u; /* 0 marks an empty slot */
    mask = ui->idset_cap - 1u;
    i = key & mask;
    for (n = 0; n < ui->idset_cap; n++, i = (i + 1u) & mask) {
        if (ui->idset[i] == key) {
            ui->ctx.errors |= CYDUI_ERR_ID_COLLISION;
            return false;
        }
        if (ui->idset[i] == 0) {
            ui->idset[i] = key;
            return true;
        }
    }
    return true; /* the set is full: stop checking rather than report something false */
#else
    (void)ui;
    (void)id;
    return true;
#endif
}

/* ---- state table ---- */

/* `evict`: when the table is full, an entry is replaced (the one unused for the longest time); without it nothing is. */
static cydui_state *state_get(cydui_ui *ui, cydui_id id, bool create, bool evict)
{
    uint32_t i, free_slot = ui->state_cap, oldest = ui->state_cap;
    cydui_state *s;

    for (i = 0; i < ui->state_cap; i++) {
        s = &ui->state[i];
        if (!s->used) {
            if (free_slot == ui->state_cap) {
                free_slot = i;
            }
            continue;
        }
        if (cydui_id_eq(s->id, id)) {
            s->last_frame = ui->frame;
            return s;
        }
        if (ui->frame - s->last_frame > CYDUI_STATE_TTL && free_slot == ui->state_cap) {
            free_slot = i; /* expired: reusable right now */
        }
        if (!cydui_id_eq(s->id, ui->active) &&
            (oldest == ui->state_cap ||
             ui->frame - s->last_frame > ui->frame - ui->state[oldest].last_frame)) {
            oldest = i;
        }
    }
    if (!create) {
        return NULL;
    }
    if (free_slot == ui->state_cap && evict) {
        free_slot = oldest; /* full: replace the entry unused for the longest time */
        if (free_slot != ui->state_cap && ui->frame - ui->state[free_slot].last_frame <= 1u) {
            ui->ctx.errors |= CYDUI_ERR_STATE_FULL; /* ... and it was still in use: its state is lost */
        }
    }
    if (free_slot == ui->state_cap) {
        if (ui->state_cap && evict) {
            ui->ctx.errors |= CYDUI_ERR_STATE_FULL; /* nothing could be replaced (a table of no entries is a choice, a tween that finds no room just jumps) */
        }
        return NULL;
    }
    s = &ui->state[free_slot];
    memset(s, 0, sizeof(*s));
    s->id = id;
    s->used = 1;
    s->last_frame = ui->frame;
    return s;
}

cydui_state *cydui_state_get(cydui_ui *ui, cydui_id id, bool create)
{
    return state_get(ui, id, create, true);
}

cydui_state *cydui__state_get_keep(cydui_ui *ui, cydui_id id)
{
    return state_get(ui, id, true, false);
}
