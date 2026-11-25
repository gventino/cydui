#include "touch_cal.h"

#include <math.h>
#include <string.h>

#define RAW_SCALE 4096.0 /* the fit runs on raw / 4096, so that its numbers are about 1 */
#define MIN_DET 1e-6     /* of the normal equations, in those units */
#define MIN_AFFINE_DET 1e-5 /* of the map, in pixels squared per raw unit squared (a CYD has about 0.0075) */

static bool finite_f(double v)
{
    return isfinite(v) != 0;
}

/* Cramer's rule on a 3 x 3 system A x = b */
static bool solve3(double a[3][3], const double b[3], double x[3])
{
    double det = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
                 a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    int c, r, k;

    if (!(fabs(det) > MIN_DET)) {
        return false;
    }
    for (c = 0; c < 3; c++) {
        double t[3][3];
        for (r = 0; r < 3; r++) {
            for (k = 0; k < 3; k++) {
                t[r][k] = k == c ? b[r] : a[r][k];
            }
        }
        x[c] = (t[0][0] * (t[1][1] * t[2][2] - t[1][2] * t[2][1]) - t[0][1] * (t[1][0] * t[2][2] - t[1][2] * t[2][0]) +
                t[0][2] * (t[1][0] * t[2][1] - t[1][1] * t[2][0])) /
               det;
    }
    return true;
}

bool cydui_cal_valid(const cydui_touch_cal *c)
{
    int i;
    double det;

    if (!c) {
        return false;
    }
    for (i = 0; i < 6; i++) {
        if (!finite_f(c->m[i])) {
            return false;
        }
    }
    if (fabs(c->m[0]) > 10.0 || fabs(c->m[1]) > 10.0 || fabs(c->m[3]) > 10.0 || fabs(c->m[4]) > 10.0 ||
        fabs(c->m[2]) > 1e5 || fabs(c->m[5]) > 1e5) {
        return false;
    }
    det = (double)c->m[0] * c->m[4] - (double)c->m[1] * c->m[3];
    return fabs(det) >= MIN_AFFINE_DET;
}

cydui_cal_status cydui_cal_fit(cydui_touch_cal *out, const cydui_cal_sample *pts, int n, float max_residual_px,
                               float *worst_px)
{
    double s_uu = 0, s_uv = 0, s_u = 0, s_vv = 0, s_v = 0;
    double tx_u = 0, tx_v = 0, tx = 0, ty_u = 0, ty_v = 0, ty = 0;
    double a[3][3], bx[3], by[3], cx[3], cy[3], worst = 0;
    cydui_touch_cal fit;
    int i;

    if (worst_px) {
        *worst_px = 0.0f;
    }
    if (!out || !pts || n < 3 || n > CYDUI_CAL_MAX_POINTS) {
        return CYDUI_CAL_ERR_ARG;
    }
    for (i = 0; i < n; i++) {
        double u, v;
        if (!finite_f(pts[i].raw_x) || !finite_f(pts[i].raw_y) || !finite_f(pts[i].x) || !finite_f(pts[i].y)) {
            return CYDUI_CAL_ERR_NOT_FINITE;
        }
        u = pts[i].raw_x / RAW_SCALE;
        v = pts[i].raw_y / RAW_SCALE;
        s_uu += u * u;
        s_uv += u * v;
        s_u += u;
        s_vv += v * v;
        s_v += v;
        tx_u += u * pts[i].x;
        tx_v += v * pts[i].x;
        tx += pts[i].x;
        ty_u += u * pts[i].y;
        ty_v += v * pts[i].y;
        ty += pts[i].y;
    }
    a[0][0] = s_uu; a[0][1] = s_uv; a[0][2] = s_u;
    a[1][0] = s_uv; a[1][1] = s_vv; a[1][2] = s_v;
    a[2][0] = s_u;  a[2][1] = s_v;  a[2][2] = (double)n;
    bx[0] = tx_u; bx[1] = tx_v; bx[2] = tx;
    by[0] = ty_u; by[1] = ty_v; by[2] = ty;
    if (!solve3(a, bx, cx) || !solve3(a, by, cy)) {
        return CYDUI_CAL_ERR_DEGENERATE;
    }
    fit.m[0] = (float)(cx[0] / RAW_SCALE);
    fit.m[1] = (float)(cx[1] / RAW_SCALE);
    fit.m[2] = (float)cx[2];
    fit.m[3] = (float)(cy[0] / RAW_SCALE);
    fit.m[4] = (float)(cy[1] / RAW_SCALE);
    fit.m[5] = (float)cy[2];
    for (i = 0; i < 6; i++) {
        if (!finite_f(fit.m[i])) {
            return CYDUI_CAL_ERR_NOT_FINITE;
        }
    }
    for (i = 0; i < n; i++) {
        double px = (double)fit.m[0] * pts[i].raw_x + (double)fit.m[1] * pts[i].raw_y + fit.m[2];
        double py = (double)fit.m[3] * pts[i].raw_x + (double)fit.m[4] * pts[i].raw_y + fit.m[5];
        double e = sqrt((px - pts[i].x) * (px - pts[i].x) + (py - pts[i].y) * (py - pts[i].y));
        if (e > worst) {
            worst = e;
        }
    }
    if (worst_px) {
        *worst_px = (float)worst;
    }
    if (!cydui_cal_valid(&fit)) {
        return CYDUI_CAL_ERR_DEGENERATE;
    }
    if (!((float)worst <= max_residual_px)) { /* the number that is reported is the number that is compared */
        return CYDUI_CAL_ERR_RESIDUAL;
    }
    *out = fit;
    return CYDUI_CAL_OK;
}

static int16_t clamp_round(double v, int16_t size)
{
    double r = floor(v + 0.5);
    if (!(r > 0.0)) {
        return 0;
    }
    if (r > (double)(size - 1)) {
        return (int16_t)(size - 1);
    }
    return (int16_t)r;
}

void cydui_cal_apply(const cydui_touch_cal *c, int raw_x, int raw_y, int16_t width, int16_t height, int16_t *x,
                     int16_t *y)
{
    double fx = (double)c->m[0] * raw_x + (double)c->m[1] * raw_y + c->m[2];
    double fy = (double)c->m[3] * raw_x + (double)c->m[4] * raw_y + c->m[5];

    *x = clamp_round(fx, width);
    *y = clamp_round(fy, height);
}

void cydui_cal_default(cydui_touch_cal *c, int16_t width, int16_t height, int raw_min, int raw_max, bool swap_xy,
                       bool flip_x, bool flip_y)
{
    double span = (double)(raw_max - raw_min);
    double sx = (double)width / span, sy = (double)height / span; /* pixels per raw unit */
    double ox = flip_x ? (double)width + raw_min * sx : -raw_min * sx;
    double oy = flip_y ? (double)height + raw_min * sy : -raw_min * sy;
    double kx = flip_x ? -sx : sx, ky = flip_y ? -sy : sy;

    memset(c, 0, sizeof(*c));
    if (swap_xy) { /* x comes from raw Y, y from raw X */
        c->m[1] = (float)kx;
        c->m[2] = (float)ox;
        c->m[3] = (float)ky;
        c->m[5] = (float)oy;
    } else {
        c->m[0] = (float)kx;
        c->m[2] = (float)ox;
        c->m[4] = (float)ky;
        c->m[5] = (float)oy;
    }
}

/* ---- the blob ---- */

uint32_t cydui_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    int b;

    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

#define BLOB_MAGIC 0x314C4143u /* the bytes "CAL1", read as little endian */

void cydui_cal_pack(const cydui_touch_cal *c, uint8_t out[CYDUI_CAL_BLOB_SIZE])
{
    int i;
    uint32_t bits;

    put32(out, BLOB_MAGIC);
    for (i = 0; i < 6; i++) {
        memcpy(&bits, &c->m[i], sizeof(bits)); /* IEEE 754 single on every target we have */
        put32(out + 4 + 4 * i, bits);
    }
    put32(out + 28, cydui_crc32(out, 28));
}

bool cydui_cal_unpack(cydui_touch_cal *c, const uint8_t *blob, size_t len)
{
    cydui_touch_cal t;
    int i;

    if (!c || !blob || len != CYDUI_CAL_BLOB_SIZE || get32(blob) != BLOB_MAGIC || get32(blob + 28) != cydui_crc32(blob, 28)) {
        return false;
    }
    for (i = 0; i < 6; i++) {
        uint32_t bits = get32(blob + 4 + 4 * i);
        memcpy(&t.m[i], &bits, sizeof(bits));
    }
    if (!cydui_cal_valid(&t)) {
        return false;
    }
    *c = t;
    return true;
}
