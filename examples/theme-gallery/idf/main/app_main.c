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

/* A Kconfig bool that is off is not defined at all: say 0, so that it can be used in an expression too. */
#ifndef CONFIG_GALLERY_ACCEPTANCE
#define CONFIG_GALLERY_ACCEPTANCE 0
#endif

#define ROWS 20 /* rows per strip: 12 strips; two buffers of 12.8 KB each */
#define W 320
#define H 240

static const char *TAG = "gallery";

/* All the memory of the UI: commands, text and state in one object, the strips apart (DMA, in internal RAM). */
CYDUI_STORAGE(storage, W, H, 1, 320, 1536, 48); /* the keyboard on the screen needs about 120 commands */
DMA_ATTR static cydui_color strips[2 * ROWS * W];
static uint64_t dirty[(H + ROWS - 1) / ROWS];
static cydui_esp32 dev;
static gallery g;
static cydui_ui ui;

static int64_t t_ui, t_end;
static uint32_t n_frames;

/* ---- events injected under the real touch, for the self-test of the keyboard ---- */

#define INJ_MAX 64
static cydui_backend real_be, inj_be;
static cydui_event inj_q[INJ_MAX];
static int inj_head, inj_len;

/* The clock of the UI: the real one, except while the hashes are taken at times of their own (the effects of a theme follow it). */
static bool virt_clock;
static uint32_t virt_now;

static uint32_t inj_now(void *user)
{
    (void)user;
    return virt_clock ? virt_now : real_be.now_ms(real_be.user);
}

static bool inj_poll(void *user, cydui_event *ev)
{
    (void)user;
    if (inj_len > 0) {
        *ev = inj_q[inj_head];
        inj_head = (inj_head + 1) % INJ_MAX;
        inj_len--;
        return true;
    }
    return real_be.poll_event ? real_be.poll_event(real_be.user, ev) : false;
}

#if CONFIG_GALLERY_TEXT_TEST && !CONFIG_GALLERY_ACCEPTANCE
static void inj_push(cydui_event_type type, int x, int y, uint32_t key)
{
    if (inj_len < INJ_MAX) {
        cydui_event *e = &inj_q[(inj_head + inj_len++) % INJ_MAX];
        memset(e, 0, sizeof(*e));
        e->type = type;
        e->x = (int16_t)x;
        e->y = (int16_t)y;
        e->key = key;
    }
}
#endif

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
    cydui_ui_config cfg = CYDUI_CONFIG(storage, &inj_be, &g.themes[0]);

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
/* The same states as `theme-gallery --headless --theme T --tab P [--at MS] --hash` on the PC: a new UI, six frames 16 ms
 * apart (the last one at `at_ms` when that is not negative), and the hash of what the sixth one sends (all the strips: it
 * is invalidated first). The clock is a made-up one, so that the effects that follow it are the PC's. */
static void hash_of(int t, int p, long at_ms)
{
    int f;

    virt_clock = true;
    virt_now = 0;
    if (!start_ui(t, p)) {
        virt_clock = false;
        return;
    }
    for (f = 0; f < 5; f++) {
        one_frame();
        virt_now += 16;
    }
    if (at_ms >= 0) {
        virt_now = (uint32_t)at_ms;
    }
    cydui_invalidate(&ui.ctx);
    cydui_esp32_hash_enable(&dev, true);
    one_frame();
    cydui_esp32_hash_enable(&dev, false);
    virt_clock = false;
    if (at_ms >= 0) {
        printf("GALLERY_HASH theme=%d tab=%d at=%ld %016" PRIx64 "\n", t, p, at_ms, cydui_esp32_hash(&dev));
    } else {
        printf("GALLERY_HASH theme=%d tab=%d %016" PRIx64 "\n", t, p, cydui_esp32_hash(&dev));
    }
}

static void print_hashes(void)
{
    int t, p;

    for (t = 0; t < GALLERY_THEMES; t++) {
        for (p = 0; p < GALLERY_PAGES; p++) {
            hash_of(t, p, -1);
        }
    }
    for (p = 0; p < GALLERY_PAGES; p++) { /* the theme with effects: when they are settled, and in the middle of a glitch */
        hash_of(GALLERY_THEMES - 1, p, 5000);
        hash_of(GALLERY_THEMES - 1, p, 4810);
    }
}
#endif

#if CONFIG_GALLERY_BENCH_SECONDS > 0 && !CONFIG_GALLERY_ACCEPTANCE
static unsigned states_used(void)
{
    unsigned n = 0;
    uint32_t i;

    for (i = 0; i < ui.state_cap; i++) {
        n += ui.state[i].used ? 1u : 0u;
    }
    return n;
}

/* CT-08: the animated theme through a long run of pages and a clock that moves by itself: the state table does not
 * grow, no tween is left moving, and nothing is raised. */
static void tween_growth_test(void)
{
    unsigned at_200 = 0, most_running = 0;
    uint32_t f;
    bool more = false;

    virt_clock = true;
    virt_now = 0;
    if (!start_ui(4, 0)) {
        virt_clock = false;
        return;
    }
    for (f = 0; f < 1500; f++) {
        g.tab = (int)((f / 50) % GALLERY_PAGES); /* another page every 50 frames: tabs, buttons and checkboxes come and go */
        more = one_frame();
        virt_now += 16;
        if (f == 200) at_200 = states_used();
        if (ui.tweens_now > most_running) most_running = ui.tweens_now;
    }
    for (f = 0; f < 60 && more; f++) { /* and then nothing is asked: it settles */
        more = one_frame();
        virt_now += 16;
    }
    virt_clock = false;
    printf("GALLERY_TWEENS states_used_at_200=%u states_used_at_1500=%u state_entries=%u most_running=%u settled=%d errors=0x%x\n",
           at_200, states_used(), (unsigned)ui.state_cap, most_running, !more, (unsigned)cydui_ui_errors(&ui));
}

/* Frames as fast as they go for a few seconds. `page` 0 is the page of controls, 4 the animation; with
 * `all_strips` every frame is invalidated first (every strip is drawn and sent, the worst case). */
static void bench(const char *name, int theme, int page, bool all_strips)
{
    int64_t start, now;
    uint32_t frames = 0, flushes0;
    int i;

    if (!start_ui(theme, page)) {
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
        if (ui.ctx.post.apply) { /* the post-effect of the theme alone, over the whole screen */
            t0 = esp_timer_get_time();
            for (k = 0; k < 20; k++) {
                for (y = 0; y < H; y += ROWS) {
                    ui.ctx.post.apply(ui.ctx.post.user, strips, W, (int16_t)y, ROWS);
                }
            }
            t1 = esp_timer_get_time();
            printf("GALLERY_BENCH %s effect_only_us_per_frame=%" PRId64 "\n", name, (t1 - t0) / 20);
        }
    }
}
#endif

#if CONFIG_GALLERY_PROFILE && !CONFIG_GALLERY_ACCEPTANCE
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

#if CONFIG_GALLERY_TEXT_TEST && !CONFIG_GALLERY_ACCEPTANCE
/* The centers of the keys of the keyboard of the screen (docs/10): the same arithmetic as src/keyboard.c. */
static int key_x(const int *weights, int n, int i)
{
    int total = 0, cum = 0, k, avail, x0, x1;

    for (k = 0; k < n; k++) total += weights[k];
    avail = W - 6 - (n - 1) * 3;
    for (k = 0; k < i; k++) cum += weights[k];
    x0 = 3 + cum * avail / total + i * 3;
    cum += weights[i];
    x1 = 3 + cum * avail / total + i * 3;
    return (x0 + x1) / 2;
}

static void tap_at(int x, int y, int64_t *worst_us, uint32_t *frames, int64_t *sum_us)
{
    static const cydui_event_type seq[2] = {CYDUI_EVENT_DOWN, CYDUI_EVENT_UP};
    int i, f;

    for (i = 0; i < 2; i++) {
        inj_push(seq[i], x, y, 0);
        for (f = 0; f < (i == 0 ? 1 : 4); f++) { /* the press, and the release with the frames that carry the key to the field */
            int64_t a = esp_timer_get_time(), b;

            one_frame();
            b = esp_timer_get_time() - a;
            *worst_us = b > *worst_us ? b : *worst_us;
            *sum_us += b;
            (*frames)++;
        }
    }
}

static void text_selftest(void)
{
    static const int w10[10] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, w9[9] = {2, 2, 2, 2, 2, 2, 2, 2, 2};
    static const int w_row2[9] = {3, 2, 2, 2, 2, 2, 2, 2, 3}, w_bottom[5] = {3, 3, 10, 4, 3};
    const int row_y[4] = {97 + 3 + 16, 97 + 3 + 35 + 16, 97 + 3 + 70 + 16, 97 + 3 + 105 + 16};
    int64_t worst = 0, sum = 0;
    uint32_t frames = 0, strips0;
    bool ok;

    if (!start_ui(1, 5)) {
        return;
    }
    memset(&inj_q, 0, sizeof(inj_q));
    inj_head = inj_len = 0;
    for (int i = 0; i < 6; i++) one_frame();
    strips0 = cydui_esp32_flushes(&dev);
    tap_at(30, 64, &worst, &frames, &sum);                              /* the name field: the focus, the keyboard */
    tap_at(key_x(w_row2, 9, 0), row_y[2], &worst, &frames, &sum);       /* Shift */
    tap_at(key_x(w9, 9, 5), row_y[1], &worst, &frames, &sum);          /* H */
    tap_at(key_x(w10, 10, 7), row_y[0], &worst, &frames, &sum);         /* i */
    tap_at(key_x(w_bottom, 5, 2), row_y[3], &worst, &frames, &sum);     /* space */
    tap_at(key_x(w_bottom, 5, 0), row_y[3], &worst, &frames, &sum);     /* the page of numbers */
    tap_at(key_x(w10, 10, 0), row_y[0], &worst, &frames, &sum);         /* 1 */
    tap_at(key_x(w_bottom, 5, 3), row_y[3], &worst, &frames, &sum);     /* Ok: confirms and closes */
    ok = strcmp(g.name, "Hi 1") == 0 && !cydui_has_focus(&ui);
    printf("GALLERY_TEXT_TEST pass=%d name=\"%s\" focus_after=%d frames=%" PRIu32 " avg_frame_us=%" PRId64 " worst_frame_us=%" PRId64
           " strips_per_frame=%.2f errors=0x%x\n",
           ok, g.name, cydui_has_focus(&ui), frames, frames ? sum / frames : 0, worst,
           frames ? (double)(cydui_esp32_flushes(&dev) - strips0) / frames : 0.0, (unsigned)cydui_ui_errors(&ui));
}
#endif

#if CONFIG_GALLERY_MEMORY_REPORT && !CONFIG_GALLERY_ACCEPTANCE
static void memory_report(void)
{
    printf("GALLERY_MEMORY bytes: ui_storage=%u strips=%u dirty=%u gallery(%d themes)=%u cydui_ui=%u cydui_esp32=%u cydui_theme=%u cydui_state=%u\n",
           (unsigned)sizeof(storage), (unsigned)sizeof(strips), (unsigned)sizeof(dirty), GALLERY_THEMES, (unsigned)sizeof(g), (unsigned)sizeof(ui),
           (unsigned)sizeof(dev), (unsigned)sizeof(cydui_theme), (unsigned)sizeof(cydui_state));
    printf("GALLERY_MEMORY heap: internal_free=%u largest_block=%u minimum_ever=%u\n",
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
}
#endif

#if CONFIG_GALLERY_BUDGET_TEST && !CONFIG_GALLERY_ACCEPTANCE
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

#if CONFIG_GALLERY_IDLE_SECONDS > 0 && !CONFIG_GALLERY_ACCEPTANCE
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
    real_be = *cydui_esp32_backend(&dev); /* the UI reads the events through a layer that can inject some (the self-test) */
    inj_be = real_be;
    inj_be.poll_event = inj_poll;
    inj_be.now_ms = inj_now;
    if (!start_ui(1, 0)) {
        return;
    }
    ESP_LOGI(TAG, "free heap after: %u bytes", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    one_frame();
    cydui_esp32_set_backlight(&dev, 100); /* only now, so that no garbage is ever seen */

#if CONFIG_GALLERY_PRINT_HASH
    print_hashes();
#endif
#if CONFIG_GALLERY_BENCH_SECONDS > 0 && !CONFIG_GALLERY_ACCEPTANCE
    bench("full_redraw", 1, 0, true);
    bench("animation_dirty_regions", 1, 4, false);
    bench("animation_all_strips", 1, 4, true);
    bench("retro_full_redraw", 3, 0, true);
    bench("cyberpunk_full_redraw", 4, 0, true);
    bench("cyberpunk_animation_dirty_regions", 4, 4, false);
    bench("cyberpunk_animation_all_strips", 4, 4, true);
    tween_growth_test();
#endif
#if CONFIG_GALLERY_PROFILE && !CONFIG_GALLERY_ACCEPTANCE
    profile_commands();
#endif
#if CONFIG_GALLERY_TEXT_TEST && !CONFIG_GALLERY_ACCEPTANCE
    text_selftest();
#endif
#if CONFIG_GALLERY_MEMORY_REPORT && !CONFIG_GALLERY_ACCEPTANCE
    memory_report();
#endif
#if CONFIG_GALLERY_BUDGET_TEST && !CONFIG_GALLERY_ACCEPTANCE
    budget_test();
#endif
#if CONFIG_GALLERY_IDLE_SECONDS > 0 && !CONFIG_GALLERY_ACCEPTANCE
    idle_report();
#endif
    if (!start_ui(1, CONFIG_GALLERY_ACCEPTANCE ? 3 : 0)) {
        return;
    }
#if CONFIG_GALLERY_ACCEPTANCE
    g.acceptance = true; /* only here: the hashes above are of the pages as the PC draws them */
#endif
    if (cydui_esp32_touch_status(&dev) == ESP_OK) {
#if CONFIG_GALLERY_FORCE_CALIBRATION || CONFIG_GALLERY_ACCEPTANCE
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
#if CONFIG_GALLERY_ACCEPTANCE
        {
            static bool reported;
            int max_px;
            gallery_accept_state as = gallery_acceptance(&g, &max_px);

            if (as >= GALLERY_ACCEPT_PASS && !reported) {
                printf("GALLERY_ACCEPT cal=%d targets=%d max_px=%d p95_ms=%.1f p50_ms=%.1f n=%d pass=%d spi=%dMHz rot=%d\n",
                       cydui_esp32_calibrated(&dev), g.check_n, max_px, g.touch.lat_p95_us / 1000.0, g.touch.lat_p50_us / 1000.0,
                       g.touch.lat_n, as == GALLERY_ACCEPT_PASS, CONFIG_CYDUI_ESP32_LCD_SPI_MHZ,
#ifdef CONFIG_CYDUI_ESP32_LCD_ROTATION_180
                       180
#else
                       0
#endif
                );
                reported = true;
            } else if (as < GALLERY_ACCEPT_PASS) {
                reported = false;
            }
        }
#endif
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
