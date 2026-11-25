/*
 * Scroll areas and virtual lists. The area keeps its state in the state table: `a` is how far the
 * content is scrolled (in 1/256 px, so slow coasting still moves), `b` the speed (px/s, positive when
 * the content goes up), `c` the height of the content as measured at the end of the last frame and
 * `d` the clock at the previous frame. Everything is integers, driven by the backend's clock, so the
 * same events at the same times always give the same offsets.
 */

#include <string.h>

#include "ui_internal.h"

#define FRAC 8
#define OFFSET_CAP ((int64_t)1 << 30)
#define MAX_DT 100u /* a longer gap between two frames counts as this long */
#define SCROLLBAR_W 4
#define SCROLLBAR_INSET 2

static int16_t i16(int64_t v)
{
    return (int16_t)(v < -32000 ? -32000 : (v > 32000 ? 32000 : v));
}

static bool inside(cydui_rect r, int x, int y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static int64_t abs64(int64_t v)
{
    return v < 0 ? -v : v;
}

/*
 * One frame of the area's motion: a catch, a steal of the touch, a drag, or a coast; then the clamp.
 * `bounded` is false while the height of the content is not known yet.
 */
static void step(cydui_ui *ui, cydui_id id, cydui_state *st, cydui_rect view, int32_t content, bool bounded)
{
    cydui_input *in = &ui->in;
    uint32_t dt = in->has_clock ? in->now - (uint32_t)st->d : 0u;
    bool mine = cydui_id_eq(ui->active, id);
    int64_t off = st->a, max_off = OFFSET_CAP;

    if (dt > MAX_DT) {
        dt = MAX_DT;
    }
    if (in->frame_down && inside(view, in->press_x, in->press_y)) {
        st->b = 0; /* a finger on a coasting area stops it */
    }
    /* A drag that began on something else in the window: the area takes the touch over, before any
     * child can show it as pressed in this frame. What keeps the press (a slider) stays with it. */
    if (!mine && (in->down || in->frame_up) && in->moved_far && !ui->active_capture && inside(view, in->press_x, in->press_y)) {
        ui->active = id;
        mine = true;
    }

    if (mine && (in->down || in->frame_up) && in->moved_far) {
        int dy = in->y - in->frame_y;

        off -= (int64_t)dy * (1 << FRAC);
        if (dt > 0 && (in->down || dy != 0)) { /* the speed averages the recent frames, so it forgets a finger that rests; the frame of the release usually has no travel and says nothing */
            int64_t vn = -(int64_t)dy * 1000 / (int64_t)dt;
            int64_t v = ((int64_t)st->b + vn) / 2;
            v = v > CYDUI_SCROLL_MAX_V ? CYDUI_SCROLL_MAX_V : (v < -CYDUI_SCROLL_MAX_V ? -CYDUI_SCROLL_MAX_V : v);
            st->b = (int32_t)v;
        }
        if (in->frame_up && abs64(st->b) < CYDUI_SCROLL_FLING_MIN) {
            st->b = 0;
        }
    } else if (st->b != 0) {
        if (dt > 0) {
            int64_t v = st->b, mag = abs64(v);

            off += v * (int64_t)dt * (1 << FRAC) / 1000;
            mag -= (mag * (int64_t)dt + CYDUI_SCROLL_FRICTION_MS - 1) / CYDUI_SCROLL_FRICTION_MS; /* at least 1 */
            st->b = mag < CYDUI_SCROLL_MIN_V ? 0 : (int32_t)(v < 0 ? -mag : mag);
        }
    }

    if (bounded) {
        max_off = (int64_t)(content > view.h ? content - view.h : 0) * (1 << FRAC);
        if (max_off > OFFSET_CAP) {
            max_off = OFFSET_CAP;
        }
    }
    if (off < 0 || off > max_off) {
        off = off < 0 ? 0 : max_off;
        st->b = 0; /* no overscroll: the end stops the motion */
    }
    st->a = (int32_t)off;
    st->d = (int32_t)in->now;
    if (st->b != 0) {
        ui->relayout = true; /* still coasting: another frame, please */
    }
}

/* Opens the container of an area (or list) and, unless it is nested, takes its state through `step`. */
static cydui_region *open_area(cydui_ui *ui, const char *label, int view_h, bool list, int32_t content)
{
    cydui_mods m = cydui_take_mods(ui);
    cydui_region *r;
    cydui_rect view;
    cydui_state *st;
    cydui_id id;
    bool nested = false;
    int i;

    for (i = 1; i <= ui->depth; i++) {
        nested = nested || ui->regions[i].kind == CYDUI_LAYOUT_SCROLL;
    }
    if (nested) {
        ui->ctx.errors |= CYDUI_ERR_BAD_ARG;
    }
    if (m.h < 0 && !m.fill) {
        m.h = i16(view_h < 0 ? 0 : view_h);
    }
    ui->mods = m;
    if (!cydui__open_region(ui, CYDUI_LAYOUT_SCROLL, 1, 0)) {
        return NULL;
    }
    r = &ui->regions[ui->depth];
    if (r->fix_w < 0) {
        r->fix_w = r->avail_w; /* the area is as wide as its window, whatever its content is */
    }
    if (nested) {
        r->scroll_flags = CYDUI_SCROLL_F_INERT;
        return r;
    }
    view.x = r->x0;
    view.y = r->y0;
    view.w = r->avail_w;
    view.h = r->avail_h;
    id = cydui_id_str(ui, label ? label : "");
    cydui_id_claim(ui, id);
    r->wid = id;
    r->view_y = r->y0;
    r->scroll_flags = list ? CYDUI_SCROLL_F_LIST : 0;
    r->content = content;

    st = cydui_state_get(ui, id, true);
    if (st) {
        step(ui, id, st, view, list ? content : st->c, list || st->c > 0);
        r->off_px = st->a >> FRAC;
    }
    if (!list) {
        r->y0 = i16((int64_t)view.y - r->off_px);
        r->cy = r->y0;
    }
    cydui_set_clip(&ui->ctx, view);
    return r;
}

void cydui_scroll_begin(cydui_ui *ui, const char *id, int16_t view_h)
{
    cydui__check_thread(ui);
    open_area(ui, id, view_h, false, 0);
}

void cydui__scroll_finish(cydui_ui *ui)
{
    cydui_region *r = &ui->regions[ui->depth];
    cydui_rect view;
    cydui_state *st;
    int32_t content;
    bool held;

    if (r->kind != CYDUI_LAYOUT_SCROLL || (r->scroll_flags & CYDUI_SCROLL_F_INERT)) {
        return;
    }
    view.x = r->x0;
    view.y = r->view_y;
    view.w = r->avail_w;
    view.h = r->avail_h;
    content = (r->scroll_flags & CYDUI_SCROLL_F_LIST) ? r->content : r->ext_h;

    st = cydui_state_get(ui, r->wid, false);
    if (st && st->c != content) {
        st->c = content;
        ui->relayout = true; /* the clamp of this frame used the old height */
    }
    held = cydui_id_eq(ui->active, r->wid) && ui->in.down && ui->in.moved_far;
    (void)cydui_interact(ui, r->wid, view, CYDUI_SENSE_DRAG); /* a touch on bare space grabs the area */

    if (content > view.h && view.w > 2 * SCROLLBAR_INSET + SCROLLBAR_W && view.h > 2 * SCROLLBAR_INSET) {
        cydui_paint_info pi;

        memset(&pi, 0, sizeof(pi));
        pi.kind = CYDUI_WIDGET_SCROLLBAR;
        pi.state = held ? CYDUI_STATE_PRESSED : CYDUI_STATE_NORMAL;
        pi.rect.x = (int16_t)(view.x + view.w - SCROLLBAR_INSET - SCROLLBAR_W);
        pi.rect.y = (int16_t)(view.y + SCROLLBAR_INSET);
        pi.rect.w = SCROLLBAR_W;
        pi.rect.h = (int16_t)(view.h - 2 * SCROLLBAR_INSET);
        pi.value = r->off_px;
        pi.min = 0;
        pi.max = content - view.h;
        pi.aux = view;
        cydui_paint(ui, &pi);
    }
    cydui_reset_clip(&ui->ctx);
}

void cydui_scroll_end(cydui_ui *ui)
{
    cydui__check_thread(ui);
    if (ui->skipped == 0 && ui->depth > 0) {
        cydui__scroll_finish(ui);
    }
    cydui__end_kind(ui, CYDUI_LAYOUT_SCROLL);
}

void cydui_scroll_to(cydui_ui *ui, const char *label, int32_t y)
{
    cydui_state *st;

    cydui__check_thread(ui);
    st = cydui_state_get(ui, cydui_id_str(ui, label ? label : ""), true);
    if (st) {
        y = y < 0 ? 0 : (y > CYDUI_LIST_MAX_PX ? CYDUI_LIST_MAX_PX : y);
        st->a = y * (1 << FRAC);
        st->b = 0;
        ui->relayout = true; /* the clamp to the real height happens when the area is built */
    }
}

void cydui__scroll_reveal(cydui_ui *ui, cydui_rect r, int limit)
{
    int d;

    for (d = ui->depth; d >= 1; d--) {
        const cydui_region *a = &ui->regions[d];
        cydui_state *st;
        int bottom = r.y + r.h, top_limit, shift = 0;

        if (a->kind != CYDUI_LAYOUT_SCROLL || (a->scroll_flags & (CYDUI_SCROLL_F_LIST | CYDUI_SCROLL_F_INERT))) {
            continue; /* a list is not a place for a field, and a nested area is a plain column */
        }
        top_limit = a->view_y;
        if (bottom > limit) {
            shift = bottom - limit; /* down: what is below the keyboard comes up */
        }
        if (r.y - shift < top_limit) {
            shift = r.y - top_limit; /* never past the top of the field (a very tall one shows its top) */
        }
        if (shift == 0) {
            return;
        }
        st = cydui_state_get(ui, a->wid, false);
        if (st) {
            int64_t off = (int64_t)st->a + (int64_t)shift * (1 << FRAC);
            st->a = (int32_t)(off < 0 ? 0 : (off > (int64_t)OFFSET_CAP ? (int64_t)OFFSET_CAP : off));
            st->b = 0;
            ui->relayout = true;
        }
        return;
    }
}

/* ---- virtual lists ---- */

void cydui_list_begin(cydui_ui *ui, const char *id, int32_t count, int16_t item_h, int16_t view_h,
                      int32_t *first, int32_t *last)
{
    cydui_region *r;
    int64_t content = 0, off, lo, hi;
    int32_t f = 0, l = 0;

    cydui__check_thread(ui);
    if (item_h < 0) {
        item_h = 0;
    }
    if (count > 0 && item_h > 0) {
        content = (int64_t)count * item_h;
        if (content > CYDUI_LIST_MAX_PX) {
            content = CYDUI_LIST_MAX_PX;
        }
    }
    r = open_area(ui, id, view_h, true, (int32_t)content);
    if (r && content > 0 && !(r->scroll_flags & CYDUI_SCROLL_F_INERT)) {
        r->item_h = item_h;
        off = r->off_px;
        lo = off / item_h;
        hi = (off + r->avail_h + item_h - 1) / item_h;
        hi = hi > count ? count : hi;
        f = (int32_t)(lo > hi ? hi : lo);
        l = (int32_t)hi;
    }
    if (first) {
        *first = f;
    }
    if (last) {
        *last = l;
    }
}

void cydui_begin_list_item(cydui_ui *ui, int32_t index)
{
    cydui_region *r = &ui->regions[ui->depth];
    cydui_rect rect;

    cydui__check_thread(ui);
    if (ui->skipped > 0 || r->kind != CYDUI_LAYOUT_SCROLL || !(r->scroll_flags & CYDUI_SCROLL_F_LIST) ||
        r->item_h <= 0) { /* not in a list: an item that holds its own place, so that the end matches */
        ui->ctx.errors |= CYDUI_ERR_LAYOUT;
        cydui__open_region_at(ui, CYDUI_LAYOUT_COLUMN, (cydui_rect){0, 0, 0, 0});
        cydui_push_id_int(ui, index);
        return;
    }
    rect.x = r->x0;
    rect.y = i16((int64_t)r->view_y + (int64_t)index * r->item_h - r->off_px);
    rect.w = r->avail_w;
    rect.h = r->item_h;
    cydui__open_region_at(ui, CYDUI_LAYOUT_COLUMN, rect);
    cydui_push_id_int(ui, index);
}

void cydui_end_list_item(cydui_ui *ui)
{
    cydui__check_thread(ui);
    if (ui->skipped == 0 && ui->depth > 0 && ui->regions[ui->depth].kind == CYDUI_LAYOUT_COLUMN) {
        cydui_pop_id(ui);
    }
    cydui__end_kind(ui, CYDUI_LAYOUT_COLUMN);
}

void cydui_list_end(cydui_ui *ui)
{
    cydui_scroll_end(ui);
}
