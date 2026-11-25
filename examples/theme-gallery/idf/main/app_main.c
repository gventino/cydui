/*
 * theme-gallery on the CYD: the screens of examples/theme-gallery (the same gallery.c as the PC program) on
 * the real panel. At boot it can print the hash of every page in every theme (to compare with the PC), time
 * the speed of the drawing, the memory, the frame budget and the CPU use at rest (the GALLERY_* lines of the
 * serial log); then it runs, with the touch screen.
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "cydui/ui.h"
#include "cydui_esp32.h"
#include "cydui_sans_16.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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
    cydui_lat_stats lat;

    cydui_esp32_touch_state(&dev, &st);
    cydui_esp32_latency(&dev, &lat);
    g.touch.known = cydui_esp32_touch_status(&dev) == ESP_OK;
    g.touch.down = st.down;
    g.touch.x = st.x;
    g.touch.y = st.y;
    g.touch.raw_x = st.raw_x;
    g.touch.raw_y = st.raw_y;
    g.touch.raw_z = st.raw_z;
    g.touch.lat_n = lat.n;
    g.touch.lat_p50_us = lat.p50;
    g.touch.lat_p95_us = lat.p95;
    g.touch.lat_max_us = lat.max;
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
/* Frames as fast as they go for a few seconds. `page` 0 is the page of controls, 4 the animation; with
 * `all_strips` every frame is invalidated first (every strip is drawn and sent, the worst case). */
static void bench(const char *name, int page, bool all_strips)
{
    int64_t start, now;
    uint32_t frames = 0, flushes0;
    int i;

    if (!start_ui(1, page)) {
        return;
    }
    for (i = 0; i < 6; i++) {
        one_frame();
    }
    t_ui = t_end = 0;
    n_frames = 0;
    flushes0 = cydui_esp32_flushes(&dev);
    start = now = esp_timer_get_time();
    while (now - start < (int64_t)CONFIG_GALLERY_BENCH_SECONDS * 1000000) {
        if (all_strips) {
            cydui_invalidate(&ui.ctx);
        }
        one_frame();
        frames++;
        now = esp_timer_get_time();
    }
    printf("GALLERY_BENCH %s spi=%dMHz frames=%" PRIu32 " fps=%.1f ui_us=%" PRId64 " end_us=%" PRId64 " strips_per_frame=%.2f\n",
           name, CONFIG_CYDUI_ESP32_LCD_SPI_MHZ, frames, (double)frames * 1e6 / (double)(now - start), t_ui / (int64_t)frames,
           t_end / (int64_t)frames, (double)(cydui_esp32_flushes(&dev) - flushes0) / (double)frames);
    if (all_strips) { /* the two halves of that number, apart: drawing the strips alone, and sending strips alone */
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
        printf("GALLERY_BENCH %s parts: draw_only_us_per_frame=%" PRId64 " send_only_us_per_frame=%" PRId64 "\n", name,
               (t1 - t0) / 20, (t2 - t1) / 20);
    }
}
#endif

#if CONFIG_GALLERY_PROFILE
/* ---- the cost of each kind of command on its own: one strip of 320 x 20, drawn 50 times ---- */

static const uint8_t mask20[200] = { /* a 20 x 20 mask of 4 bits per pixel: a diagonal band */
#define R(n) 0xFF, 0xFF, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFF
    R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0), R(0)
#undef R
};

typedef struct {
    const char *name;
    int area; /* pixels the command covers, to turn the time into a time per pixel */
    void (*draw)(cydui_ctx *c);
} probe_t;

static void p_clear(cydui_ctx *c) { (void)c; }
static void p_rect(cydui_ctx *c) { cydui_draw_rect(c, (cydui_rect){10, 0, 300, 20}, 0xF800, 255); }
static void p_rect_alpha(cydui_ctx *c) { cydui_draw_rect(c, (cydui_rect){10, 0, 300, 20}, 0xF800, 128); }
static void p_rrect(cydui_ctx *c) { cydui_draw_rrect(c, (cydui_rect){10, 0, 300, 20}, 8, 0x07E0, 255); }
static void p_outline(cydui_ctx *c) { cydui_draw_rrect_outline(c, (cydui_rect){10, 0, 300, 20}, 8, 2, 0x07E0, 255); }
static void p_line(cydui_ctx *c) { cydui_draw_line(c, 0, 0, 319, 19, 2, 0xFFFF, 255); }
static void p_gradient(cydui_ctx *c) { cydui_draw_gradient(c, (cydui_rect){10, 0, 300, 20}, 0xF800, 0x001F, true, 255); }
static void p_bitmap(cydui_ctx *c)
{
    cydui_draw_bitmap(c, (cydui_rect){10, 0, 20, 20}, mask20, sizeof(mask20), CYDUI_BITMAP_A4, 0xFFFF, 255);
}
static void p_text(cydui_ctx *c)
{
    cydui_draw_text(c, &cydui_sans_16, 10, 0, 0, CYDUI_ALIGN_LEFT, "The quick brown fox jumps", CYDUI_NTS, 0xFFFF, 255);
}

static void profile_commands(void)
{
    static const probe_t probes[] = {
        {"clear_only", 0, p_clear},        {"rect_opaque_300x20", 6000, p_rect}, {"rect_alpha_300x20", 6000, p_rect_alpha},
        {"rrect_filled_300x20", 6000, p_rrect}, {"rrect_outline_300x20", 6000, p_outline}, {"line_2px_diagonal", 0, p_line},
        {"gradient_300x20", 6000, p_gradient}, {"bitmap_a4_20x20", 400, p_bitmap}, {"text_16px_25_chars", 0, p_text},
    };
    size_t i;

    if (!start_ui(1, 0)) {
        return;
    }
    for (i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        int64_t t0, t1;
        int k;

        cydui_begin_frame(&ui.ctx, 0);
        probes[i].draw(&ui.ctx);
        t0 = esp_timer_get_time();
        for (k = 0; k < 50; k++) {
            cydui_raster_strip(ui.ctx.cmds, cydui_cmd_count(&ui.ctx), strips, W, 0, ROWS, 0);
        }
        t1 = esp_timer_get_time();
        printf("GALLERY_PROFILE %-22s commands=%u us_per_strip=%.1f", probes[i].name, (unsigned)cydui_cmd_count(&ui.ctx),
               (double)(t1 - t0) / 50.0);
        if (probes[i].area) {
            printf(" ns_per_pixel=%.0f", (double)(t1 - t0) * 1000.0 / 50.0 / probes[i].area);
        }
        printf("\n");
    }
}
#endif

#if CONFIG_GALLERY_MEMORY_REPORT
static void memory_report(void)
{
    printf("GALLERY_MEMORY bytes: ui_storage=%u strips=%u dirty=%u gallery(3 themes)=%u cydui_ui=%u cydui_esp32=%u cydui_theme=%u cydui_state=%u\n",
           (unsigned)sizeof(storage), (unsigned)sizeof(strips), (unsigned)sizeof(dirty), (unsigned)sizeof(g), (unsigned)sizeof(ui),
           (unsigned)sizeof(dev), (unsigned)sizeof(cydui_theme), (unsigned)sizeof(cydui_state));
    printf("GALLERY_MEMORY heap: internal_free=%u largest_block=%u minimum_ever=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
}
#endif

#if CONFIG_GALLERY_BUDGET_TEST
/* CT-15: a frame with a budget of 1 ms that takes 3 ms before its widgets are called builds none of them. */
static void budget_test(void)
{
    if (!start_ui(1, 0)) {
        return;
    }
    one_frame();
    one_frame();
    cydui_ui_clear_errors(&ui);
    ui.frame_budget_ms = 1;
    cydui_ui_begin(&ui, cydui_theme_of(&ui)->style.bg);
    esp_rom_delay_us(3000);
    gallery_frame(&ui, &g);
    cydui_ui_end(&ui);
    printf("GALLERY_BUDGET slow_frame errors=0x%x (0x200 is CYDUI_ERR_FRAME_BUDGET) widgets_built=%u\n",
           (unsigned)cydui_ui_errors(&ui), (unsigned)ui.widgets);
    cydui_ui_clear_errors(&ui);
    ui.frame_budget_ms = 20;
    cydui_invalidate(&ui.ctx);
    one_frame();
    printf("GALLERY_BUDGET normal_frame errors=0x%x widgets_built=%u\n", (unsigned)cydui_ui_errors(&ui), (unsigned)ui.widgets);
    ui.frame_budget_ms = 0;
}
#endif

#if CONFIG_GALLERY_IDLE_SECONDS > 0
/* NFR-04: nothing to draw and nobody touching: how much CPU does every task take? */
static void idle_report(void)
{
    static TaskStatus_t a[24], b[24];
    UBaseType_t na, nb, i, j;
    uint32_t t1, t2, sent0;
    int64_t end;
    uint32_t wakeups = 0;

    if (!start_ui(1, 0)) {
        return;
    }
    for (i = 0; i < 8; i++) {
        one_frame();
    }
    sent0 = cydui_esp32_flushes(&dev);
    na = uxTaskGetSystemState(a, 24, &t1);
    end = esp_timer_get_time() + (int64_t)CONFIG_GALLERY_IDLE_SECONDS * 1000000;
    while (esp_timer_get_time() < end) { /* the loop of the application, with nothing happening */
        if (!cydui_esp32_wait(&dev, 1000)) {
            wakeups++;
            continue; /* time ran out and nothing changed: no frame needed */
        }
    }
    nb = uxTaskGetSystemState(b, 24, &t2);
    printf("GALLERY_IDLE seconds=%d strips_sent=%u wakeups=%u\n", CONFIG_GALLERY_IDLE_SECONDS,
           (unsigned)(cydui_esp32_flushes(&dev) - sent0), (unsigned)wakeups);
    for (i = 0; i < nb; i++) {
        for (j = 0; j < na; j++) {
            if (a[j].xHandle == b[i].xHandle) {
                /* the counter is one clock for both cores: a task that ran all the time on its core takes 100 % */
                double pct = 100.0 * (double)(b[i].ulRunTimeCounter - a[j].ulRunTimeCounter) / (double)(t2 - t1);
                printf("GALLERY_CPU task=%-12s pct=%.2f\n", b[i].pcTaskName, pct);
            }
        }
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
    bench("full_redraw", 0, true);
    bench("animation_dirty_regions", 4, false);
    bench("animation_all_strips", 4, true);
#endif
#if CONFIG_GALLERY_PROFILE
    profile_commands();
#endif
#if CONFIG_GALLERY_MEMORY_REPORT
    memory_report();
#endif
#if CONFIG_GALLERY_BUDGET_TEST
    budget_test();
#endif
#if CONFIG_GALLERY_IDLE_SECONDS > 0
    idle_report();
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
    cydui_invalidate(&ui.ctx);
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
            cydui_lat_stats lat;
            cydui_esp32_latency(&dev, &lat);
            printf("GALLERY_STATS frames=%" PRIu32 " ui_us=%" PRId64 " end_us=%" PRId64 " strips_sent=%" PRIu32
                   " free_heap=%u lat_n=%d lat_p50_ms=%.1f lat_p95_ms=%.1f lat_max_ms=%.1f\n",
                   n_frames, n_frames ? t_ui / n_frames : 0, n_frames ? t_end / n_frames : 0, cydui_esp32_flushes(&dev),
                   (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), lat.n, lat.p50 / 1000.0, lat.p95 / 1000.0,
                   lat.max / 1000.0);
            t_ui = t_end = 0;
            n_frames = 0;
            last = now;
        }
        if (!more && !cydui_esp32_wait(&dev, 1000)) {
            continue; /* a second passed with nothing happening: nothing to draw */
        }
    }
}
