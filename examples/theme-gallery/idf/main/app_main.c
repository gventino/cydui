/*
 * theme-gallery on the CYD: the screens of examples/theme-gallery (the same gallery.c as the PC program)
 * on the real panel. At boot it can print the hash of every page in every theme and time a full redraw;
 * then it runs.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "cydui/ui.h"
#include "cydui_esp32.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cydui_sans_16.h"
#include "gallery.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#define ROWS 20 /* rows per strip: 12 strips; two buffers of 12.8 KB each */
#define W 320
#define H 240

static const char *TAG = "gallery";

/* All the memory of the UI: commands, text and state in one object, the strips apart (DMA, in internal RAM). */
CYDUI_STORAGE(storage, W, H, 1, 192, 1024, 32);
DMA_ATTR static cydui_color strips[2 * ROWS * W];
static uint64_t dirty[(H + ROWS - 1) / ROWS];
static cydui_esp32 dev;
static gallery g;
static cydui_ui ui;

static int64_t t_ui, t_end;
static uint32_t n_frames;

static bool one_frame(void)
{
    int64_t a = esp_timer_get_time(), b, c;
    bool more;

    cydui_ui_begin(&ui, cydui_theme_of(&ui)->style.bg);
    gallery_frame(&ui, &g);
    b = esp_timer_get_time();
    more = cydui_ui_end(&ui);
    c = esp_timer_get_time();
    gallery_apply(&ui, &g);
    t_ui += b - a;
    t_end += c - b;
    n_frames++;
    return more;
}

static void fill_touch(void)
{
    cydui_esp32_touch_info st;

    cydui_esp32_touch_state(&dev, &st);
    g.touch.known = cydui_esp32_touch_status(&dev) == ESP_OK;
    g.touch.down = st.down;
    g.touch.x = st.x;
    g.touch.y = st.y;
    g.touch.raw_x = st.raw_x;
    g.touch.raw_y = st.raw_y;
    g.touch.raw_z = st.raw_z;
}

static void calibrate(void)
{
    esp_err_t e = cydui_esp32_calibrate(&dev, &ui.ctx, &cydui_sans_16, 60000);

    printf("GALLERY_CALIBRATION result=%s\n", esp_err_to_name(e));
    g.check_n = 0;
}

static bool start_ui(int theme, int tab)
{
    cydui_ui_config cfg = CYDUI_CONFIG(storage, cydui_esp32_backend(&dev), &g.themes[0]);

    gallery_init(&g, theme, tab);
    cfg.theme = &g.themes[theme];
    cfg.core.strip_mem = strips;
    cfg.core.strip_mem_size = sizeof(strips);
    cfg.core.strip_buffers = 2;
    cfg.core.dirty_mem = dirty;
    cfg.core.dirty_mem_size = sizeof(dirty);
    cfg.frame_budget_ms = 0;
    memset(dirty, 0, sizeof(dirty));
    if (cydui_ui_init(&ui, &cfg) != 0) {
        ESP_LOGE(TAG, "cydui_ui_init failed");
        return false;
    }
    return true;
}

#if CONFIG_GALLERY_PRINT_HASH
/* The same states as `theme-gallery --headless --theme T --tab P --hash` on the PC: a new UI, six frames,
 * and the hash of what the sixth one sends (all the strips: it is invalidated first). */
static void print_hashes(void)
{
    int t, p, f;

    for (t = 0; t < GALLERY_THEMES; t++) {
        for (p = 0; p < GALLERY_PAGES; p++) {
            if (!start_ui(t, p)) {
                return;
            }
            for (f = 0; f < 5; f++) {
                one_frame();
            }
            cydui_invalidate(&ui.ctx);
            cydui_esp32_hash_enable(&dev, true);
            one_frame();
            cydui_esp32_hash_enable(&dev, false);
            printf("GALLERY_HASH theme=%d tab=%d %016" PRIx64 "\n", t, p, cydui_esp32_hash(&dev));
        }
    }
}
#endif

#if CONFIG_GALLERY_BENCH_SECONDS > 0
static void bench_full_redraw(void)
{
    int64_t start, now;
    uint32_t frames = 0, flushes0;

    if (!start_ui(1, 0)) {
        return;
    }
    for (int i = 0; i < 5; i++) {
        one_frame();
    }
    t_ui = t_end = 0;
    n_frames = 0;
    flushes0 = cydui_esp32_flushes(&dev);
    start = now = esp_timer_get_time();
    while (now - start < (int64_t)CONFIG_GALLERY_BENCH_SECONDS * 1000000) {
        cydui_invalidate(&ui.ctx); /* every strip, every frame: the worst case */
        one_frame();
        frames++;
        now = esp_timer_get_time();
    }
    printf("GALLERY_BENCH full_redraw spi=%dMHz frames=%" PRIu32 " fps=%.1f ui_us=%" PRId64 " render_and_send_us=%" PRId64
           " strips_per_frame=%.1f\n",
           CONFIG_CYDUI_ESP32_LCD_SPI_MHZ, frames, (double)frames * 1e6 / (double)(now - start), t_ui / (int64_t)frames,
           t_end / (int64_t)frames, (double)(cydui_esp32_flushes(&dev) - flushes0) / (double)frames);

    /* The two halves of that number, apart: drawing the strips alone, and sending strips alone. */
    {
        const cydui_backend *be = cydui_esp32_backend(&dev);
        int64_t t0, t1, t2;
        int k, y;

        t0 = esp_timer_get_time();
        for (k = 0; k < 20; k++) {
            for (y = 0; y < H; y += ROWS) {
                cydui_raster_strip(ui.ctx.cmds, cydui_cmd_count(&ui.ctx), strips, W, (int16_t)y, ROWS, 0);
            }
        }
        t1 = esp_timer_get_time();
        for (k = 0; k < 20; k++) {
            for (y = 0; y < H; y += ROWS) {
                cydui_rect area = {0, (int16_t)y, W, ROWS};
                be->flush_strip(be->user, &area, strips);
            }
            be->frame_end(be->user);
        }
        t2 = esp_timer_get_time();
        printf("GALLERY_BENCH parts: draw_only_us_per_frame=%" PRId64 " send_only_us_per_frame=%" PRId64 "\n", (t1 - t0) / 20,
               (t2 - t1) / 20);
    }
}
#endif

void app_main(void)
{
    cydui_esp32_config cfg = {.max_strip_bytes = ROWS * W * sizeof(cydui_color)};
    esp_err_t e = nvs_flash_init();
    int64_t last = esp_timer_get_time();
    bool checked = false;

    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);

    ESP_LOGI(TAG, "free heap before the backend: %u bytes", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    e = cydui_esp32_init(&dev, &cfg);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "the display did not start: %s", esp_err_to_name(e));
        return;
    }
    if (!start_ui(1, 0)) {
        return;
    }
    ESP_LOGI(TAG, "free heap after: %u bytes", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    one_frame();
    cydui_esp32_set_backlight(&dev, 100); /* only now, so that no garbage is ever seen */

#if CONFIG_GALLERY_PRINT_HASH
    print_hashes();
#endif
#if CONFIG_GALLERY_BENCH_SECONDS > 0
    bench_full_redraw();
#endif
    if (!start_ui(1, 0)) {
        return;
    }
    if (cydui_esp32_touch_status(&dev) == ESP_OK) {
#if CONFIG_GALLERY_FORCE_CALIBRATION
        calibrate();
#elif CONFIG_GALLERY_CALIBRATE_AT_BOOT
        if (!cydui_esp32_calibrated(&dev)) {
            calibrate();
        }
#endif
    } else {
        ESP_LOGW(TAG, "no touch: %s", esp_err_to_name(cydui_esp32_touch_status(&dev)));
    }
    for (;;) {
        bool more;
        int64_t now;

        fill_touch();
        more = one_frame() || gallery_animating(&g);
        if (g.calibrate_requested) {
            g.calibrate_requested = false;
            calibrate();
            more = true;
        }
        if (g.check_n >= GALLERY_CHECKS && !checked) { /* the four taps of the touch test, once */
            int sum = 0, worst = 0, i;
            for (i = 0; i < GALLERY_CHECKS; i++) {
                sum += g.check_err[i];
                worst = g.check_err[i] > worst ? g.check_err[i] : worst;
            }
            printf("GALLERY_TOUCH_CHECK err=%d,%d,%d,%d mean=%d max=%d px calibrated=%d\n", g.check_err[0], g.check_err[1],
                   g.check_err[2], g.check_err[3], sum / GALLERY_CHECKS, worst, cydui_esp32_calibrated(&dev));
            checked = true;
        } else if (g.check_n < GALLERY_CHECKS) {
            checked = false;
        }
        now = esp_timer_get_time();
        if (now - last > 2000000) {
            printf("GALLERY_STATS frames=%" PRIu32 " ui_us=%" PRId64 " end_us=%" PRId64 " strips_sent=%" PRIu32
                   " free_heap=%u min_free=%u\n",
                   n_frames, n_frames ? t_ui / n_frames : 0, n_frames ? t_end / n_frames : 0, cydui_esp32_flushes(&dev),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                   (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
            t_ui = t_end = 0;
            n_frames = 0;
            last = now;
        }
        if (!more) {
            cydui_esp32_wait(&dev, 1000); /* asleep until a touch event, or a second */
        }
    }
}
