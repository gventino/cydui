/* Context behavior: init validation, command list limits (CT-01) and bitmap checks (CT-10). */

#include <string.h>

#include "cydui_mem.h"
#include "testutil.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static cydui_color fb[TU_W * TU_H];
static cydui_color strip[TU_W * 8];

typedef struct {
    cydui_mem mem;
    cydui_ctx ctx;
    cydui_cmd cmds[3];
} fixture;

static uint32_t init_fixture(fixture *f, size_t strip_bytes)
{
    cydui_config cfg;
    cydui_mem_init(&f->mem, fb, TU_W, TU_H);
    memset(&cfg, 0, sizeof(cfg));
    cfg.backend = &f->mem.backend;
    cfg.cmd_mem = f->cmds;
    cfg.cmd_mem_size = sizeof(f->cmds);
    cfg.strip_mem = strip;
    cfg.strip_mem_size = strip_bytes;
    return cydui_init(&f->ctx, &cfg);
}

static void test_init_rejects_bad_config(void)
{
    cydui_ctx ctx;
    cydui_config cfg;
    cydui_mem mem;
    cydui_cmd cmds[1];

    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_init(&ctx, NULL));

    cydui_mem_init(&mem, fb, TU_W, TU_H);
    memset(&cfg, 0, sizeof(cfg));
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_init(&ctx, &cfg)); /* no backend */

    cfg.backend = &mem.backend;
    cfg.cmd_mem = cmds;
    cfg.cmd_mem_size = sizeof(cmds);
    cfg.strip_mem = strip;
    cfg.strip_mem_size = TU_W * 2 - 1; /* less than one row */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_NO_MEMORY, cydui_init(&ctx, &cfg));

    cfg.strip_mem_size = sizeof(strip);
    cfg.cmd_mem_size = sizeof(cydui_cmd) - 1; /* less than one command */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_NO_MEMORY, cydui_init(&ctx, &cfg));

    cfg.cmd_mem = NULL;
    cfg.cmd_mem_size = 1000;
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_NO_MEMORY, cydui_init(&ctx, &cfg));
}

static void test_strip_height_is_derived_from_memory(void)
{
    fixture f;

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, TU_W * 2 * 5 + 17));
    TEST_ASSERT_EQUAL_INT(5, cydui_strip_height(&f.ctx));

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, sizeof(strip)));
    TEST_ASSERT_EQUAL_INT(8, cydui_strip_height(&f.ctx));
}

static void test_command_list_overflow_drops_and_flags(void)
{
    fixture f;
    int i;

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, sizeof(strip)));
    cydui_begin_frame(&f.ctx, 0);
    for (i = 0; i < 5; i++) {
        cydui_draw_rect(&f.ctx, tu_rect(i * 10, 0, 5, 5), 0xFFFF, 255);
    }
    TEST_ASSERT_EQUAL_UINT32(3, cydui_cmd_count(&f.ctx));
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_CMD_OVERFLOW, cydui_errors(&f.ctx));

    /* Rendering with a full list must still work and not touch memory outside it. */
    cydui_end_frame(&f.ctx);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, fb[0]);

    /* A new frame starts empty; errors stay until cleared. */
    cydui_begin_frame(&f.ctx, 0);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_cmd_count(&f.ctx));
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_CMD_OVERFLOW, cydui_errors(&f.ctx));
    cydui_clear_errors(&f.ctx);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_errors(&f.ctx));
}

static void test_invisible_commands_use_no_capacity(void)
{
    fixture f;

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, sizeof(strip)));
    cydui_begin_frame(&f.ctx, 0);
    cydui_draw_rect(&f.ctx, tu_rect(0, 0, 0, 10), 1, 255);        /* empty */
    cydui_draw_rect(&f.ctx, tu_rect(0, 0, 10, -3), 1, 255);       /* negative */
    cydui_draw_rect(&f.ctx, tu_rect(500, 500, 10, 10), 1, 255);   /* off screen */
    cydui_draw_rect(&f.ctx, tu_rect(-50, -50, 10, 10), 1, 255);   /* off screen */
    cydui_draw_rect(&f.ctx, tu_rect(0, 0, 10, 10), 1, 0);         /* fully transparent */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_cmd_count(&f.ctx));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_errors(&f.ctx));
}

static void test_clip_culls_and_resets(void)
{
    fixture f;

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, sizeof(strip)));
    cydui_begin_frame(&f.ctx, 0);
    cydui_set_clip(&f.ctx, tu_rect(0, 0, 10, 10));
    cydui_draw_rect(&f.ctx, tu_rect(50, 50, 5, 5), 1, 255);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_cmd_count(&f.ctx));
    cydui_reset_clip(&f.ctx);
    cydui_draw_rect(&f.ctx, tu_rect(50, 50, 5, 5), 1, 255);
    TEST_ASSERT_EQUAL_UINT32(1, cydui_cmd_count(&f.ctx));

    /* begin_frame resets the clip too */
    cydui_set_clip(&f.ctx, tu_rect(0, 0, 1, 1));
    cydui_begin_frame(&f.ctx, 0);
    cydui_draw_rect(&f.ctx, tu_rect(50, 50, 5, 5), 1, 255);
    TEST_ASSERT_EQUAL_UINT32(1, cydui_cmd_count(&f.ctx));
}

static void test_zero_thickness_line_is_an_error(void)
{
    fixture f;

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, sizeof(strip)));
    cydui_begin_frame(&f.ctx, 0);
    cydui_draw_line(&f.ctx, 0, 0, 10, 10, 0, 1, 255);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_cmd_count(&f.ctx));
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_errors(&f.ctx));
}

/* CT-10: bitmap data must cover w*h*bpp bytes. */
static void test_bitmap_length_is_checked(void)
{
    fixture f;
    static const uint16_t px[4] = {1, 2, 3, 4};
    static const uint8_t a4[2] = {0xF0, 0x0F};

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, sizeof(strip)));
    cydui_begin_frame(&f.ctx, 0);

    cydui_draw_bitmap(&f.ctx, tu_rect(0, 0, 2, 2), px, sizeof(px), CYDUI_BITMAP_RGB565, 0, 255);
    TEST_ASSERT_EQUAL_UINT32(1, cydui_cmd_count(&f.ctx));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_errors(&f.ctx));

    cydui_draw_bitmap(&f.ctx, tu_rect(0, 0, 2, 2), px, sizeof(px) - 1, CYDUI_BITMAP_RGB565, 0, 255);
    TEST_ASSERT_EQUAL_UINT32(1, cydui_cmd_count(&f.ctx));
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_BITMAP, cydui_errors(&f.ctx));
    cydui_clear_errors(&f.ctx);

    /* A4 rows are padded to whole bytes: 3 px wide -> 2 bytes per row. */
    cydui_draw_bitmap(&f.ctx, tu_rect(0, 0, 3, 1), a4, 1, CYDUI_BITMAP_A4, 1, 255);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_BITMAP, cydui_errors(&f.ctx));
    cydui_clear_errors(&f.ctx);
    cydui_draw_bitmap(&f.ctx, tu_rect(0, 0, 3, 1), a4, 2, CYDUI_BITMAP_A4, 1, 255);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_errors(&f.ctx));
    TEST_ASSERT_EQUAL_UINT32(2, cydui_cmd_count(&f.ctx));
}

static void test_bitmap_size_does_not_overflow(void)
{
    fixture f;
    static const uint16_t px[1] = {1};

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, sizeof(strip)));
    cydui_begin_frame(&f.ctx, 0);
    /* Largest possible dimensions with a 2-byte buffer: must be rejected, not wrap. */
    cydui_draw_bitmap(&f.ctx, tu_rect(0, 0, 32767, 32767), px, sizeof(px), CYDUI_BITMAP_RGB565, 0,
                      255);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_BITMAP, cydui_errors(&f.ctx));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_cmd_count(&f.ctx));
    cydui_clear_errors(&f.ctx);

    cydui_draw_bitmap(&f.ctx, tu_rect(0, 0, 2, 2), NULL, 100, CYDUI_BITMAP_RGB565, 0, 255);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_BITMAP, cydui_errors(&f.ctx));
    cydui_clear_errors(&f.ctx);

    cydui_draw_bitmap(&f.ctx, tu_rect(0, 0, 0, 2), px, sizeof(px), CYDUI_BITMAP_RGB565, 0, 255);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_errors(&f.ctx)); /* empty: nothing to do, not an error */
}

static void test_end_frame_flushes_every_row_once(void)
{
    fixture f;

    TEST_ASSERT_EQUAL_UINT32(0, init_fixture(&f, TU_W * 2 * 7)); /* 7-row strips */
    cydui_begin_frame(&f.ctx, 0x1234);
    cydui_end_frame(&f.ctx);
    TEST_ASSERT_EQUAL_UINT32((TU_H + 6) / 7, f.mem.flushes); /* 240 / 7 -> 35 strips */
    TEST_ASSERT_EQUAL_HEX16(0x1234, fb[0]);
    TEST_ASSERT_EQUAL_HEX16(0x1234, fb[TU_W * TU_H - 1]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_rejects_bad_config);
    RUN_TEST(test_strip_height_is_derived_from_memory);
    RUN_TEST(test_command_list_overflow_drops_and_flags);
    RUN_TEST(test_invisible_commands_use_no_capacity);
    RUN_TEST(test_clip_culls_and_resets);
    RUN_TEST(test_zero_thickness_line_is_an_error);
    RUN_TEST(test_bitmap_length_is_checked);
    RUN_TEST(test_bitmap_size_does_not_overflow);
    RUN_TEST(test_end_frame_flushes_every_row_once);
    return UNITY_END();
}
