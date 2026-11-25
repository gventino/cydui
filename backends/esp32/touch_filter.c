#include "touch_filter.h"

#include <string.h>

cydui_touch_cfg cydui_touch_cfg_default(void)
{
    cydui_touch_cfg c;

    c.z_on = 600;
    c.z_off = 350;
    c.down_ticks = 2;
    c.up_ticks = 3;
    c.move_min = 24;    /* about 2 px on a 320 px screen */
    c.max_spread = 220; /* about 18 px */
    c.max_jump = 1200;  /* about a third of the screen in one tick */
    return c;
}

void cydui_touch_filter_init(cydui_touch_filter *f, const cydui_touch_cfg *cfg)
{
    memset(f, 0, sizeof(*f));
    f->cfg = *cfg;
    if (f->cfg.down_ticks < 1) f->cfg.down_ticks = 1;
    if (f->cfg.up_ticks < 1) f->cfg.up_ticks = 1;
    if (f->cfg.z_off > f->cfg.z_on) f->cfg.z_off = f->cfg.z_on;
}

static void sort_u16(uint16_t *v, int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        uint16_t t = v[i];
        for (j = i - 1; j >= 0 && v[j] > t; j--) {
            v[j + 1] = v[j];
        }
        v[j + 1] = t;
    }
}

/* mean without the smallest and the largest (with fewer than 3 values: the plain mean); spread = max - min */
static uint16_t trimmed(const uint16_t *in, int n, uint16_t *spread)
{
    uint16_t v[CYDUI_TOUCH_MAX_READS];
    uint32_t sum = 0;
    int i, lo = n >= 3 ? 1 : 0, hi = n >= 3 ? n - 1 : n;

    memcpy(v, in, (size_t)n * sizeof(v[0]));
    sort_u16(v, n);
    *spread = (uint16_t)(v[n - 1] - v[0]);
    for (i = lo; i < hi; i++) {
        sum += v[i];
    }
    return (uint16_t)((sum + (uint32_t)(hi - lo) / 2u) / (uint32_t)(hi - lo));
}

static uint16_t absdiff(uint16_t a, uint16_t b)
{
    return (uint16_t)(a > b ? a - b : b - a);
}

static bool far_from(uint16_t x, uint16_t y, uint16_t px, uint16_t py, uint16_t limit)
{
    return absdiff(x, px) > limit || absdiff(y, py) > limit;
}

static void emit(cydui_event *ev, cydui_event_type type, uint16_t x, uint16_t y)
{
    ev->type = type;
    ev->x = (int16_t)x;
    ev->y = (int16_t)y;
}

bool cydui_touch_tick(cydui_touch_filter *f, const cydui_touch_read *reads, int n, bool irq_low, cydui_event *ev)
{
    uint16_t xs[CYDUI_TOUCH_MAX_READS], ys[CYDUI_TOUCH_MAX_READS], zs[CYDUI_TOUCH_MAX_READS];
    uint16_t x = 0, y = 0, z = 0, spread_x, spread_y, spread_z;
    bool pressed = false;
    int i;

    if (n > CYDUI_TOUCH_MAX_READS) {
        n = CYDUI_TOUCH_MAX_READS;
    }
    if (irq_low && n > 0 && reads) {
        for (i = 0; i < n; i++) {
            xs[i] = reads[i].x;
            ys[i] = reads[i].y;
            zs[i] = reads[i].z;
        }
        x = trimmed(xs, n, &spread_x);
        y = trimmed(ys, n, &spread_y);
        z = trimmed(zs, n, &spread_z);
        if (spread_x > f->cfg.max_spread || spread_y > f->cfg.max_spread) {
            return false; /* a contact that is bouncing: no vote, neither for a press nor for a release */
        }
        f->sample_x = x;
        f->sample_y = y;
        f->sample_z = z;
        f->samples++;
        pressed = f->down ? z >= f->cfg.z_off : z >= f->cfg.z_on;
    }

    if (!pressed) {
        f->press_run = 0;
        f->jump_pending = false;
        if (f->down && ++f->release_run >= f->cfg.up_ticks) {
            f->down = false;
            f->release_run = 0;
            emit(ev, CYDUI_EVENT_UP, f->x, f->y);
            return true;
        }
        return false;
    }

    f->release_run = 0;
    if (!f->down) {
        if (++f->press_run >= f->cfg.down_ticks) {
            f->down = true;
            f->press_run = 0;
            f->jump_pending = false;
            f->x = x;
            f->y = y;
            emit(ev, CYDUI_EVENT_DOWN, x, y);
            return true;
        }
        return false;
    }

    if (far_from(x, y, f->x, f->y, f->cfg.max_jump)) {
        if (!f->jump_pending || far_from(x, y, f->jx, f->jy, f->cfg.max_jump)) {
            f->jump_pending = true; /* believed only if the next tick lands near this one */
            f->jx = x;
            f->jy = y;
            return false;
        }
    }
    f->jump_pending = false;
    if (absdiff(x, f->x) >= f->cfg.move_min || absdiff(y, f->y) >= f->cfg.move_min) {
        f->x = x;
        f->y = y;
        emit(ev, CYDUI_EVENT_MOVE, x, y);
        return true;
    }
    return false;
}
