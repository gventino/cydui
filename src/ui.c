#include "cydui/ui.h"

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

static cydui_id id_mix(cydui_id parent, const void *bytes, size_t n)
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

/* ---- style ---- */

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
    return s;
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

uint32_t cydui_ui_init(cydui_ui *ui, const cydui_ui_config *cfg)
{
    cydui_arena a;
    uint32_t err;

    memset(ui, 0, sizeof(*ui));
    if (!cfg || !cfg->style || !cydui_font_fallback(cfg->style->font)) {
        return CYDUI_ERR_BAD_ARG;
    }
    err = cydui_init(&ui->ctx, &cfg->core);
    if (err) {
        return err;
    }
    ui->style = *cfg->style;

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
    ui->thread_id = cfg->thread_id;
    ui->thread = cfg->thread_id ? cfg->thread_id() : 0;
    return 0;
}

/* CT-11: in debug builds, flag a call from the wrong task. The call itself still goes through. */
static void check_thread(cydui_ui *ui)
{
#if CYDUI_DEBUG_THREAD
    if (ui->thread_id && ui->thread_id() != ui->thread) {
        ui->ctx.errors |= CYDUI_ERR_THREAD;
    }
#else
    (void)ui;
#endif
}

/* ---- frame ---- */

bool cydui_ui_begin(cydui_ui *ui, cydui_color clear)
{
    check_thread(ui);
    cydui_begin_frame(&ui->ctx, clear);
    ui->widgets = 0;
    ui->id_depth = 0;
    if (ui->idset_cap) {
        memset(ui->idset, 0, (size_t)ui->idset_cap * sizeof(uint32_t));
    }
    return true;
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
    check_thread(ui);
    ui->id_depth = 0;
    if (ui->state_cap) {
        sweep_state(ui);
    }
    cydui_end_frame(&ui->ctx);
    ui->frame++;
    return false;
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
    return id_mix(id_top(ui), label ? label : "", label ? label_len(label) : 0);
}

static void push_id(cydui_ui *ui, cydui_id id)
{
    check_thread(ui);
    if (ui->id_depth >= CYDUI_MAX_ID_DEPTH) {
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        return;
    }
    ui->id_stack[ui->id_depth++] = id;
}

void cydui_push_id_str(cydui_ui *ui, const char *s)
{
    push_id(ui, id_mix(id_top(ui), s ? s : "", s ? label_len(s) : 0));
}

void cydui_push_id_int(cydui_ui *ui, int32_t v)
{
    uint8_t b[4]; /* explicit little endian, so the id does not depend on the machine */
    uint32_t u = (uint32_t)v;
    b[0] = (uint8_t)u;
    b[1] = (uint8_t)(u >> 8);
    b[2] = (uint8_t)(u >> 16);
    b[3] = (uint8_t)(u >> 24);
    push_id(ui, id_mix(id_top(ui), b, sizeof(b)));
}

void cydui_push_id_ptr(cydui_ui *ui, const void *p)
{
    uint8_t b[8];
    uint64_t u = (uint64_t)(uintptr_t)p;
    size_t i;
    for (i = 0; i < sizeof(b); i++) {
        b[i] = (uint8_t)(u >> (8u * i));
    }
    push_id(ui, id_mix(id_top(ui), b, sizeof(b)));
}

void cydui_pop_id(cydui_ui *ui)
{
    check_thread(ui);
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

cydui_state *cydui_state_get(cydui_ui *ui, cydui_id id, bool create)
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
    if (free_slot == ui->state_cap) {
        free_slot = oldest; /* full: replace the entry unused for the longest time */
    }
    if (free_slot == ui->state_cap) {
        return NULL;
    }
    s = &ui->state[free_slot];
    memset(s, 0, sizeof(*s));
    s->id = id;
    s->used = 1;
    s->last_frame = ui->frame;
    return s;
}
