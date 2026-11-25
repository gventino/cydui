#ifndef CYDUI_TOUCH_CAL_H
#define CYDUI_TOUCH_CAL_H

/*
 * Calibration of a resistive touch screen: the affine map from the raw readings of the controller to
 * screen pixels, fitted by least squares to the touches on a few targets, checked before it is used or
 * saved (CT-06), and packed into a small blob for the flash. Pure C: no ESP-IDF headers, tested on the PC.
 *
 *     x = m[0] * raw_x + m[1] * raw_y + m[2]
 *     y = m[3] * raw_x + m[4] * raw_y + m[5]
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float m[6];
} cydui_touch_cal;

/** A target the user touched: where the controller said, and where the target is on the screen. */
typedef struct {
    float raw_x, raw_y;
    float x, y;
} cydui_cal_sample;

typedef enum {
    CYDUI_CAL_OK = 0,
    CYDUI_CAL_ERR_ARG,        /**< no output or points, fewer than 3 or more than 16 points */
    CYDUI_CAL_ERR_NOT_FINITE, /**< a point or a result is NaN or infinite */
    CYDUI_CAL_ERR_DEGENERATE, /**< the points do not span the plane (collinear, or all the same) */
    CYDUI_CAL_ERR_RESIDUAL    /**< some target is farther from the fitted map than allowed */
} cydui_cal_status;

#define CYDUI_CAL_MAX_POINTS 16
#define CYDUI_CAL_BLOB_SIZE 32

/**
 * Least-squares fit of the map to `n` touches. `out` is only written when the result is accepted:
 * finite, invertible, and with no target more than `max_residual_px` pixels from where the map puts it.
 * `*worst_px` (optional) gets the largest residual in any case where it could be computed.
 */
cydui_cal_status cydui_cal_fit(cydui_touch_cal *out, const cydui_cal_sample *pts, int n, float max_residual_px,
                               float *worst_px);

/** The map a raw reading goes through: rounded to the nearest pixel and clamped to the screen. */
void cydui_cal_apply(const cydui_touch_cal *c, int raw_x, int raw_y, int16_t width, int16_t height, int16_t *x,
                     int16_t *y);

/** True for a map that can be used: finite, invertible, not absurdly large. */
bool cydui_cal_valid(const cydui_touch_cal *c);

/**
 * A usable map for a panel whose controller has not been calibrated: the raw range [raw_min, raw_max]
 * on both axes spread over the screen, with the axes swapped (the CYD: raw Y is horizontal) and/or
 * mirrored.
 */
void cydui_cal_default(cydui_touch_cal *c, int16_t width, int16_t height, int raw_min, int raw_max, bool swap_xy,
                       bool flip_x, bool flip_y);

/** Packs a map into 32 bytes (magic, six little-endian floats, CRC-32) for the flash. */
void cydui_cal_pack(const cydui_touch_cal *c, uint8_t out[CYDUI_CAL_BLOB_SIZE]);

/** Unpacks and checks a blob: its length, magic, CRC-32 and the map itself. False leaves `c` alone. */
bool cydui_cal_unpack(cydui_touch_cal *c, const uint8_t *blob, size_t len);

/** CRC-32 (IEEE 802.3, the one of zlib) of a block. */
uint32_t cydui_crc32(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_TOUCH_CAL_H */
