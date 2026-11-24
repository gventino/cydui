/*
 * hw-probe: diagnostic firmware for the CYD (ESP32-2432S028R).
 *
 * Stages (all results go to the serial log, prefix "PROBE"):
 *   1. backlight + ILI9341 init + ID read
 *   2. color fills and a test pattern (color order, inversion, rotation)
 *   3. SPI clock sweep with full-screen fill timing
 *   4. touch loop: raw XPT2046 x/y/z, plus a dot drawn at a naive position
 *
 * This is throwaway diagnostic code, not part of the library.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "PROBE";

/* Kconfig bools are undefined when disabled; normalize to 0/1 for use in expressions. */
#ifdef CONFIG_PROBE_LCD_INVERT
#define LCD_INVERT 1
#else
#define LCD_INVERT 0
#endif
#ifdef CONFIG_PROBE_LCD_BGR
#define LCD_BGR 1
#else
#define LCD_BGR 0
#endif
#ifdef CONFIG_PROBE_LCD_BL_ACTIVE_HIGH
#define LCD_BL_ACTIVE_HIGH 1
#else
#define LCD_BL_ACTIVE_HIGH 0
#endif
#ifdef CONFIG_PROBE_LCD_NATIVE_LANDSCAPE
#define LCD_NATIVE_LANDSCAPE 1
#else
#define LCD_NATIVE_LANDSCAPE 0
#endif
#ifdef CONFIG_PROBE_BOARD_PROFILE_CYD
#define BOARD_PROFILE_CYD 1
#else
#define BOARD_PROFILE_CYD 0
#endif

/* ---- pin validation (CT-14): output pins must not be flash or input-only ---- */
#define PIN_OK_AS_OUTPUT(p) (!((p) >= 6 && (p) <= 11) && !((p) >= 34 && (p) <= 39))
_Static_assert(PIN_OK_AS_OUTPUT(CONFIG_PROBE_LCD_MOSI), "LCD MOSI pin cannot be an output");
_Static_assert(PIN_OK_AS_OUTPUT(CONFIG_PROBE_LCD_SCLK), "LCD SCLK pin cannot be an output");
_Static_assert(PIN_OK_AS_OUTPUT(CONFIG_PROBE_LCD_CS), "LCD CS pin cannot be an output");
_Static_assert(PIN_OK_AS_OUTPUT(CONFIG_PROBE_LCD_DC), "LCD DC pin cannot be an output");
_Static_assert(PIN_OK_AS_OUTPUT(CONFIG_PROBE_LCD_BL), "Backlight pin cannot be an output");
_Static_assert(PIN_OK_AS_OUTPUT(CONFIG_PROBE_TOUCH_CLK), "Touch CLK pin cannot be an output");
_Static_assert(PIN_OK_AS_OUTPUT(CONFIG_PROBE_TOUCH_MOSI), "Touch MOSI pin cannot be an output");
_Static_assert(PIN_OK_AS_OUTPUT(CONFIG_PROBE_TOUCH_CS), "Touch CS pin cannot be an output");

static void warn_strapping(int pin, const char *name)
{
#if !CONFIG_PROBE_BOARD_PROFILE_CYD
    if (pin == 0 || pin == 2 || pin == 5 || pin == 12 || pin == 15) {
        ESP_LOGW(TAG, "%s uses strapping pin GPIO%d (not covered by the CYD profile)", name, pin);
    }
#else
    (void)pin;
    (void)name;
#endif
}

/* ---- display ---- */
#define LCD_HOST SPI2_HOST
#define TOUCH_HOST SPI3_HOST
#define LCD_LONG 320
#define LCD_SHORT 240
#define CHUNK_PIXELS 3200

static spi_device_handle_t s_lcd;
static spi_device_handle_t s_touch;
static uint16_t *s_dma_buf; /* CHUNK_PIXELS big-endian RGB565 */
static int s_width, s_height;

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define RED RGB565(255, 0, 0)
#define GREEN RGB565(0, 255, 0)
#define BLUE RGB565(0, 0, 255)
#define WHITE 0xFFFF
#define BLACK 0x0000
#define YELLOW RGB565(255, 255, 0)
#define MAGENTA RGB565(255, 0, 255)
#define CYAN RGB565(0, 255, 255)

static void lcd_pre_cb(spi_transaction_t *t)
{
    gpio_set_level(CONFIG_PROBE_LCD_DC, (int)(intptr_t)t->user);
}

static esp_err_t lcd_add_device(int mhz)
{
    spi_device_interface_config_t dev = {
        .clock_speed_hz = mhz * 1000 * 1000,
        .mode = 0,
        .spics_io_num = CONFIG_PROBE_LCD_CS,
        .queue_size = 4,
        .pre_cb = lcd_pre_cb,
    };
    return spi_bus_add_device(LCD_HOST, &dev, &s_lcd);
}

static void lcd_cmd(uint8_t cmd)
{
    spi_transaction_t t = {.length = 8, .tx_buffer = &cmd, .user = (void *)0};
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_lcd, &t));
}

static void lcd_data(const void *data, size_t len)
{
    if (len == 0) {
        return;
    }
    spi_transaction_t t = {.length = len * 8, .tx_buffer = data, .user = (void *)1};
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_lcd, &t));
}

static void lcd_write(uint8_t cmd, const uint8_t *data, size_t len)
{
    lcd_cmd(cmd);
    lcd_data(data, len);
}

/* Reads `len` bytes after sending `cmd`. The first byte is a dummy. */
static void lcd_read(uint8_t cmd, uint8_t *out, size_t len)
{
    /* CS_KEEP_ACTIVE requires owning the bus across the two transactions. */
    ESP_ERROR_CHECK(spi_device_acquire_bus(s_lcd, portMAX_DELAY));
    spi_transaction_t c = {.length = 8, .tx_buffer = &cmd, .user = (void *)0, .flags = SPI_TRANS_CS_KEEP_ACTIVE};
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_lcd, &c));
    spi_transaction_t r = {.length = len * 8, .rxlength = len * 8, .rx_buffer = out, .user = (void *)1};
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_lcd, &r));
    spi_device_release_bus(s_lcd);
}

static uint8_t madctl_value(void)
{
    if (CONFIG_PROBE_LCD_MADCTL_RAW >= 0) {
        return (uint8_t)CONFIG_PROBE_LCD_MADCTL_RAW;
    }
    static const uint8_t base[4] = {0x40, 0x20, 0x80, 0xE0}; /* MX, MV, MY, MX|MY|MV */
    return base[CONFIG_PROBE_LCD_ROTATION] | (LCD_BGR ? 0x08 : 0x00);
}

static void lcd_set_window(int x0, int y0, int x1, int y1)
{
    uint8_t ca[4] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    uint8_t pa[4] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
    lcd_write(0x2A, ca, 4);
    lcd_write(0x2B, pa, 4);
    lcd_cmd(0x2C);
}

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (x < 0 || y < 0 || w <= 0 || h <= 0 || x + w > s_width || y + h > s_height) {
        return;
    }
    uint16_t be = (uint16_t)((color >> 8) | (color << 8));
    size_t total = (size_t)w * h;
    size_t n = total < CHUNK_PIXELS ? total : CHUNK_PIXELS;
    for (size_t i = 0; i < n; i++) {
        s_dma_buf[i] = be;
    }
    lcd_set_window(x, y, x + w - 1, y + h - 1);
    while (total > 0) {
        size_t c = total < CHUNK_PIXELS ? total : CHUNK_PIXELS;
        lcd_data(s_dma_buf, c * 2);
        total -= c;
    }
}

static void lcd_init(void)
{
    static const struct {
        uint8_t cmd;
        uint8_t data[15];
        uint8_t len;
    } seq[] = {
        {0x3A, {0x55}, 1},
        {0xCF, {0x00, 0x83, 0x30}, 3},
        {0xED, {0x64, 0x03, 0x12, 0x81}, 4},
        {0xE8, {0x85, 0x01, 0x79}, 3},
        {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
        {0xF7, {0x20}, 1},
        {0xEA, {0x00, 0x00}, 2},
        {0xC0, {0x26}, 1},
        {0xC1, {0x11}, 1},
        {0xC5, {0x35, 0x3E}, 2},
        {0xC7, {0xBE}, 1},
        {0xB1, {0x00, 0x1B}, 2},
        {0xF2, {0x08}, 1},
        {0x26, {0x01}, 1},
        {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0x87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
        {0xE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
        {0xB7, {0x07}, 1},
        {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    };

    lcd_cmd(0x01); /* software reset (RST is not wired on the CYD) */
    vTaskDelay(pdMS_TO_TICKS(150));

    uint8_t id4[5] = {0}, d3[5] = {0};
    lcd_read(0x04, id4, 4);
    lcd_read(0xD3, d3, 4);
    ESP_LOGI(TAG, "RDDID  (0x04): dummy=%02X id=%02X %02X %02X", id4[0], id4[1], id4[2], id4[3]);
    ESP_LOGI(TAG, "ID4    (0xD3): dummy=%02X id=%02X %02X %02X  (ILI9341 expects 00 93 41)", d3[0], d3[1], d3[2], d3[3]);

    for (size_t i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
        lcd_write(seq[i].cmd, seq[i].data, seq[i].len);
    }
    uint8_t madctl = madctl_value();
    lcd_write(0x36, &madctl, 1);
    ESP_LOGI(TAG, "MADCTL=0x%02X invert=%d native_landscape=%d", madctl, LCD_INVERT, LCD_NATIVE_LANDSCAPE);

    lcd_cmd(0x11); /* sleep out */
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_cmd(LCD_INVERT ? 0x21 : 0x20);
    lcd_cmd(0x29); /* display on */
    vTaskDelay(pdMS_TO_TICKS(20));
}

static void backlight_on(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ledc_channel_config_t ch = {
        .gpio_num = CONFIG_PROBE_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = LCD_BL_ACTIVE_HIGH ? 255 : 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

/* ---- stages ---- */
static void test_pattern(void)
{
    /* Black background first (clears leftovers), then a 3 px white border at the
     * very edge of the addressed area: shows exactly which part of the panel we
     * are writing to. */
    fill_rect(0, 0, s_width, s_height, BLACK);
    fill_rect(0, 0, s_width, 3, WHITE);
    fill_rect(0, s_height - 3, s_width, 3, WHITE);
    fill_rect(0, 0, 3, s_height, WHITE);
    fill_rect(s_width - 3, 0, 3, s_height, WHITE);

    int third = (s_width - 6) / 3;
    fill_rect(3, 60, third, s_height - 120, RED);
    fill_rect(3 + third, 60, third, s_height - 120, GREEN);
    fill_rect(3 + 2 * third, 60, third, s_height - 120, BLUE);

    /* center cross */
    fill_rect(s_width / 2 - 1, 3, 2, s_height - 6, YELLOW);
    fill_rect(3, s_height / 2 - 1, s_width - 6, 2, YELLOW);

    /* corner markers: tells rotation / mirroring apart */
    fill_rect(3, 3, 40, 40, WHITE);                               /* top-left: white    */
    fill_rect(s_width - 43, 3, 40, 40, MAGENTA);                  /* top-right: magenta */
    fill_rect(3, s_height - 43, 40, 40, CYAN);                    /* bottom-left: cyan  */
    fill_rect(s_width - 43, s_height - 43, 40, 40, YELLOW);       /* bottom-right: yellow */
}

static void stage_colors(void)
{
    static const struct {
        const char *name;
        uint16_t c;
    } fills[] = {{"RED", RED}, {"GREEN", GREEN}, {"BLUE", BLUE}, {"WHITE", WHITE}, {"BLACK", BLACK}};
    for (size_t i = 0; i < sizeof(fills) / sizeof(fills[0]); i++) {
        ESP_LOGI(TAG, "stage 2: full-screen fill %s", fills[i].name);
        fill_rect(0, 0, s_width, s_height, fills[i].c);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
    ESP_LOGI(TAG, "stage 2: test pattern: bands R|G|B; corners TL=white TR=magenta BL=cyan BR=yellow");
    test_pattern();
    vTaskDelay(pdMS_TO_TICKS(6000));
}

static void stage_sweep(void)
{
    static const int mhz[] = {20, 40, 60, 80};
    for (size_t i = 0; i < sizeof(mhz) / sizeof(mhz[0]); i++) {
        ESP_ERROR_CHECK(spi_bus_remove_device(s_lcd));
        ESP_ERROR_CHECK(lcd_add_device(mhz[i]));
        int64_t t0 = esp_timer_get_time();
        const int frames = 10;
        for (int f = 0; f < frames; f++) {
            fill_rect(0, 0, s_width, s_height, (f & 1) ? BLUE : RED);
        }
        int64_t us = esp_timer_get_time() - t0;
        test_pattern();
        ESP_LOGI(TAG, "stage 3: SPI %2d MHz: %d full-screen fills in %lld ms -> %.1f fps; pattern on screen for 4 s",
                 mhz[i], frames, (long long)(us / 1000), frames * 1e6 / (double)us);
        vTaskDelay(pdMS_TO_TICKS(4000));
    }
    ESP_ERROR_CHECK(spi_bus_remove_device(s_lcd));
    ESP_ERROR_CHECK(lcd_add_device(CONFIG_PROBE_LCD_SPI_MHZ));
    ESP_LOGI(TAG, "stage 3: back to configured %d MHz", CONFIG_PROBE_LCD_SPI_MHZ);
}

static uint16_t xpt_read(uint8_t cmd)
{
    uint8_t tx[3] = {cmd, 0, 0}, rx[3] = {0};
    spi_transaction_t t = {.length = 24, .tx_buffer = tx, .rx_buffer = rx};
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_touch, &t));
    return (uint16_t)(((rx[1] << 8) | rx[2]) >> 3) & 0x0FFF;
}

static uint16_t xpt_avg(uint8_t cmd)
{
    /* The first conversion after switching channels is polluted by the previous
     * channel's plate voltage: discard it, then average. */
    (void)xpt_read(cmd);
    uint32_t sum = 0;
    for (int i = 0; i < 4; i++) {
        sum += xpt_read(cmd);
    }
    return (uint16_t)(sum / 4);
}

static void stage_touch(void)
{
    ESP_LOGI(TAG, "stage 4: touch loop. Touch the 4 corners (white TL, magenta TR, cyan BL, yellow BR) and the center.");
    test_pattern();

    uint16_t xmin = 4095, xmax = 0, ymin = 4095, ymax = 0, zmax = 0;
    int prev_x = -1, prev_y = -1, n = 0;
    int64_t last_summary = esp_timer_get_time();

    for (;;) {
        if (gpio_get_level(CONFIG_PROBE_TOUCH_IRQ) == 0) {
            uint16_t x = xpt_avg(0xD0), y = xpt_avg(0x90);
            uint16_t z1 = xpt_read(0xB0), z2 = xpt_read(0xC0);
            int z = z1 + 4095 - z2;
            if (z > 0 && z < 4095 * 2 && x > 0 && x < 4095 && y > 0 && y < 4095) {
                if (x < xmin) xmin = x;
                if (x > xmax) xmax = x;
                if (y < ymin) ymin = y;
                if (y > ymax) ymax = y;
                if ((uint16_t)z > zmax) zmax = (uint16_t)z;
                if (n++ % 5 == 0) {
                    ESP_LOGI(TAG, "touch raw x=%4u y=%4u z1=%4u z2=%4u z=%4d", x, y, z1, z2, z);
                }
                /* naive mapping, just to see where it lands */
                /* Measured on this board: raw Y is the horizontal axis, raw X the vertical one,
                 * both increasing right/down, usable range about 400..3600. */
                int px = ((int)y - 400) * s_width / 3200, py = ((int)x - 400) * s_height / 3200;
                if (px < 0) px = 0;
                if (py < 0) py = 0;
                if (prev_x >= 0) {
                    fill_rect(prev_x, prev_y, 6, 6, BLACK);
                }
                if (px > s_width - 6) px = s_width - 6;
                if (py > s_height - 6) py = s_height - 6;
                fill_rect(px, py, 6, 6, WHITE);
                prev_x = px;
                prev_y = py;
            }
        }
        if (esp_timer_get_time() - last_summary > 10 * 1000 * 1000) {
            last_summary = esp_timer_get_time();
            ESP_LOGI(TAG, "touch summary: x[%u..%u] y[%u..%u] zmax=%u", xmin, xmax, ymin, ymax, zmax);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "hw-probe starting (profile CYD=%d)", BOARD_PROFILE_CYD);
    warn_strapping(CONFIG_PROBE_LCD_MISO, "LCD MISO");
    warn_strapping(CONFIG_PROBE_LCD_CS, "LCD CS");
    warn_strapping(CONFIG_PROBE_LCD_DC, "LCD DC");

    /* MV (0x20) swaps rows/columns. ILI9341 is natively 240x320, ILI9342C natively 320x240. */
    bool mv = (madctl_value() & 0x20) != 0;
    bool landscape = LCD_NATIVE_LANDSCAPE ? !mv : mv;
    s_width = landscape ? LCD_LONG : LCD_SHORT;
    s_height = landscape ? LCD_SHORT : LCD_LONG;
    s_dma_buf = heap_caps_malloc(CHUNK_PIXELS * sizeof(uint16_t), MALLOC_CAP_DMA);
    ESP_ERROR_CHECK(s_dma_buf ? ESP_OK : ESP_ERR_NO_MEM);

    gpio_config_t dc = {.pin_bit_mask = 1ULL << CONFIG_PROBE_LCD_DC, .mode = GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&dc));

    backlight_on();

    spi_bus_config_t lcd_bus = {
        .mosi_io_num = CONFIG_PROBE_LCD_MOSI,
        .miso_io_num = CONFIG_PROBE_LCD_MISO,
        .sclk_io_num = CONFIG_PROBE_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CHUNK_PIXELS * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &lcd_bus, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(lcd_add_device(CONFIG_PROBE_LCD_SPI_MHZ));

    ESP_LOGI(TAG, "stage 1: panel init");
    lcd_init();

    spi_bus_config_t touch_bus = {
        .mosi_io_num = CONFIG_PROBE_TOUCH_MOSI,
        .miso_io_num = CONFIG_PROBE_TOUCH_MISO,
        .sclk_io_num = CONFIG_PROBE_TOUCH_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TOUCH_HOST, &touch_bus, SPI_DMA_DISABLED));
    spi_device_interface_config_t tdev = {
        .clock_speed_hz = 2 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = CONFIG_PROBE_TOUCH_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(TOUCH_HOST, &tdev, &s_touch));
    gpio_config_t irq = {.pin_bit_mask = 1ULL << CONFIG_PROBE_TOUCH_IRQ, .mode = GPIO_MODE_INPUT};
    ESP_ERROR_CHECK(gpio_config(&irq));

#ifdef CONFIG_PROBE_HOLD_PATTERN
    ESP_LOGI(TAG, "hold mode: test pattern only (%dx%d)", s_width, s_height);
    test_pattern();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif
    stage_colors();
    stage_sweep();
    ESP_LOGI(TAG, "free heap: %" PRIu32 " bytes", esp_get_free_heap_size());
    stage_touch();
}
