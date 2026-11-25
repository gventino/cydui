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
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

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
