/*
 * libFuzzer harness for the pure parts of the ESP32 backend that take numbers from the outside world (CT-06, AB-05,
 * R-04): the touch filter (bursts of raw readings and the pen interrupt), the calibration (fit, apply, pack, unpack
 * of a blob read back from the flash) and the latency ring.
 *
 * Oracles: the events of the filter are a valid sequence (a DOWN only while up, a MOVE or UP only while down, a UP at
 * the last position, nothing that needs a touch without one); a fitted map is usable, a pixel is on the screen;
 * a packed map reads back bit for bit, and a blob with any one byte changed is refused; a random blob either is
 * refused or gives a usable map; percentiles are ordered.
 *
 * Input: a selector byte, then what that part needs.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/backend.h"
#include "latency.h"
#include "touch_cal.h"
#include "touch_filter.h"

typedef struct {
    const uint8_t *p;
    size_t n;
} reader;

static uint8_t u8(reader *r)
{
    return r->n ? (r->n--, *r->p++) : 0;
}

static uint16_t u16(reader *r)
{
    uint16_t lo = u8(r);
    return (uint16_t)(lo | ((uint16_t)u8(r) << 8));
}

static float f32(reader *r)
{
    uint32_t bits = 0;
    float v;
    int i;
    uint8_t mode = u8(r);

    if (mode % 4 == 0) { /* any bit pattern: NaN, infinities, denormals */
        for (i = 0; i < 4; i++) bits |= (uint32_t)u8(r) << (8 * i);
        memcpy(&v, &bits, sizeof(v));
        return v;
    }
    return (float)((int)u16(r) - 2048) / (mode % 4 == 1 ? 1.0f : 16.0f); /* plausible numbers */
}

static void touch_filter_run(reader *r)
{
    cydui_touch_cfg cfg = cydui_touch_cfg_default();
    cydui_touch_filter f;
    cydui_touch_read reads[CYDUI_TOUCH_MAX_READS + 6];
    bool down = false;
    uint16_t last_x = 0, last_y = 0;
    int ticks;

    if (u8(r) & 1) { /* a configuration of any numbers, the degenerate ones too */
        cfg.z_on = u16(r);
        cfg.z_off = u16(r);
        cfg.down_ticks = u8(r);
        cfg.up_ticks = u8(r);
        cfg.move_min = u16(r);
        cfg.max_spread = u16(r);
        cfg.max_jump = u16(r);
    }
    cydui_touch_filter_init(&f, &cfg);
    if (f.cfg.down_ticks < 1 || f.cfg.up_ticks < 1 || f.cfg.z_off > f.cfg.z_on) {
        abort(); /* the filter must repair its configuration */
    }
    for (ticks = 0; ticks < 400 && r->n > 0; ticks++) {
        uint8_t b = u8(r);
        bool irq_low = (b & 1) != 0;
        int n = (b >> 1) % (CYDUI_TOUCH_MAX_READS + 7) - 1; /* -1 .. beyond the limit */
        int i;
        uint16_t base_x = u16(r), base_y = u16(r), base_z = u16(r);
        cydui_event ev;
        bool got;

        for (i = 0; i < CYDUI_TOUCH_MAX_READS + 6; i++) {
            uint8_t wobble = (b >> 4);
            reads[i].x = (uint16_t)(base_x + (u8(r) % (wobble + 1)));
            reads[i].y = (uint16_t)(base_y + (u8(r) % (wobble + 1)));
            reads[i].z = (uint16_t)(base_z + (u8(r) % (wobble + 1)));
        }
        memset(&ev, 0, sizeof(ev));
        got = cydui_touch_tick(&f, n == 0 && (b & 0x80) ? NULL : reads, n, irq_low, &ev);
        if (got) {
            if (ev.type == CYDUI_EVENT_DOWN) {
                if (down || !irq_low || n <= 0) abort();
                down = true;
                last_x = (uint16_t)ev.x;
                last_y = (uint16_t)ev.y;
            } else if (ev.type == CYDUI_EVENT_MOVE) {
                if (!down || !irq_low || n <= 0) abort();
                last_x = (uint16_t)ev.x;
                last_y = (uint16_t)ev.y;
            } else if (ev.type == CYDUI_EVENT_UP) {
                if (!down || (uint16_t)ev.x != last_x || (uint16_t)ev.y != last_y) abort();
                down = false;
            } else {
                abort(); /* nothing else comes out of a filter */
            }
        }
        if (f.down != down) abort();
    }
}

static void calibration_run(reader *r)
{
    cydui_cal_sample pts[CYDUI_CAL_MAX_POINTS + 3];
    cydui_touch_cal cal, back;
    uint8_t blob[CYDUI_CAL_BLOB_SIZE], other[CYDUI_CAL_BLOB_SIZE + 4];
    int n = (int)(u8(r) % (CYDUI_CAL_MAX_POINTS + 5)) - 1, i, k;
    float worst = -1.0f, max_res = f32(r);
    int16_t w = (int16_t)(1 + u8(r)), h = (int16_t)(1 + u8(r)), px, py;
    cydui_cal_status st;

    for (i = 0; i < CYDUI_CAL_MAX_POINTS + 3; i++) {
        pts[i].raw_x = f32(r);
        pts[i].raw_y = f32(r);
        pts[i].x = f32(r);
        pts[i].y = f32(r);
    }
    memset(&cal, 0, sizeof(cal));
    st = cydui_cal_fit(&cal, pts, n, max_res, &worst);
    if (st == CYDUI_CAL_OK && !cydui_cal_valid(&cal)) abort(); /* an accepted map is a usable one */
    if (st == CYDUI_CAL_OK) {
        int rx = (int)u16(r) - 32768, ry = (int)u16(r) - 32768;

        cydui_cal_apply(&cal, rx, ry, w, h, &px, &py);
        if (px < 0 || px >= w || py < 0 || py >= h) abort();
        cydui_cal_pack(&cal, blob);
        memset(&back, 0, sizeof(back));
        if (!cydui_cal_unpack(&back, blob, sizeof(blob)) || memcmp(&back, &cal, sizeof(cal)) != 0) abort();
        for (k = 0; k < CYDUI_CAL_BLOB_SIZE; k++) { /* one byte changed: the CRC (or the magic) says no */
            uint8_t saved = blob[k];
            blob[k] = (uint8_t)(saved ^ (uint8_t)(1u + u8(r) % 255u));
            memset(&back, 0, sizeof(back));
            if (cydui_cal_unpack(&back, blob, sizeof(blob))) abort();
            blob[k] = saved;
        }
        if (cydui_cal_unpack(&back, blob, sizeof(blob) - 1) || cydui_cal_unpack(&back, blob, 0)) abort(); /* too short */
    } else if (st != CYDUI_CAL_ERR_ARG && st != CYDUI_CAL_ERR_NOT_FINITE && st != CYDUI_CAL_ERR_DEGENERATE &&
               st != CYDUI_CAL_ERR_RESIDUAL) {
        abort();
    }
    cydui_cal_default(&cal, w, h, (int)u16(r) - 100, (int)u16(r), (u8(r) & 1) != 0, (u8(r) & 1) != 0, (u8(r) & 1) != 0);
    if (!cydui_cal_valid(&cal)) { /* a default for a nonsense raw range may be refused, but never used by accident */
        cydui_cal_apply(&cal, 100, 100, w, h, &px, &py);
        if (px < 0 || px >= w || py < 0 || py >= h) abort();
    }
    for (k = 0; k < (int)sizeof(other); k++) other[k] = u8(r); /* what the flash may hold */
    memset(&back, 0, sizeof(back));
    if (cydui_cal_unpack(&back, other, (size_t)(u8(r) % (sizeof(other) + 1))) && !cydui_cal_valid(&back)) abort();
    (void)cydui_crc32(other, sizeof(other));
}

static void latency_run(reader *r)
{
    cydui_lat l;
    cydui_lat_stats s;
    int n = 0;

    cydui_lat_init(&l);
    while (r->n > 0 && n < 600) {
        uint32_t v = (uint32_t)u16(r) | ((uint32_t)(u8(r) % 3 == 0 ? u16(r) : 0) << 16);
        cydui_lat_add(&l, v);
        n++;
        cydui_lat_stats_get(&l, &s);
        if (l.n < 0 || l.n > CYDUI_LAT_MAX || l.n != (n < CYDUI_LAT_MAX ? n : CYDUI_LAT_MAX)) abort();
        if (s.p50 > s.p95 || s.p95 > s.max) abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    reader r;

    r.p = data;
    r.n = size;
    switch (u8(&r) % 3) {
    case 0:
        touch_filter_run(&r);
        break;
    case 1:
        calibration_run(&r);
        break;
    default:
        latency_run(&r);
        break;
    }
    return 0;
}
