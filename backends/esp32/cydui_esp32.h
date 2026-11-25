#ifndef CYDUI_ESP32_H
#define CYDUI_ESP32_H

/*
 * The cydui backend for the ESP32 (ESP-IDF 5.5): an ILI9342C / ILI9341 / ST7789 display over SPI with DMA
 * and an XPT2046 resistive touch controller on a separate SPI bus. Pins, clocks, controller and the
 * touch filter are set in menuconfig ("cydui ESP32 backend").
 *
 * The application provides the memory (the strip buffers, which must be in internal RAM) and the
 * cydui_esp32 object; the backend allocates nothing after cydui_esp32_init.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cydui/backend.h"
#include "cydui/cydui.h"
#include "driver/spi_master.h"
#include "touch_cal.h"
#include "latency.h"
#include "touch_filter.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /** The largest strip the core will send, in bytes: strip rows * width * 2. */
    size_t max_strip_bytes;
} cydui_esp32_config;

typedef struct cydui_esp32 {
    cydui_backend be; /**< hand &dev->be (or cydui_esp32_backend) to cydui_init */
    /* ---- private from here ---- */
    spi_device_handle_t lcd;
    spi_transaction_t lcd_trans;
    bool lcd_pending;
    size_t max_strip_bytes;
    uint32_t flushes;
    uint32_t rejected;
    bool hash_on;
    uint64_t hash;
    /* the touch screen */
    spi_device_handle_t touch;
    esp_err_t touch_status;
    QueueHandle_t touch_q;      /* cydui_event, already in pixels */
    SemaphoreHandle_t touch_irq; /* given by the pen interrupt */
    TaskHandle_t touch_task;
    cydui_touch_filter filter;
    cydui_touch_cal cal;
    bool calibrated;
    portMUX_TYPE touch_lock;
    cydui_lat lat;                /* touch -> picture, in us */
    int64_t down_pending_us;      /* a DOWN taken from the queue whose frame has not ended yet */
    uint16_t raw_x, raw_y, raw_z; /* the latest steady sample */
    int16_t px, py;               /* ... and where the map puts it */
    bool pressed;
    uint32_t sample_count;
} cydui_esp32;

/**
 * Starts the display (and its backlight, off until the first frame is drawn). Returns ESP_OK or the
 * error of the step that failed, and never blocks forever: the application decides what to do.
 */
esp_err_t cydui_esp32_init(cydui_esp32 *dev, const cydui_esp32_config *cfg);

/** The backend for cydui_init. */
const cydui_backend *cydui_esp32_backend(cydui_esp32 *dev);

/** Backlight brightness, 0..100. */
void cydui_esp32_set_backlight(cydui_esp32 *dev, int percent);

/**
 * The result of starting the touch screen. cydui_esp32_init only fails for the display; a touch controller
 * that does not answer leaves the screen working and this says why (the application decides what to do).
 */
esp_err_t cydui_esp32_touch_status(const cydui_esp32 *dev);

/** The latest steady sample (raw controller units), where the calibration puts it, and whether it is pressed. */
typedef struct {
    uint16_t raw_x, raw_y, raw_z;
    int16_t x, y;
    bool down;
    uint32_t samples;
} cydui_esp32_touch_info;
void cydui_esp32_touch_state(cydui_esp32 *dev, cydui_esp32_touch_info *out);

/** True when a calibration saved in the flash (or made by cydui_esp32_calibrate) is in use. */
bool cydui_esp32_calibrated(const cydui_esp32 *dev);

/** Forgets the saved calibration (and goes back to the default map). */
esp_err_t cydui_esp32_calibration_erase(cydui_esp32 *dev);

/**
 * Calibrates the touch screen with five targets (the four corners and the center), drawn with the
 * primitives of `ctx` (the context of the UI: &ui.ctx) and labeled with `font`. Each target is touched once;
 * the map is fitted by least squares, REFUSED if any target ends up farther than
 * CONFIG_CYDUI_ESP32_CAL_MAX_RESIDUAL pixels from where the map puts it (CT-06; it asks again, three times),
 * saved in the flash and used from then on. Returns ESP_OK, ESP_ERR_TIMEOUT (nobody touched a target for
 * timeout_ms: the previous map stays), ESP_ERR_INVALID_RESPONSE (three bad rounds) or the flash error.
 * It runs in the calling task and feeds the watchdog.
 */
esp_err_t cydui_esp32_calibrate(cydui_esp32 *dev, cydui_ctx *ctx, const cydui_font *font, uint32_t timeout_ms);

/**
 * Sleeps for up to `ms` milliseconds, or less: it returns as soon as a touch event is waiting, so an
 * application that has nothing to redraw uses no CPU (NFR-04). Returns true when an event is waiting and
 * false when the time ran out, in which case nothing has changed and there is no need to build a frame.
 * The task watchdog is fed.
 */
bool cydui_esp32_wait(cydui_esp32 *dev, uint32_t ms);

/**
 * Touch-to-picture latency (NFR-02) of the last CYDUI_LAT_MAX presses: from the moment the press was queued
 * (after the filter confirmed it, which adds about (CONFIG_CYDUI_ESP32_TOUCH_DOWN_TICKS - 1) sampling periods
 * to what the finger felt) to the end of the frame that showed it, in microseconds.
 */
void cydui_esp32_latency(const cydui_esp32 *dev, cydui_lat_stats *out);

/** Strips handed to the panel since the start. */
uint32_t cydui_esp32_flushes(const cydui_esp32 *dev);

/**
 * (CONFIG_CYDUI_ESP32_PIXEL_HASH) Starts or stops hashing the pixels of every strip sent; starting
 * resets the hash. cydui_esp32_hash returns the FNV-1a 64 of all the pixel bytes since, in the byte order
 * of cydui_mem_hash, so the number for a full frame can be compared with the PC.
 */
void cydui_esp32_hash_enable(cydui_esp32 *dev, bool on);
uint64_t cydui_esp32_hash(const cydui_esp32 *dev);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_ESP32_H */
