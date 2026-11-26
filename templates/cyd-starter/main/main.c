/*
 * @NAME@ - a starting point for the ESP32 "Cheap Yellow Display" with cydui.
 * A label and a button with a counter. Change build_screen() and keep going.
 */

#include <stdio.h>

#include "cydui/ui.h"
#include "cydui_esp32.h"
#include "cydui_sans_16.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define W 320
#define H 240
#define ROWS 20 /* rows per strip: two buffers of 12.8 KB; taller is faster, the picture is the same */

static const char *TAG = "@NAME@";

/* All the memory of the UI, sized once: screen, rows per strip, commands per frame, text bytes, state entries.
 * cydui_ui_stats_get() tells how much a screen really uses; see docs/12-guide.md. */
CYDUI_STORAGE(storage, W, H, 1, 192, 1024, 32);
DMA_ATTR static cydui_color strips[2 * ROWS * W];
static uint64_t dirty[(H + ROWS - 1) / ROWS]; /* only the strips that changed are sent */

static cydui_esp32 dev;
static cydui_ui ui;
static cydui_theme theme;

static void build_screen(void)
{
    static int clicks;
    char text[32];

    snprintf(text, sizeof text, "clicks: %d", clicks);
    cydui_label(&ui, text);
    if (cydui_button(&ui, "Click me")) {
        clicks++;
    }
}

void app_main(void)
{
    cydui_esp32_config cfg = {.max_strip_bytes = ROWS * W * sizeof(cydui_color)};
    cydui_ui_config ucfg;
    esp_err_t e = nvs_flash_init(); /* the touch calibration lives in the flash */

    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);

    e = cydui_esp32_init(&dev, &cfg);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "the display did not start: %s", esp_err_to_name(e));
        return;
    }
    cydui_theme_default(&theme, &cydui_sans_16);
    ucfg = (cydui_ui_config)CYDUI_CONFIG(storage, cydui_esp32_backend(&dev), &theme);
    ucfg.core.strip_mem = strips;
    ucfg.core.strip_mem_size = sizeof(strips);
    ucfg.core.strip_buffers = 2;
    ucfg.core.dirty_mem = dirty;
    ucfg.core.dirty_mem_size = sizeof(dirty);
    if (cydui_ui_init(&ui, &ucfg) != 0) {
        ESP_LOGE(TAG, "cydui_ui_init failed");
        return;
    }

    cydui_ui_begin(&ui, theme.style.bg); /* the first picture, before the backlight comes on */
    build_screen();
    cydui_ui_end(&ui);
    cydui_esp32_set_backlight(&dev, 100);

    if (cydui_esp32_touch_status(&dev) == ESP_OK && !cydui_esp32_calibrated(&dev)) {
        /* five touches on crosses, once; a stylus is better than a finger. Saved in the flash. */
        cydui_esp32_calibrate(&dev, &ui.ctx, &cydui_sans_16, 60000);
    }

    for (;;) {
        cydui_ui_begin(&ui, theme.style.bg);
        build_screen();
        if (!cydui_ui_end(&ui)) {
            cydui_esp32_wait(&dev, 250); /* nothing moves: sleep until a touch (or a quarter of a second) */
        }
    }
}
