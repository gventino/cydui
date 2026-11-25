/*
 * Layout: containers place their children one after the other. A child's position is final the
 * moment it is placed, so widgets can be hit-tested in the same frame.
 *
 * `fill` children share the space left along the main axis. How many there are, and how much the
 * other children take, is only known at the end of the container, so it is remembered from the
 * previous frame in the state table (this is the one frame of delay). When it changes the UI asks
 * for another frame.
 */

#include <string.h>

#include "ui_internal.h"

/* A size: never negative. */
static int16_t i16(int v)
{
    return (int16_t)(v < 0 ? 0 : (v > 32767 ? 32767 : v));
}

/* A position: inside a scrolled area it is often negative. */
static int16_t s16(int v)
{
    return (int16_t)(v < -32768 ? -32768 : (v > 32767 ? 32767 : v));
}

/* ---- modifiers ---- */

static void mods_reset(cydui_mods *m)
{
    m->w = m->h = -1;
    m->fill = m->stretch = m->disabled = false;
}

void cydui_next_width(cydui_ui *ui, int px)
{
    ui->mods.w = i16(px);
}

void cydui_next_height(cydui_ui *ui, int px)
{
    ui->mods.h = i16(px);
}

void cydui_next_fill(cydui_ui *ui)
{
    ui->mods.fill = true;
}

void cydui_next_stretch(cydui_ui *ui)
{
    ui->mods.stretch = true;
}

void cydui_next_disabled(cydui_ui *ui)
{
    ui->mods.disabled = true;
}

cydui_mods cydui_take_mods(cydui_ui *ui)
{
    cydui_mods m = ui->mods;
    mods_reset(&ui->mods);
    return m;
}

/* ---- geometry of one container ---- */

static cydui_region *top(cydui_ui *ui)
{
    return &ui->regions[ui->depth];
}

static bool main_is_x(const cydui_region *r)
{
    return r->kind == CYDUI_LAYOUT_ROW;
}

/* Where the next child of `r` starts. The gap goes before a child, so it follows set_spacing at once. */
static void origin(const cydui_region *r, int *x, int *y)
{
    int gap = r->count > 0 ? r->spacing : 0;

    if (r->kind == CYDUI_LAYOUT_ROW) {
        *x = r->cx + gap;
        *y = r->y0;
    } else if (r->kind == CYDUI_LAYOUT_GRID) {
        *x = r->x0 + (int)(r->count % r->cols) * (r->cell_w + r->spacing);
        *y = r->cy;
    } else {
        *x = r->x0;
        *y = r->cy + gap;
    }
}

/* The room `r` has left, from the point where its next child starts. */
static void room(const cydui_region *r, int *w, int *h)
{
    int x, y, rw, rh;

    origin(r, &x, &y);
    if (r->kind == CYDUI_LAYOUT_ROW) {
        rw = r->avail_w - (x - r->x0);
        rh = r->avail_h;
    } else if (r->kind == CYDUI_LAYOUT_GRID) {
        rw = r->cell_w;
        rh = r->avail_h - (y - r->y0);
    } else {
        rw = r->avail_w;
        rh = r->avail_h - (y - r->y0);
    }
    *w = rw < 0 ? 0 : rw;
    *h = rh < 0 ? 0 : rh;
}

/* One `fill` child's share of the main axis. The others' share is known from the last frame. */
static int fill_share(const cydui_region *r)
{
    int main_avail, total, base, rem;

    if (r->kind == CYDUI_LAYOUT_GRID) {
        return r->cell_w; /* a grid has no main axis to share: fill means the whole cell */
    }
    main_avail = main_is_x(r) ? r->avail_w : r->avail_h;
    if (r->fills_prev == 0) { /* nothing known yet: take what is left */
        int w, h;
        room(r, &w, &h);
        return main_is_x(r) ? w : h;
    }
    total = main_avail - r->fixed_prev;
    if (total < 0) {
        total = 0;
    }
    base = total / r->fills_prev;
    rem = total % r->fills_prev;
    return base + ((int)r->fill_idx < rem ? 1 : 0);
}

/* Resolves the size of a child from its content size and the modifiers. */
static void resolve_size(const cydui_region *r, const cydui_mods *m, int *w, int *h)
{
    int rw, rh;

    if (m->w >= 0) *w = m->w;
    if (m->h >= 0) *h = m->h;
    if (m->fill) {
        if (main_is_x(r) || r->kind == CYDUI_LAYOUT_GRID) {
            if (m->w < 0) *w = fill_share(r);
        } else if (m->h < 0) {
            *h = fill_share(r);
        }
    }
    if (m->stretch) {
        room(r, &rw, &rh);
        if (main_is_x(r)) {
            if (m->h < 0) *h = r->avail_h;
        } else if (m->w < 0) {
            *w = rw;
        }
    }
    *w = i16(*w);
    *h = i16(*h);
}

/* Records that `rect` was placed in `r`: moves the cursor and keeps the totals. */
static void commit(cydui_region *r, cydui_rect rect, const cydui_mods *m)
{
    int ext_w = rect.x + rect.w - r->x0, ext_h = rect.y + rect.h - r->y0;
    int main_size = main_is_x(r) ? rect.w : rect.h;

    if (ext_w > r->ext_w) r->ext_w = i16(ext_w);
    if (ext_h > r->ext_h) r->ext_h = i16(ext_h);

    if (r->kind != CYDUI_LAYOUT_GRID) {
        if (r->count > 0) {
            r->fixed_now = i16(r->fixed_now + r->spacing);
        }
        if (m->fill) {
            r->fills_now++;
            r->fill_idx++;
        } else {
            r->fixed_now = i16(r->fixed_now + main_size);
        }
    }

    if (r->kind == CYDUI_LAYOUT_ROW) {
        r->cx = s16(rect.x + rect.w);
    } else if (r->kind == CYDUI_LAYOUT_GRID) {
        if (rect.h > r->row_h) r->row_h = rect.h;
        if ((uint16_t)(r->count + 1u) % r->cols == 0) { /* the row is complete */
            r->cy = s16(r->cy + r->row_h + r->spacing);
            r->row_h = 0;
        }
    } else {
        r->cy = s16(rect.y + rect.h);
    }
    r->count++;
}

/*
 * How far a child of size w x h is moved from where the cursor puts it, by the container's
 * alignment. The sizes are the previous frame's; nothing is negative.
 */
static void align_offset(const cydui_region *r, int w, int h, int *dx, int *dy)
{
    bool row = main_is_x(r);
    bool root = r->kind == CYDUI_LAYOUT_ROOT; /* always a column: its room is known from the first frame */
    int total, off;

    *dx = *dy = 0;
    if (r->kind == CYDUI_LAYOUT_GRID) {
        return;
    }
    if (r->place_cross != CYDUI_PLACE_START) {
        if (row) {
            total = r->fix_h >= 0 ? r->fix_h : r->prev_h; /* a row has no padding */
            off = total - h;
        } else {
            total = root ? r->avail_w : (r->fix_w >= 0 ? r->fix_w - 2 * r->pad : r->prev_w);
            off = total - w;
        }
        off = off < 0 ? 0 : (r->place_cross == CYDUI_PLACE_CENTER ? off / 2 : off);
        if (row) *dy += off; else *dx += off;
    }
    if (r->place_main != CYDUI_PLACE_START && r->known) { /* before its size is known nothing is moved */
        if (row) {
            total = r->fix_w >= 0 ? r->fix_w : 0;
            off = total - r->prev_w;
        } else {
            total = root ? r->avail_h : (r->fix_h >= 0 ? r->fix_h - 2 * r->pad : 0);
            off = total - r->prev_h;
        }
        off = off < 0 ? 0 : (r->place_main == CYDUI_PLACE_CENTER ? off / 2 : off);
        if (row) *dx += off; else *dy += off;
    }
}

cydui_rect cydui_alloc(cydui_ui *ui, const cydui_mods *m, int w, int h)
{
    cydui_region *r = top(ui);
    cydui_rect rect;
    int x, y, dx, dy;

    cydui__check_thread(ui);
    resolve_size(r, m, &w, &h);
    origin(r, &x, &y);
    rect.x = (int16_t)x;
    rect.y = (int16_t)y;
    rect.w = (int16_t)w;
    rect.h = (int16_t)h;
    commit(r, rect, m); /* the container measures its children where the cursor put them, not where they end up */
    align_offset(r, w, h, &dx, &dy);
    rect.x = s16(x + dx);
    rect.y = s16(y + dy);
    return rect;
}

cydui_rect cydui_alloc_rect(cydui_ui *ui, int w, int h)
{
    cydui_mods m = cydui_take_mods(ui);
    return cydui_alloc(ui, &m, w, h);
}

cydui_rect cydui_layout_avail(const cydui_ui *ui)
{
    const cydui_region *r = &ui->regions[ui->depth];
    cydui_rect out;
    int x, y, w, h;

    origin(r, &x, &y);
    room(r, &w, &h);
    out.x = (int16_t)x;
    out.y = (int16_t)y;
    out.w = (int16_t)w;
    out.h = (int16_t)h;
    return out;
}

/* ---- the state a container keeps between frames (only when it has `fill` children) ---- */

static cydui_id container_id(cydui_region *parent)
{
    uint8_t b[3];
    b[0] = 0x4C; /* 'L': keeps these ids apart from widget ids */
    b[1] = (uint8_t)parent->child_idx;
    b[2] = (uint8_t)(parent->child_idx >> 8);
    parent->child_idx++;
    return cydui__id_mix(parent->sid, b, sizeof(b));
}

static void load_fill_state(cydui_ui *ui, cydui_region *r)
{
    cydui_state *st = cydui_state_get(ui, r->sid, false);

    if (st) {
        r->fills_prev = (uint16_t)st->a;
        r->fixed_prev = (int16_t)st->b;
        r->prev_w = (int16_t)st->c;
        r->prev_h = (int16_t)st->d;
        r->known = 1;
    }
}

static void save_fill_state(cydui_ui *ui, const cydui_region *r)
{
    cydui_state *st;
    bool placed = r->place_cross || r->place_main || r->keep_ext; /* someone needs this container's size */

    if (r->fills_now == 0 && r->fills_prev == 0 && !placed) {
        return;
    }
    st = cydui_state_get(ui, r->sid, r->fills_now > 0 || placed);
    if (!st) {
        return; /* no room in the table: the next frame will not know */
    }
    if (st->a != (int32_t)r->fills_now || st->b != (int32_t)r->fixed_now) {
        ui->relayout = true; /* the shares were computed from stale numbers */
    }
    if (placed && (st->c != (int32_t)r->ext_w || st->d != (int32_t)r->ext_h)) {
        ui->relayout = true; /* so were the places */
    }
    st->a = r->fills_now;
    st->b = r->fixed_now;
    st->c = r->ext_w;
    st->d = r->ext_h;
}

/* ---- containers ---- */

bool cydui__open_region(cydui_ui *ui, cydui_layout_kind kind, int cols, int pad)
{
    cydui_mods m = cydui_take_mods(ui);
    cydui_region *parent, *r;
    int ox, oy, aw, ah, fw = -1, fh = -1;

    cydui__check_thread(ui);
    if (ui->depth >= CYDUI_MAX_LAYOUT_DEPTH) {
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        ui->skipped++;
        return false;
    }
    parent = top(ui);
    origin(parent, &ox, &oy);
    room(parent, &aw, &ah);

    /* the size the container takes in its parent, if it is not just its content */
    if (m.w >= 0) fw = m.w;
    if (m.h >= 0) fh = m.h;
    if (m.fill) {
        int share = fill_share(parent);
        if (main_is_x(parent) || parent->kind == CYDUI_LAYOUT_GRID) {
            if (m.w < 0) fw = share;
        } else if (m.h < 0) {
            fh = share;
        }
    }
    if (m.stretch) {
        if (main_is_x(parent)) {
            if (m.h < 0) fh = parent->avail_h;
        } else if (m.w < 0) {
            fw = aw;
        }
    }
    if (fw >= 0) aw = fw;
    if (fh >= 0) ah = fh;

    ui->depth++;
    r = top(ui);
    memset(r, 0, sizeof(*r));
    r->kind = (uint8_t)kind;
    r->pad = i16(pad);
    r->x0 = s16(ox + r->pad);
    r->y0 = s16(oy + r->pad);
    r->avail_w = i16(aw - 2 * r->pad);
    r->avail_h = i16(ah - 2 * r->pad);
    r->cx = r->x0;
    r->cy = r->y0;
    r->spacing = parent->spacing;
    r->fix_w = (int16_t)fw;
    r->fix_h = (int16_t)fh;
    r->was_fill = m.fill;
    r->cols = kind == CYDUI_LAYOUT_GRID ? (uint16_t)(cols < 1 ? 1 : (cols > 64 ? 64 : cols)) : 1u;
    if (kind == CYDUI_LAYOUT_GRID) {
        int gaps = (int)(r->cols - 1u) * r->spacing;
        r->cell_w = i16((r->avail_w - gaps) / (int)r->cols);
    }
    r->sid = container_id(parent);
    load_fill_state(ui, r);
    r->keep_ext = parent->place_cross != CYDUI_PLACE_START || parent->place_main != CYDUI_PLACE_START;
    if (r->keep_ext && (r->known || (fw >= 0 && fh >= 0))) { /* the parent places this container: by the size it had */
        int dx, dy;
        align_offset(parent, fw >= 0 ? fw : r->prev_w + 2 * r->pad, fh >= 0 ? fh : r->prev_h + 2 * r->pad,
                     &dx, &dy);
        r->x0 = s16(r->x0 + dx);
        r->y0 = s16(r->y0 + dy);
        r->cx = r->x0;
        r->cy = r->y0;
    }
    return true;
}

/* A container that sits exactly where it is told and takes exactly the size it is told (list items). */
bool cydui__open_region_at(cydui_ui *ui, cydui_layout_kind kind, cydui_rect rect)
{
    cydui_region *parent, *r;

    cydui__check_thread(ui);
    if (ui->depth >= CYDUI_MAX_LAYOUT_DEPTH) {
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        ui->skipped++;
        return false;
    }
    parent = top(ui);
    ui->depth++;
    r = top(ui);
    memset(r, 0, sizeof(*r));
    r->kind = (uint8_t)kind;
    r->x0 = r->cx = rect.x;
    r->y0 = r->cy = rect.y;
    r->avail_w = rect.w;
    r->avail_h = rect.h;
    r->spacing = parent->spacing;
    r->fix_w = rect.w;
    r->fix_h = rect.h;
    r->cols = 1;
    r->sid = container_id(parent);
    load_fill_state(ui, r);
    return true;
}

/* The panel of a card that is about to close, drawn now that its size is known and put behind its children. */
static void finish_card(cydui_ui *ui)
{
    const cydui_region *r = &ui->regions[ui->depth];
    cydui_paint_info pi;
    uint32_t mid;

    if (ui->depth == 0 || r->kind != CYDUI_LAYOUT_COLUMN || !r->card) {
        return;
    }
    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_PANEL;
    pi.state = CYDUI_STATE_NORMAL;
    pi.rect.x = s16(r->x0 - r->pad);
    pi.rect.y = s16(r->y0 - r->pad);
    pi.rect.w = r->fix_w >= 0 ? r->fix_w : i16(r->ext_w + 2 * r->pad);
    pi.rect.h = r->fix_h >= 0 ? r->fix_h : i16(r->ext_h + 2 * r->pad);
    mid = ui->ctx.cmd_len;
    cydui_paint(ui, &pi);
    cydui__cmd_move_tail(&ui->ctx, r->cmd_start, mid);
}

static void close_region(cydui_ui *ui)
{
    cydui_region done = *top(ui);
    cydui_region *parent;
    cydui_mods none;
    cydui_rect rect;
    int w, h, x, y;

    save_fill_state(ui, &done);
    ui->depth--;
    parent = top(ui);

    w = done.ext_w + 2 * done.pad;
    h = done.ext_h + 2 * done.pad;
    if (done.fix_w >= 0) w = done.fix_w;
    if (done.fix_h >= 0) h = done.fix_h;
    origin(parent, &x, &y);
    rect.x = (int16_t)x;
    rect.y = (int16_t)y;
    rect.w = i16(w);
    rect.h = i16(h);

    /* a container that asked to fill counts among its parent's sharers: it is the same size either
     * way, but the parent has to know how many of them there are */
    mods_reset(&none);
    none.fill = done.was_fill;
    commit(parent, rect, &none);
}

void cydui__end_kind(cydui_ui *ui, cydui_layout_kind kind)
{
    cydui__check_thread(ui);
    if (ui->skipped > 0) { /* the matching begin was too deep and ignored */
        ui->skipped--;
        return;
    }
    if (ui->depth == 0 || top(ui)->kind != (uint8_t)kind) {
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        return;
    }
    close_region(ui);
}

void cydui_begin_row(cydui_ui *ui) { cydui__open_region(ui, CYDUI_LAYOUT_ROW, 1, 0); }
void cydui_end_row(cydui_ui *ui) { cydui__end_kind(ui, CYDUI_LAYOUT_ROW); }
void cydui_begin_column(cydui_ui *ui) { cydui__open_region(ui, CYDUI_LAYOUT_COLUMN, 1, 0); }
void cydui_end_column(cydui_ui *ui) { cydui__end_kind(ui, CYDUI_LAYOUT_COLUMN); }
void cydui_begin_grid(cydui_ui *ui, int cols) { cydui__open_region(ui, CYDUI_LAYOUT_GRID, cols, 0); }
void cydui_end_grid(cydui_ui *ui) { cydui__end_kind(ui, CYDUI_LAYOUT_GRID); }
void cydui_begin_padding(cydui_ui *ui, int px) { cydui__open_region(ui, CYDUI_LAYOUT_PADDING, 1, px < 0 ? 0 : px); }
void cydui_end_padding(cydui_ui *ui) { cydui__end_kind(ui, CYDUI_LAYOUT_PADDING); }

void cydui_begin_card(cydui_ui *ui)
{
    uint32_t start = ui->ctx.cmd_len;

    if (cydui__open_region(ui, CYDUI_LAYOUT_COLUMN, 1, ui->theme->style.pad_x)) {
        cydui_region *r = top(ui);
        r->card = 1;
        r->cmd_start = start;
    }
}

void cydui_end_card(cydui_ui *ui)
{
    cydui__check_thread(ui);
    if (ui->skipped == 0 && ui->depth > 0) {
        if (top(ui)->kind == CYDUI_LAYOUT_COLUMN && !top(ui)->card) {
            ui->ctx.errors |= CYDUI_ERR_LAYOUT; /* this end belongs to a plain column */
        }
        finish_card(ui);
    }
    cydui__end_kind(ui, CYDUI_LAYOUT_COLUMN);
}

static uint8_t place_of(cydui_place p)
{
    return (uint8_t)(p == CYDUI_PLACE_CENTER || p == CYDUI_PLACE_END ? p : CYDUI_PLACE_START);
}

void cydui_set_cross_align(cydui_ui *ui, cydui_place place)
{
    cydui__check_thread(ui);
    top(ui)->place_cross = place_of(place);
}

void cydui_set_main_align(cydui_ui *ui, cydui_place place)
{
    cydui__check_thread(ui);
    top(ui)->place_main = place_of(place);
}

void cydui_set_spacing(cydui_ui *ui, int px)
{
    cydui__check_thread(ui);
    top(ui)->spacing = i16(px);
}

void cydui_spacer(cydui_ui *ui, int px)
{
    cydui_mods m = cydui_take_mods(ui);
    if (main_is_x(top(ui))) {
        cydui_alloc(ui, &m, px, 0);
    } else {
        cydui_alloc(ui, &m, 0, px);
    }
}

void cydui_spacer_fill(cydui_ui *ui)
{
    cydui_next_fill(ui);
    cydui_spacer(ui, 0);
}

/* ---- frame ---- */

void cydui__layout_begin_frame(cydui_ui *ui)
{
    cydui_region *r = &ui->regions[0];
    int margin = ui->theme->style.spacing;
    const cydui_backend *be = ui->ctx.backend;
    uint8_t tag = 0x52; /* 'R' */

    mods_reset(&ui->mods);
    ui->depth = 0;
    ui->skipped = 0;
    memset(r, 0, sizeof(*r));
    r->kind = CYDUI_LAYOUT_ROOT;
    r->x0 = r->cx = i16(margin);
    r->y0 = r->cy = i16(margin);
    r->avail_w = i16(be->width - 2 * margin);
    r->avail_h = i16(be->height - 2 * margin);
    r->spacing = ui->theme->style.spacing;
    r->cols = 1;
    {
        cydui_id seed;
        seed.h = 0x811C9DC5u;
        seed.c = 0x1D;
        r->sid = cydui__id_mix(seed, &tag, 1);
    }
    load_fill_state(ui, r);
}

void cydui__layout_end_frame(cydui_ui *ui)
{
    while (ui->depth > 0) { /* containers the application left open */
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        finish_card(ui);
        cydui__scroll_finish(ui);
        close_region(ui);
    }
    save_fill_state(ui, &ui->regions[0]);
}
