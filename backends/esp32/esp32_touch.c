/*
 * The touch screen of the ESP32 backend: an XPT2046 on its own SPI bus, sampled by a task of its own while
 * the screen is touched (so the touch does not depend on how long a frame takes), filtered
 * (touch_filter.c), mapped to pixels by the calibration (touch_cal.c) and handed to the UI through a queue.
 * The calibration is made on screen with five targets and kept in the flash.
 */

#include <string.h>

#include "driver/gpio.h"
#include "esp32_internal.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "nvs.h"
#include "pins.h"
#include "sdkconfig.h"

static const char *TAG = "cydui_touch";

#define SCREEN_W 320
#define SCREEN_H 240

#ifdef CONFIG_CYDUI_ESP32_TOUCH_SWAP_XY
#define TOUCH_SWAP 1
#else
#define TOUCH_SWAP 0
#endif
#ifdef CONFIG_CYDUI_ESP32_TOUCH_FLIP_X
#define TOUCH_FLIP_X 1
#else
#define TOUCH_FLIP_X 0
#endif
#ifdef CONFIG_CYDUI_ESP32_TOUCH_FLIP_Y
#define TOUCH_FLIP_Y 1
#else
#define TOUCH_FLIP_Y 0
#endif
#ifdef CONFIG_CYDUI_ESP32_LCD_ROTATION_180
#define SCREEN_ROTATED 1
#else
#define SCREEN_ROTATED 0
#endif

/* ---- pin checks (CT-14) ---- */

CYDUI_PIN_REQUIRE_OUTPUT(touch_clk, CONFIG_CYDUI_ESP32_TOUCH_CLK);
CYDUI_PIN_REQUIRE_OUTPUT(touch_mosi, CONFIG_CYDUI_ESP32_TOUCH_MOSI);
CYDUI_PIN_REQUIRE_OUTPUT(touch_cs, CONFIG_CYDUI_ESP32_TOUCH_CS);
CYDUI_PIN_REQUIRE_INPUT(touch_miso, CONFIG_CYDUI_ESP32_TOUCH_MISO);
CYDUI_PIN_REQUIRE_INPUT(touch_irq, CONFIG_CYDUI_ESP32_TOUCH_IRQ);
CYDUI_PINS_ASSERT(CONFIG_CYDUI_ESP32_TOUCH_HOST != CONFIG_CYDUI_ESP32_LCD_HOST, the_touch_and_the_display_need_different_spi_hosts);

#ifndef CONFIG_CYDUI_ESP32_PROFILE_CYD
#if CYDUI_PIN_IS_STRAPPING(CONFIG_CYDUI_ESP32_TOUCH_CLK) || CYDUI_PIN_IS_STRAPPING(CONFIG_CYDUI_ESP32_TOUCH_MOSI) || \
    CYDUI_PIN_IS_STRAPPING(CONFIG_CYDUI_ESP32_TOUCH_CS)
#warning "a touch pin is a strapping pin (0, 2, 5, 12, 15): fine after boot on the classic CYD, suspicious elsewhere"
#endif
#endif

#define TOUCH_HOST ((spi_host_device_t)CONFIG_CYDUI_ESP32_TOUCH_HOST)
#define IRQ_PIN CONFIG_CYDUI_ESP32_TOUCH_IRQ

/* ---- the controller ---- */

static uint16_t xpt_read(spi_device_handle_t h, uint8_t cmd)
{
    uint8_t tx[3] = {cmd, 0, 0}, rx[3] = {0};
    spi_transaction_t t = {.length = 24, .tx_buffer = tx, .rx_buffer = rx};

    if (spi_device_polling_transmit(h, &t) != ESP_OK) {
        return 0;
    }
    return (uint16_t)(((rx[1] << 8) | rx[2]) >> 3) & 0x0FFF;
}

/* A burst of n conversions of each position, and one pressure. The first conversion after changing
 * channel is polluted by the plate voltage of the previous one (found in iteration 0): it is thrown away. */
static int sample_burst(cydui_esp32 *d, cydui_touch_read *r, int n)
{
    int i;
    int32_t z1, z2, z;

    if (n > CYDUI_TOUCH_MAX_READS) n = CYDUI_TOUCH_MAX_READS;
    (void)xpt_read(d->touch, 0xD0);
    for (i = 0; i < n; i++) r[i].x = xpt_read(d->touch, 0xD0);
    (void)xpt_read(d->touch, 0x90);
    for (i = 0; i < n; i++) r[i].y = xpt_read(d->touch, 0x90);
    (void)xpt_read(d->touch, 0xB0);
    z1 = xpt_read(d->touch, 0xB0);
    (void)xpt_read(d->touch, 0xC0);
    z2 = xpt_read(d->touch, 0xC0);
    z = z1 + 4095 - z2; /* pressure: 1100..2800 with a finger */
    z = z < 0 ? 0 : (z > 8190 ? 8190 : z);
    for (i = 0; i < n; i++) r[i].z = (uint16_t)z;
    return n;
}

/* ---- the calibration in the flash ---- */

static void load_calibration(cydui_esp32 *d)
{
    nvs_handle_t h;
    uint8_t blob[CYDUI_CAL_BLOB_SIZE];
    size_t len = sizeof(blob);
    cydui_touch_cal c;

    if (nvs_open("cydui", NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    if (nvs_get_blob(h, "touch_cal", blob, &len) == ESP_OK && cydui_cal_unpack(&c, blob, len)) {
        d->cal = c;
        d->calibrated = true;
        ESP_LOGI(TAG, "calibration loaded from the flash");
    } else {
        ESP_LOGW(TAG, "no valid calibration in the flash: using the default map");
    }
    nvs_close(h);
}

static esp_err_t save_calibration(const cydui_touch_cal *c)
{
    nvs_handle_t h;
    uint8_t blob[CYDUI_CAL_BLOB_SIZE];
    esp_err_t e = nvs_open("cydui", NVS_READWRITE, &h);

    if (e != ESP_OK) {
        return e;
    }
    cydui_cal_pack(c, blob);
    e = nvs_set_blob(h, "touch_cal", blob, sizeof(blob));
    if (e == ESP_OK) {
        e = nvs_commit(h);
    }
    nvs_close(h);
    return e;
}

static void default_map(cydui_touch_cal *c)
{
    cydui_cal_default(c, SCREEN_W, SCREEN_H, CONFIG_CYDUI_ESP32_TOUCH_RAW_MIN, CONFIG_CYDUI_ESP32_TOUCH_RAW_MAX,
                      TOUCH_SWAP != 0, (TOUCH_FLIP_X ^ SCREEN_ROTATED) != 0, (TOUCH_FLIP_Y ^ SCREEN_ROTATED) != 0);
}

esp_err_t cydui_esp32_calibration_erase(cydui_esp32 *d)
{
    nvs_handle_t h;
    esp_err_t e = nvs_open("cydui", NVS_READWRITE, &h);

    if (e == ESP_OK) {
        e = nvs_erase_key(h, "touch_cal");
        if (e == ESP_ERR_NVS_NOT_FOUND) e = ESP_OK;
        if (e == ESP_OK) e = nvs_commit(h);
        nvs_close(h);
    }
    portENTER_CRITICAL(&d->touch_lock);
    default_map(&d->cal);
    d->calibrated = false;
    portEXIT_CRITICAL(&d->touch_lock);
    return e;
}

/* ---- the sampling task ---- */

static void IRAM_ATTR touch_isr(void *arg)
{
    cydui_esp32 *d = (cydui_esp32 *)arg;
    BaseType_t woken = pdFALSE;

    xSemaphoreGiveFromISR(d->touch_irq, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

/* What goes through the queue: the event and the moment it was queued (for the latency). */
typedef struct {
    cydui_event ev;
    int64_t t_us;
} touch_item;

static void queue_event(cydui_esp32 *d, const cydui_event *ev)
{
    touch_item it;

    it.ev = *ev;
    it.t_us = esp_timer_get_time();
    if (xQueueSend(d->touch_q, &it, 0) != pdTRUE) { /* full: the oldest goes, the newest state is what counts */
        touch_item old;
        (void)xQueueReceive(d->touch_q, &old, 0);
        (void)xQueueSend(d->touch_q, &it, 0);
    }
}

static void touch_task(void *arg)
{
    cydui_esp32 *d = (cydui_esp32 *)arg;
    cydui_touch_read reads[CYDUI_TOUCH_MAX_READS];
    TickType_t period = pdMS_TO_TICKS(CONFIG_CYDUI_ESP32_TOUCH_PERIOD_MS);
    uint32_t seen = 0;

    if (period < 1) period = 1;
    for (;;) {
        bool busy = d->filter.down || d->filter.press_run || d->filter.release_run || d->filter.jump_pending;
        bool irq_low;
        int n = 0;
        cydui_event raw;

        if (!busy && gpio_get_level(IRQ_PIN) != 0) {
            (void)xSemaphoreTake(d->touch_irq, pdMS_TO_TICKS(250)); /* asleep until the pen interrupt */
        }
        irq_low = gpio_get_level(IRQ_PIN) == 0;
        if (irq_low) {
            n = sample_burst(d, reads, CONFIG_CYDUI_ESP32_TOUCH_READS);
        }
        if (cydui_touch_tick(&d->filter, reads, n, irq_low, &raw)) {
            cydui_touch_cal cal;
            int16_t x, y;
            cydui_event ev;

            portENTER_CRITICAL(&d->touch_lock);
            cal = d->cal;
            portEXIT_CRITICAL(&d->touch_lock);
            cydui_cal_apply(&cal, raw.x, raw.y, SCREEN_W, SCREEN_H, &x, &y);
            ev.type = raw.type;
            ev.x = x;
            ev.y = y;
            queue_event(d, &ev);
        }
        if (d->filter.samples != seen) { /* a new steady sample: for the diagnostics and the calibration */
            cydui_touch_cal cal;
            int16_t x, y;

            seen = d->filter.samples;
            portENTER_CRITICAL(&d->touch_lock);
            cal = d->cal;
            portEXIT_CRITICAL(&d->touch_lock);
            cydui_cal_apply(&cal, d->filter.sample_x, d->filter.sample_y, SCREEN_W, SCREEN_H, &x, &y);
            portENTER_CRITICAL(&d->touch_lock);
            d->raw_x = d->filter.sample_x;
            d->raw_y = d->filter.sample_y;
            d->raw_z = d->filter.sample_z;
            d->px = x;
            d->py = y;
            d->sample_count = seen;
            portEXIT_CRITICAL(&d->touch_lock);
        }
        d->pressed = d->filter.down;
        vTaskDelay(period);
    }
}

static esp_err_t touch_try_start(cydui_esp32 *d)
{
    spi_bus_config_t bus = {
        .mosi_io_num = CONFIG_CYDUI_ESP32_TOUCH_MOSI,
        .miso_io_num = CONFIG_CYDUI_ESP32_TOUCH_MISO,
        .sclk_io_num = CONFIG_CYDUI_ESP32_TOUCH_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    spi_device_interface_config_t dev = {
        .clock_speed_hz = CONFIG_CYDUI_ESP32_TOUCH_KHZ * 1000,
        .mode = 0,
        .spics_io_num = CONFIG_CYDUI_ESP32_TOUCH_CS,
        .queue_size = 1,
    };
    gpio_config_t irq = {.pin_bit_mask = 1ull << IRQ_PIN, .mode = GPIO_MODE_INPUT, .intr_type = GPIO_INTR_NEGEDGE};
    cydui_touch_cfg cfg;
    esp_err_t e;

    cfg.z_on = CONFIG_CYDUI_ESP32_TOUCH_Z_ON;
    cfg.z_off = CONFIG_CYDUI_ESP32_TOUCH_Z_OFF;
    cfg.down_ticks = CONFIG_CYDUI_ESP32_TOUCH_DOWN_TICKS;
    cfg.up_ticks = CONFIG_CYDUI_ESP32_TOUCH_UP_TICKS;
    cfg.move_min = CONFIG_CYDUI_ESP32_TOUCH_MOVE_MIN;
    cfg.max_spread = CONFIG_CYDUI_ESP32_TOUCH_MAX_SPREAD;
    cfg.max_jump = CONFIG_CYDUI_ESP32_TOUCH_MAX_JUMP;
    cydui_touch_filter_init(&d->filter, &cfg);
    default_map(&d->cal);
    d->touch_lock = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED;

    ESP_RETURN_ON_ERROR(spi_bus_initialize(TOUCH_HOST, &bus, SPI_DMA_DISABLED), TAG, "SPI bus of the touch controller");
    ESP_RETURN_ON_ERROR(spi_bus_add_device(TOUCH_HOST, &dev, &d->touch), TAG, "SPI device of the touch controller");
    d->touch_q = xQueueCreate(16, sizeof(touch_item));
    d->touch_irq = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(d->touch_q && d->touch_irq, ESP_ERR_NO_MEM, TAG, "queue");
    ESP_RETURN_ON_ERROR(gpio_config(&irq), TAG, "pen interrupt pin");
#if !CYDUI_PIN_IS_INPUT_ONLY(CONFIG_CYDUI_ESP32_TOUCH_IRQ)
    gpio_pullup_en(IRQ_PIN); /* a controller that is not there then reads as "not touched" (input-only pins have none: the CYD has one on the board) */
#endif
    e = gpio_install_isr_service(0);
    if (e != ESP_OK && e != ESP_ERR_INVALID_STATE) { /* already installed by the application: fine */
        return e;
    }
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(IRQ_PIN, touch_isr, d), TAG, "pen interrupt handler");
    load_calibration(d);
    ESP_RETURN_ON_FALSE(xTaskCreate(touch_task, "cydui_touch", 4096, d, 5, &d->touch_task) == pdPASS, ESP_ERR_NO_MEM, TAG,
                        "touch task");
    return ESP_OK;
}

void cydui_esp32_touch_start(cydui_esp32 *d)
{
    d->touch_status = touch_try_start(d);
    if (d->touch_status != ESP_OK) {
        ESP_LOGE(TAG, "the touch screen did not start (%s): the display works, there will be no touch",
                 esp_err_to_name(d->touch_status));
    } else {
        ESP_LOGI(TAG, "touch started: %s map", d->calibrated ? "calibrated" : "default");
    }
}

bool cydui_esp32_poll_event(void *user, cydui_event *ev)
{
    cydui_esp32 *d = (cydui_esp32 *)user;
    touch_item it;

    if (!d->touch_q || xQueueReceive(d->touch_q, &it, 0) != pdTRUE) {
        return false;
    }
    if (it.ev.type == CYDUI_EVENT_DOWN) {
        d->down_pending_us = it.t_us; /* closed by the end of the frame that shows it (frame_end) */
    }
    *ev = it.ev;
    return true;
}

esp_err_t cydui_esp32_touch_status(const cydui_esp32 *d)
{
    return d->touch_status;
}

bool cydui_esp32_calibrated(const cydui_esp32 *d)
{
    return d->calibrated;
}

void cydui_esp32_touch_state(cydui_esp32 *d, cydui_esp32_touch_info *out)
{
    portENTER_CRITICAL(&d->touch_lock);
    out->raw_x = d->raw_x;
    out->raw_y = d->raw_y;
    out->raw_z = d->raw_z;
    out->x = d->px;
    out->y = d->py;
    out->samples = d->sample_count;
    portEXIT_CRITICAL(&d->touch_lock);
    out->down = d->pressed;
}

bool cydui_esp32_wait(cydui_esp32 *d, uint32_t ms)
{
    touch_item it;

    while (ms > 0) {
        uint32_t chunk = ms > 500 ? 500 : ms;

        (void)esp_task_wdt_reset(); /* an application that sleeps is alive: not subscribed is fine */
        if (d->touch_q) {
            if (xQueuePeek(d->touch_q, &it, pdMS_TO_TICKS(chunk)) == pdTRUE) {
                return true; /* a touch event is waiting */
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(chunk));
        }
        ms -= chunk;
    }
    return false;
}

/* ---- the calibration on screen ---- */

#define CAL_POINTS 5
#define CAL_SAMPLES 8
#define CAL_SKIP 2 /* the first samples of a touch, while the finger settles */

static void cal_screen(cydui_ctx *ctx, const cydui_font *font, int point, const char *line, bool cross)
{
    static const int tx[CAL_POINTS] = {24, SCREEN_W - 24, 24, SCREEN_W - 24, SCREEN_W / 2};
    static const int ty[CAL_POINTS] = {24, 24, SCREEN_H - 24, SCREEN_H - 24, SCREEN_H / 2};
    cydui_text_extent e = cydui_text_size(font, line, strlen(line), 0);
    const cydui_color white = CYDUI_RGB(255, 255, 255), red = CYDUI_RGB(255, 60, 60);

    cydui_begin_frame(ctx, CYDUI_RGB(8, 12, 24));
    if (cross) {
        int x = tx[point], y = ty[point];
        cydui_draw_line(ctx, (int16_t)(x - 14), (int16_t)y, (int16_t)(x + 14), (int16_t)y, 2, white, 255);
        cydui_draw_line(ctx, (int16_t)x, (int16_t)(y - 14), (int16_t)x, (int16_t)(y + 14), 2, white, 255);
        cydui_draw_rrect(ctx, (cydui_rect){(int16_t)(x - 2), (int16_t)(y - 2), 5, 5}, 2, red, 255);
    }
    cydui_draw_text(ctx, font, (int16_t)((SCREEN_W - e.w) / 2), (int16_t)(point == 4 ? 60 : 100), 0, CYDUI_ALIGN_LEFT, line,
                    strlen(line), white, 255);
    cydui_end_frame(ctx);
}

/* Waits for a touch and measures it; ESP_OK with the raw position, or ESP_ERR_TIMEOUT. */
static esp_err_t cal_point(cydui_esp32 *d, uint32_t timeout_ms, float *rx, float *ry)
{
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    TickType_t period = pdMS_TO_TICKS(CONFIG_CYDUI_ESP32_TOUCH_PERIOD_MS);

    if (period < 1) period = 1;
    for (;;) {
        touch_item ev;
        uint32_t last;
        int n = 0;
        double sx = 0, sy = 0;
        bool lost = false;

        xQueueReset(d->touch_q);
        for (;;) { /* a finger goes down */
            if (xQueueReceive(d->touch_q, &ev, pdMS_TO_TICKS(100)) == pdTRUE && ev.ev.type == CYDUI_EVENT_DOWN) {
                break;
            }
            (void)esp_task_wdt_reset();
            if (esp_timer_get_time() > deadline) {
                return ESP_ERR_TIMEOUT;
            }
        }
        portENTER_CRITICAL(&d->touch_lock);
        last = d->sample_count;
        portEXIT_CRITICAL(&d->touch_lock);
        while (n < CAL_SAMPLES + CAL_SKIP && !lost) { /* average the samples while it is held */
            uint32_t now;
            uint16_t x, y;

            vTaskDelay(period);
            (void)esp_task_wdt_reset();
            portENTER_CRITICAL(&d->touch_lock);
            now = d->sample_count;
            x = d->raw_x;
            y = d->raw_y;
            portEXIT_CRITICAL(&d->touch_lock);
            if (!d->pressed) {
                lost = true; /* it was lifted too soon: wait for another */
            } else if (now != last) {
                last = now;
                if (n >= CAL_SKIP) {
                    sx += x;
                    sy += y;
                }
                n++;
            }
            if (esp_timer_get_time() > deadline) {
                return ESP_ERR_TIMEOUT;
            }
        }
        {
            int64_t lift_by = esp_timer_get_time() + 5000000;
            while (d->pressed && esp_timer_get_time() < lift_by) { /* wait for it to be lifted (not forever) */
                vTaskDelay(period);
                (void)esp_task_wdt_reset();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(150));
        if (!lost) {
            *rx = (float)(sx / CAL_SAMPLES);
            *ry = (float)(sy / CAL_SAMPLES);
            return ESP_OK;
        }
    }
}

esp_err_t cydui_esp32_calibrate(cydui_esp32 *d, cydui_ctx *ctx, const cydui_font *font, uint32_t timeout_ms)
{
    static const float tx[CAL_POINTS] = {24, SCREEN_W - 24, 24, SCREEN_W - 24, SCREEN_W / 2};
    static const float ty[CAL_POINTS] = {24, 24, SCREEN_H - 24, SCREEN_H - 24, SCREEN_H / 2};
    int attempt, i;
    esp_err_t e;

    if (!d || !ctx || !font || d->touch_status != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    for (attempt = 0; attempt < 3; attempt++) {
        cydui_cal_sample pts[CAL_POINTS];
        cydui_touch_cal fit;
        cydui_cal_status st;
        float worst = 0;

        for (i = 0; i < CAL_POINTS; i++) {
            char line[40];
            float rx = 0, ry = 0;

            snprintf(line, sizeof(line), attempt ? "Tap the cross (%d/5), again" : "Tap the cross (%d/5)", i + 1);
            cal_screen(ctx, font, i, line, true);
            e = cal_point(d, timeout_ms, &rx, &ry);
            if (e != ESP_OK) {
                cydui_invalidate(ctx);
                return e;
            }
            pts[i] = (cydui_cal_sample){rx, ry, tx[i], ty[i]};
        }
        st = cydui_cal_fit(&fit, pts, CAL_POINTS, (float)CONFIG_CYDUI_ESP32_CAL_MAX_RESIDUAL, &worst);
        if (st == CYDUI_CAL_OK) {
            e = save_calibration(&fit);
            if (e != ESP_OK) {
                ESP_LOGE(TAG, "the calibration was good but could not be saved: %s", esp_err_to_name(e));
            }
            portENTER_CRITICAL(&d->touch_lock);
            d->cal = fit;
            d->calibrated = true;
            portEXIT_CRITICAL(&d->touch_lock);
            ESP_LOGI(TAG, "calibrated: worst target %.1f px; map x=%.5f*rx%+.5f*ry%+.2f y=%.5f*rx%+.5f*ry%+.2f", (double)worst,
                     (double)fit.m[0], (double)fit.m[1], (double)fit.m[2], (double)fit.m[3], (double)fit.m[4], (double)fit.m[5]);
            cal_screen(ctx, font, 4, "Calibrated", false);
            vTaskDelay(pdMS_TO_TICKS(600));
            cydui_invalidate(ctx);
            return e;
        }
        ESP_LOGW(TAG, "calibration refused (status %d, worst target %.1f px): again", (int)st, (double)worst);
        cal_screen(ctx, font, 4, "Error too big: again", false);
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
    cydui_invalidate(ctx);
    return ESP_ERR_INVALID_RESPONSE;
}
