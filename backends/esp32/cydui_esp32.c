/*
 * The ESP32 backend: the display (SPI + DMA, two strips in flight), the backlight and the clock.
 * See cydui_esp32.h. The touch controller is in esp32_touch.c.
 */

#include "cydui_esp32.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "pins.h"
#include "sdkconfig.h"

static const char *TAG = "cydui_esp32";

/* ---- Kconfig booleans are undefined when off: make them 0/1 ---- */

#ifdef CONFIG_CYDUI_ESP32_LCD_INVERT
#define LCD_INVERT 1
#else
#define LCD_INVERT 0
#endif
#ifdef CONFIG_CYDUI_ESP32_LCD_BGR
#define LCD_BGR 1
#else
#define LCD_BGR 0
#endif
#ifdef CONFIG_CYDUI_ESP32_LCD_BL_ACTIVE_HIGH
#define LCD_BL_ACTIVE_HIGH 1
#else
#define LCD_BL_ACTIVE_HIGH 0
#endif
#ifdef CONFIG_CYDUI_ESP32_LCD_ROTATION_180
#define LCD_ROTATED 1
#else
#define LCD_ROTATED 0
#endif

/* ---- pin checks (CT-14), at compile time ---- */

CYDUI_PIN_REQUIRE_OUTPUT(lcd_mosi, CONFIG_CYDUI_ESP32_LCD_MOSI);
CYDUI_PIN_REQUIRE_OUTPUT(lcd_sclk, CONFIG_CYDUI_ESP32_LCD_SCLK);
CYDUI_PIN_REQUIRE_OUTPUT(lcd_cs, CONFIG_CYDUI_ESP32_LCD_CS);
CYDUI_PIN_REQUIRE_OUTPUT(lcd_dc, CONFIG_CYDUI_ESP32_LCD_DC);
CYDUI_PIN_REQUIRE_OUTPUT(lcd_backlight, CONFIG_CYDUI_ESP32_LCD_BL);
CYDUI_PIN_OPTIONAL_OUTPUT(lcd_reset, CONFIG_CYDUI_ESP32_LCD_RST);
CYDUI_PIN_OPTIONAL_INPUT(lcd_miso, CONFIG_CYDUI_ESP32_LCD_MISO);

#ifndef CONFIG_CYDUI_ESP32_PROFILE_CYD
#if CYDUI_PIN_IS_STRAPPING(CONFIG_CYDUI_ESP32_LCD_MOSI) || CYDUI_PIN_IS_STRAPPING(CONFIG_CYDUI_ESP32_LCD_SCLK) || \
    CYDUI_PIN_IS_STRAPPING(CONFIG_CYDUI_ESP32_LCD_CS) || CYDUI_PIN_IS_STRAPPING(CONFIG_CYDUI_ESP32_LCD_DC) || \
    CYDUI_PIN_IS_STRAPPING(CONFIG_CYDUI_ESP32_LCD_BL)
#warning "a display pin is a strapping pin (0, 2, 5, 12, 15): fine after boot on the classic CYD, suspicious elsewhere"
#endif
#endif

#define LCD_HOST ((spi_host_device_t)CONFIG_CYDUI_ESP32_LCD_HOST)
#define LCD_WIDTH 320
#define LCD_HEIGHT 240

/* ---- the panel ---- */

static void lcd_pre_cb(spi_transaction_t *t)
{
    gpio_set_level(CONFIG_CYDUI_ESP32_LCD_DC, (int)(intptr_t)t->user);
}

static esp_err_t lcd_cmd(cydui_esp32 *d, uint8_t cmd)
{
    spi_transaction_t t = {.length = 8, .tx_buffer = &cmd, .user = (void *)0};
    return spi_device_polling_transmit(d->lcd, &t);
}

static esp_err_t lcd_data(cydui_esp32 *d, const void *data, size_t len)
{
    spi_transaction_t t = {.length = len * 8, .tx_buffer = data, .user = (void *)1};
    return len ? spi_device_polling_transmit(d->lcd, &t) : ESP_OK;
}

static esp_err_t lcd_write(cydui_esp32 *d, uint8_t cmd, const uint8_t *data, size_t len)
{
    esp_err_t e = lcd_cmd(d, cmd);
    return e == ESP_OK ? lcd_data(d, data, len) : e;
}

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t data[15];
} reg_t;

/* The common ILI9341 register set; it worked unchanged on the ILI9342C-like panel of the CYD. */
#if !defined(CONFIG_CYDUI_ESP32_LCD_ST7789)
static const reg_t ili_regs[] = {
    {0x3A, 1, {0x55}},
    {0xCF, 3, {0x00, 0x83, 0x30}},
    {0xED, 4, {0x64, 0x03, 0x12, 0x81}},
    {0xE8, 3, {0x85, 0x01, 0x79}},
    {0xCB, 5, {0x39, 0x2C, 0x00, 0x34, 0x02}},
    {0xF7, 1, {0x20}},
    {0xEA, 2, {0x00, 0x00}},
    {0xC0, 1, {0x26}},
    {0xC1, 1, {0x11}},
    {0xC5, 2, {0x35, 0x3E}},
    {0xC7, 1, {0xBE}},
    {0xB1, 2, {0x00, 0x1B}},
    {0xF2, 1, {0x08}},
    {0x26, 1, {0x01}},
    {0xE0, 15, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}},
    {0xE1, 15, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}},
    {0xB7, 1, {0x07}},
    {0xB6, 4, {0x0A, 0x82, 0x27, 0x00}},
};
#else
/* ST7789: the usual 240x320 module set-up (best effort, not verified on hardware). */
static const reg_t st7789_regs[] = {
    {0x3A, 1, {0x55}},
    {0xB2, 5, {0x0C, 0x0C, 0x00, 0x33, 0x33}},
    {0xB7, 1, {0x35}},
    {0xBB, 1, {0x19}},
    {0xC0, 1, {0x2C}},
    {0xC2, 1, {0x01}},
    {0xC3, 1, {0x12}},
    {0xC4, 1, {0x20}},
    {0xC6, 1, {0x0F}},
    {0xD0, 2, {0xA4, 0xA1}},
    {0xE0, 14, {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23}},
    {0xE1, 14, {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23}},
};
#endif

static uint8_t madctl_value(void)
{
    uint8_t m;
#if defined(CONFIG_CYDUI_ESP32_LCD_ILI9341)
    m = LCD_ROTATED ? 0xE0 : 0x20; /* MV (and MX, MY): landscape on a panel that is natively portrait */
#elif defined(CONFIG_CYDUI_ESP32_LCD_ST7789)
    m = LCD_ROTATED ? 0xA0 : 0x60;
#else
    m = LCD_ROTATED ? 0xC0 : 0x00; /* the CYD: natively landscape, MADCTL 0 as found in iteration 0 */
#endif
    return (uint8_t)(m | (LCD_BGR ? 0x08 : 0x00));
}

static esp_err_t lcd_init_panel(cydui_esp32 *d)
{
#if defined(CONFIG_CYDUI_ESP32_LCD_ST7789)
    const reg_t *regs = st7789_regs;
    size_t n = sizeof(st7789_regs) / sizeof(st7789_regs[0]), i;
#else
    const reg_t *regs = ili_regs;
    size_t n = sizeof(ili_regs) / sizeof(ili_regs[0]), i;
#endif
    uint8_t madctl = madctl_value();

    if (CONFIG_CYDUI_ESP32_LCD_RST >= 0) {
        gpio_set_direction(CONFIG_CYDUI_ESP32_LCD_RST, GPIO_MODE_OUTPUT);
        gpio_set_level(CONFIG_CYDUI_ESP32_LCD_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(CONFIG_CYDUI_ESP32_LCD_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(120));
    } else {
        ESP_RETURN_ON_ERROR(lcd_cmd(d, 0x01), TAG, "software reset"); /* RST is not wired on the CYD */
        vTaskDelay(pdMS_TO_TICKS(150));
    }
#if defined(CONFIG_CYDUI_ESP32_LCD_ST7789)
    ESP_RETURN_ON_ERROR(lcd_cmd(d, 0x11), TAG, "sleep out"); /* ST7789 wants sleep out first */
    vTaskDelay(pdMS_TO_TICKS(120));
#endif
    for (i = 0; i < n; i++) {
        ESP_RETURN_ON_ERROR(lcd_write(d, regs[i].cmd, regs[i].data, regs[i].len), TAG, "register 0x%02X", regs[i].cmd);
    }
    ESP_RETURN_ON_ERROR(lcd_write(d, 0x36, &madctl, 1), TAG, "MADCTL");
#if !defined(CONFIG_CYDUI_ESP32_LCD_ST7789)
    ESP_RETURN_ON_ERROR(lcd_cmd(d, 0x11), TAG, "sleep out");
    vTaskDelay(pdMS_TO_TICKS(120));
#endif
    ESP_RETURN_ON_ERROR(lcd_cmd(d, LCD_INVERT ? 0x21 : 0x20), TAG, "inversion");
    ESP_RETURN_ON_ERROR(lcd_cmd(d, 0x29), TAG, "display on");
    vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "panel started: MADCTL 0x%02X, invert %d, %d MHz", madctl, LCD_INVERT, CONFIG_CYDUI_ESP32_LCD_SPI_MHZ);
    return ESP_OK;
}

static esp_err_t lcd_start(cydui_esp32 *d)
{
    spi_bus_config_t bus = {
        .mosi_io_num = CONFIG_CYDUI_ESP32_LCD_MOSI,
        .miso_io_num = CONFIG_CYDUI_ESP32_LCD_MISO,
        .sclk_io_num = CONFIG_CYDUI_ESP32_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (int)d->max_strip_bytes,
    };
    spi_device_interface_config_t dev = {
        .clock_speed_hz = CONFIG_CYDUI_ESP32_LCD_SPI_MHZ * 1000 * 1000,
        .mode = 0,
        .spics_io_num = CONFIG_CYDUI_ESP32_LCD_CS,
        .queue_size = 2,
        .pre_cb = lcd_pre_cb,
    };
    gpio_config_t dc = {.pin_bit_mask = 1ull << CONFIG_CYDUI_ESP32_LCD_DC, .mode = GPIO_MODE_OUTPUT};

    ESP_RETURN_ON_ERROR(gpio_config(&dc), TAG, "DC pin");
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "SPI bus of the display");
    ESP_RETURN_ON_ERROR(spi_bus_add_device(LCD_HOST, &dev, &d->lcd), TAG, "SPI device of the display");
    return lcd_init_panel(d);
}

/* ---- backlight ---- */

static esp_err_t backlight_start(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t ch = {
        .gpio_num = CONFIG_CYDUI_ESP32_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = LCD_BL_ACTIVE_HIGH ? 0 : 255, /* off */
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "backlight timer");
    return ledc_channel_config(&ch);
}

void cydui_esp32_set_backlight(cydui_esp32 *d, int percent)
{
    int duty;

    (void)d;
    percent = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    duty = percent * 255 / 100;
    duty = LCD_BL_ACTIVE_HIGH ? duty : 255 - duty;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, (uint32_t)duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

/* ---- the backend ---- */

static void lcd_wait(cydui_esp32 *d)
{
    if (d->lcd_pending) {
        spi_transaction_t *done;
        spi_device_get_trans_result(d->lcd, &done, portMAX_DELAY);
        d->lcd_pending = false;
    }
}

static esp_err_t lcd_window(cydui_esp32 *d, const cydui_rect *a)
{
    uint8_t ca[4] = {(uint8_t)(a->x >> 8), (uint8_t)a->x, (uint8_t)((a->x + a->w - 1) >> 8), (uint8_t)(a->x + a->w - 1)};
    uint8_t pa[4] = {(uint8_t)(a->y >> 8), (uint8_t)a->y, (uint8_t)((a->y + a->h - 1) >> 8), (uint8_t)(a->y + a->h - 1)};
    esp_err_t e = lcd_write(d, 0x2A, ca, 4);

    if (e == ESP_OK) e = lcd_write(d, 0x2B, pa, 4);
    if (e == ESP_OK) e = lcd_cmd(d, 0x2C);
    return e;
}

static void esp32_flush(void *user, const cydui_rect *area, const cydui_color *pixels)
{
    cydui_esp32 *d = (cydui_esp32 *)user;
    size_t n = (size_t)area->w * (size_t)area->h, i;
    cydui_color *px = (cydui_color *)pixels; /* the core draws it again before it is used again */

    /* The contract of flush_strip: the transfer of the previous call is complete before this one returns. */
    lcd_wait(d);
    if (n * sizeof(cydui_color) > d->max_strip_bytes || !esp_ptr_dma_capable(px) || lcd_window(d, area) != ESP_OK) {
        if (d->rejected++ == 0) {
            ESP_LOGE(TAG, "a strip was dropped: too big, not in DMA-capable RAM, or the panel did not answer");
        }
        return;
    }
#ifdef CONFIG_CYDUI_ESP32_PIXEL_HASH
    if (d->hash_on) { /* the pixels as they are in memory, byte by byte: the order of cydui_mem_hash */
        const uint8_t *b = (const uint8_t *)px;
        uint64_t h = d->hash;
        for (i = 0; i < n * 2; i++) {
            h = (h ^ b[i]) * 1099511628211ull;
        }
        d->hash = h;
    }
#endif
    for (i = 0; i < n; i++) { /* the panel wants the high byte first */
        px[i] = (cydui_color)__builtin_bswap16(px[i]);
    }
    d->lcd_trans = (spi_transaction_t){.length = n * 16, .tx_buffer = px, .user = (void *)1};
    if (spi_device_queue_trans(d->lcd, &d->lcd_trans, portMAX_DELAY) == ESP_OK) {
        d->lcd_pending = true;
        d->flushes++;
    } else {
        d->rejected++;
    }
}

static void esp32_frame_end(void *user)
{
    lcd_wait((cydui_esp32 *)user); /* every transfer is complete when the frame is over */
}

static uint32_t esp32_now_ms(void *user)
{
    (void)user;
    return (uint32_t)(esp_timer_get_time() / 1000);
}

esp_err_t cydui_esp32_init(cydui_esp32 *d, const cydui_esp32_config *cfg)
{
    esp_err_t e;

    if (!d || !cfg || cfg->max_strip_bytes == 0 || cfg->max_strip_bytes > 65536u) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(d, 0, sizeof(*d));
    d->max_strip_bytes = cfg->max_strip_bytes;
    d->be.user = d;
    d->be.width = LCD_WIDTH;
    d->be.height = LCD_HEIGHT;
    d->be.flush_strip = esp32_flush;
    d->be.frame_end = esp32_frame_end;
    d->be.now_ms = esp32_now_ms;
    d->be.poll_event = NULL; /* the touch screen is added by esp32_touch.c */

    e = backlight_start();
    if (e == ESP_OK) {
        e = lcd_start(d);
    }
    return e;
}

const cydui_backend *cydui_esp32_backend(cydui_esp32 *d)
{
    return &d->be;
}

uint32_t cydui_esp32_flushes(const cydui_esp32 *d)
{
    return d->flushes;
}

void cydui_esp32_hash_enable(cydui_esp32 *d, bool on)
{
    d->hash_on = on;
    if (on) {
        d->hash = 1469598103934665603ull; /* the basis of cydui_mem_hash: a digit short of the standard FNV one, kept so that the golden hashes stay valid */
    }
}

uint64_t cydui_esp32_hash(const cydui_esp32 *d)
{
    return d->hash;
}
