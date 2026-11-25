/*
 * What version 0.5.0 added to the core (FR-05, CT-15): two strip buffers for a backend whose transfer
 * is still running when flush_strip returns, dirty regions that skip the strips that did not change,
 * and the time budget of a frame. The central test is a property: with dirty regions and two buffers
 * the screen is, frame after frame, the one that plain rendering gives.
 */

#include <stdio.h>
#include <string.h>

#include "cydui_sans_12.h"
#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define W 64
#define H 48
#define ROWS 6 /* eight strips */
#define STRIPS (H / ROWS)

/* ---- a backend that can be slow, like a DMA transfer, and counts what it is sent ---- */

typedef struct {
    cydui_backend be;
    cydui_color fb[W * H];
    const cydui_color *pending; /* a transfer that has not finished */
    cydui_rect pending_area;
    bool late;
    int flushes, ys[64], waits_with_pending;
} slow_t;

static void copy_out(slow_t *b, const cydui_color *px, const cydui_rect *a)
{
    int y;
    for (y = 0; y < a->h; y++) {
        memcpy(&b->fb[(a->y + y) * W], &px[y * a->w], (size_t)a->w * sizeof(cydui_color));
    }
}

static void finish(slow_t *b)
{
    if (b->pending) {
        copy_out(b, b->pending, &b->pending_area);
        b->pending = NULL;
    }
}

static void slow_flush(void *u, const cydui_rect *a, const cydui_color *px)
{
    slow_t *b = (slow_t *)u;

    finish(b); /* the contract: the previous transfer is complete before this call returns */
    if (b->flushes < 64) {
        b->ys[b->flushes] = a->y;
    }
    b->flushes++;
    if (b->late) {
        b->pending = px;
        b->pending_area = *a;
    } else {
        copy_out(b, px, a);
    }
}

static void slow_frame_end(void *u)
{
    finish((slow_t *)u);
}

static void slow_init(slow_t *b, bool late)
{
    memset(b, 0, sizeof(*b));
    b->be.user = b;
    b->be.width = W;
    b->be.height = H;
    b->be.flush_strip = slow_flush;
    b->be.frame_end = slow_frame_end;
    b->late = late;
}

typedef struct {
    slow_t dev;
    cydui_ctx ctx;
    cydui_cmd cmds[160];
    cydui_color strip[2 * ROWS * W];
    char text[512];
    uint64_t dirty[16];
} sys_t;

static uint32_t sys_init(sys_t *s, bool late, int buffers, bool dirty)
{
    cydui_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    slow_init(&s->dev, late);
    cfg.backend = &s->dev.be;
    cfg.cmd_mem = s->cmds;
    cfg.cmd_mem_size = sizeof(s->cmds);
    cfg.strip_mem = s->strip;
    cfg.strip_mem_size = (size_t)ROWS * W * sizeof(cydui_color) * (size_t)(buffers == 2 ? 2 : 1);
    cfg.strip_buffers = (uint8_t)buffers;
    cfg.text_mem = s->text;
    cfg.text_mem_size = sizeof(s->text);
    cfg.dirty_mem = dirty ? s->dirty : NULL;
    cfg.dirty_mem_size = dirty ? sizeof(s->dirty) : 0;
    return cydui_init(&s->ctx, &cfg);
}

/* ---- scenes: a list of shapes where a frame changes one or two of them ---- */

static uint32_t rnd(uint32_t *s)
{
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

#define ROW_A 0xF0, 0x0F, 0xF0, 0x0F, 0xF0
#define ROW_B 0x11, 0x22, 0x33, 0x44, 0x55
/* two 10 x 10 masks of 4 bits per pixel (5 bytes a row) that differ, at different addresses */
static const uint8_t mask_a[50] = {ROW_A, ROW_A, ROW_A, ROW_A, ROW_A, ROW_A, ROW_A, ROW_A, ROW_A, ROW_A};
static const uint8_t mask_b[50] = {ROW_B, ROW_B, ROW_B, ROW_B, ROW_B, ROW_B, ROW_B, ROW_B, ROW_B, ROW_B};

static const char *const words[4] = {"abc", "xyz", "Oi!", "123"};

/* shape `i` in variant `v` */
static void shape(cydui_ctx *c, int i, uint32_t v)
{
    uint32_t s = 0x9E3779B9u * (uint32_t)(i + 1) + v * 0x85EBCA6Bu;
    cydui_rect r;
    cydui_color col;

    rnd(&s);
    r.x = (int16_t)(rnd(&s) % 50);
    r.y = (int16_t)(rnd(&s) % 40);
    r.w = (int16_t)(4 + rnd(&s) % 20);
    r.h = (int16_t)(3 + rnd(&s) % 14);
    col = (cydui_color)rnd(&s);
    switch (i % 6) {
    case 0: cydui_draw_rect(c, r, col, (uint8_t)(100 + rnd(&s) % 156)); break;
    case 1: cydui_draw_rrect(c, r, (uint8_t)(rnd(&s) % 8), col, 255); break;
    case 2: cydui_draw_line(c, r.x, r.y, (int16_t)(r.x + r.w), (int16_t)(r.y + r.h), (uint8_t)(1 + rnd(&s) % 3), col, 255); break;
    case 3: cydui_draw_gradient(c, r, col, (cydui_color)rnd(&s), (rnd(&s) & 1u) != 0, 255); break;
    case 4: cydui_draw_text(c, &cydui_sans_12, r.x, r.y, 0, CYDUI_ALIGN_LEFT, words[rnd(&s) % 4], CYDUI_NTS, col, 255); break;
    default: cydui_draw_bitmap(c, (cydui_rect){r.x, r.y, 10, 10}, (rnd(&s) & 1u) ? mask_a : mask_b, sizeof(mask_a),
                               CYDUI_BITMAP_A4, col, 255); break;
    }
}

#define SHAPES 12
static uint32_t variant[SHAPES];

static void frame(cydui_ctx *c, cydui_color clear)
{
    int i;
    cydui_begin_frame(c, clear);
    for (i = 0; i < SHAPES; i++) {
        shape(c, i, variant[i]);
    }
    cydui_end_frame(c);
}

static void reset_variants(void)
{
    memset(variant, 0, sizeof(variant));
}

/* ---- two buffers ---- */

static void test_the_buffers_validate(void)
{
    static sys_t s;
    cydui_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&s, false, 0, false));
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&s, false, 1, false));
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&s, false, 2, false));
    TEST_ASSERT_EQUAL_INT(ROWS, cydui_strip_height(&s.ctx)); /* the memory holds two strips of ROWS rows */

    cfg.backend = &s.dev.be;
    cfg.cmd_mem = s.cmds;
    cfg.cmd_mem_size = sizeof(s.cmds);
    cfg.strip_mem = s.strip;
    cfg.strip_mem_size = sizeof(s.strip);
    cfg.strip_buffers = 3;
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_init(&s.ctx, &cfg));
    cfg.strip_buffers = 2;
    cfg.strip_mem_size = (size_t)W * sizeof(cydui_color) * 3u; /* three rows: one row per buffer is the most that fits */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_init(&s.ctx, &cfg));
    TEST_ASSERT_EQUAL_INT(1, cydui_strip_height(&s.ctx));
    cfg.strip_mem_size = (size_t)W * sizeof(cydui_color); /* one row: nothing for two */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_NO_MEMORY, cydui_init(&s.ctx, &cfg));
}

static void test_two_buffers_make_a_slow_transfer_safe_and_one_buffer_does_not(void)
{
    static sys_t ref, two, one;
    int f, differing_with_one = 0;

    reset_variants();
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&ref, false, 1, false));
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&two, true, 2, false));
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&one, true, 1, false)); /* a backend that is slow, with one buffer: the contract is broken */
    for (f = 0; f < 12; f++) {
        variant[f % SHAPES] += 1u;
        frame(&ref.ctx, 0);
        frame(&two.ctx, 0);
        frame(&one.ctx, 0);
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(ref.dev.fb, two.dev.fb, sizeof(ref.dev.fb), "two buffers and a slow transfer changed the picture");
        differing_with_one += memcmp(ref.dev.fb, one.dev.fb, sizeof(ref.dev.fb)) != 0;
    }
    TEST_ASSERT_TRUE_MESSAGE(differing_with_one > 0, "the slow backend does not break one buffer: the test proves nothing");
}

/* ---- dirty regions ---- */

static void test_an_identical_frame_sends_nothing_and_the_first_sends_everything(void)
{
    static sys_t s;

    reset_variants();
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&s, false, 1, true));
    frame(&s.ctx, 0);
    TEST_ASSERT_EQUAL_INT(STRIPS, s.dev.flushes);
    frame(&s.ctx, 0);
    TEST_ASSERT_EQUAL_INT(STRIPS, s.dev.flushes); /* nothing new */
    frame(&s.ctx, 0);
    TEST_ASSERT_EQUAL_INT(STRIPS, s.dev.flushes);
}

static void test_a_change_sends_only_the_strips_it_touches(void)
{
    static sys_t s;
    int before, i;

    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&s, false, 1, true));
    cydui_begin_frame(&s.ctx, 0);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 2, 8, 3}, 0x1234, 255);   /* strip 0 */
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 20, 8, 3}, 0x4321, 255);  /* strip 3 */
    cydui_end_frame(&s.ctx);
    before = s.dev.flushes;
    cydui_begin_frame(&s.ctx, 0);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 2, 8, 3}, 0x1234, 255);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 20, 8, 3}, 0x4322, 255);  /* another color */
    cydui_end_frame(&s.ctx);
    TEST_ASSERT_EQUAL_INT(before + 1, s.dev.flushes);
    TEST_ASSERT_EQUAL_INT(18, s.dev.ys[before]); /* strip 3: rows 18..23 */

    before = s.dev.flushes;
    cydui_begin_frame(&s.ctx, 0);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 2, 8, 3}, 0x1234, 255);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 17, 8, 8}, 0x4322, 255); /* now it spans strips 2, 3 and 4 */
    cydui_end_frame(&s.ctx);
    TEST_ASSERT_EQUAL_INT(before + 3, s.dev.flushes);
    for (i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL_INT(12 + 6 * i, s.dev.ys[before + i]);
    }
}

static void test_a_command_on_the_edge_of_a_strip_belongs_to_that_strip_only(void)
{
    static sys_t s;
    int before;

    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&s, false, 1, true));
    cydui_begin_frame(&s.ctx, 0);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 6, 8, 3}, 0x1111, 255);   /* rows 6..8: begins on the first row of strip 1 */
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 15, 8, 3}, 0x2222, 255);  /* rows 15..17: ends on the last row of strip 2 */
    cydui_end_frame(&s.ctx);
    before = s.dev.flushes;
    cydui_begin_frame(&s.ctx, 0);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 6, 8, 3}, 0x1112, 255);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 15, 8, 3}, 0x2222, 255);
    cydui_end_frame(&s.ctx);
    TEST_ASSERT_EQUAL_INT(before + 1, s.dev.flushes);
    TEST_ASSERT_EQUAL_INT(6, s.dev.ys[before]);
    before = s.dev.flushes;
    cydui_begin_frame(&s.ctx, 0);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 6, 8, 3}, 0x1112, 255);
    cydui_draw_rect(&s.ctx, (cydui_rect){2, 15, 8, 3}, 0x2223, 255);
    cydui_end_frame(&s.ctx);
    TEST_ASSERT_EQUAL_INT(before + 1, s.dev.flushes);
    TEST_ASSERT_EQUAL_INT(12, s.dev.ys[before]);
}

static void test_the_background_and_an_invalidation_resend_everything(void)
{
    static sys_t s;

    reset_variants();
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&s, false, 1, true));
    frame(&s.ctx, 0);
    frame(&s.ctx, 0x00FF);
    TEST_ASSERT_EQUAL_INT(2 * STRIPS, s.dev.flushes);
    frame(&s.ctx, 0x00FF);
    TEST_ASSERT_EQUAL_INT(2 * STRIPS, s.dev.flushes);
    cydui_invalidate(&s.ctx);
    frame(&s.ctx, 0x00FF);
    TEST_ASSERT_EQUAL_INT(3 * STRIPS, s.dev.flushes);
    frame(&s.ctx, 0x00FF);
    TEST_ASSERT_EQUAL_INT(3 * STRIPS, s.dev.flushes);
}

/* every field of every kind of command must mark its strip as changed */
typedef void (*variant_fn)(cydui_ctx *c, int v);

static void v_rect_color(cydui_ctx *c, int v) { cydui_draw_rect(c, (cydui_rect){4, 4, 9, 9}, v ? 0x1111 : 0x2222, 255); }
static void v_rect_alpha(cydui_ctx *c, int v) { cydui_draw_rect(c, (cydui_rect){4, 4, 9, 9}, 0x1111, v ? 200 : 201); }
static void v_rect_pos(cydui_ctx *c, int v) { cydui_draw_rect(c, (cydui_rect){(int16_t)(4 + v), 4, 9, 9}, 0x1111, 255); }
static void v_rect_size(cydui_ctx *c, int v) { cydui_draw_rect(c, (cydui_rect){4, 4, (int16_t)(9 + v), 9}, 0x1111, 255); }
static void v_rrect_radius(cydui_ctx *c, int v) { cydui_draw_rrect(c, (cydui_rect){4, 4, 12, 12}, (uint8_t)(2 + v), 0x1111, 255); }
static void v_rrect_color(cydui_ctx *c, int v) { cydui_draw_rrect(c, (cydui_rect){4, 4, 12, 12}, 3, (cydui_color)(0x1111 + v), 255); }
static void v_outline_thickness(cydui_ctx *c, int v) { cydui_draw_rrect_outline(c, (cydui_rect){4, 4, 12, 12}, 3, (uint8_t)(1 + v), 0x1111, 255); }
static void v_line_end(cydui_ctx *c, int v) { cydui_draw_line(c, 4, 4, (int16_t)(20 + v), 12, 1, 0x1111, 255); }
static void v_line_start(cydui_ctx *c, int v) { cydui_draw_line(c, (int16_t)(4 + v), 4, 20, 12, 1, 0x1111, 255); }
static void v_line_color(cydui_ctx *c, int v) { cydui_draw_line(c, 4, 4, 20, 12, 1, (cydui_color)(0x1111 + v), 255); }
/* 2 and 3 px lines have the same bounding box: only the thickness differs */
static void v_line_thickness(cydui_ctx *c, int v) { cydui_draw_line(c, 4, 4, 20, 12, (uint8_t)(2 + v), 0x1111, 255); }
static void v_gradient_c0(cydui_ctx *c, int v) { cydui_draw_gradient(c, (cydui_rect){4, 4, 12, 12}, (cydui_color)(0x1111 + v), 0x2222, true, 255); }
static void v_gradient_c1(cydui_ctx *c, int v) { cydui_draw_gradient(c, (cydui_rect){4, 4, 12, 12}, 0x1111, (cydui_color)(0x2222 + v), true, 255); }
static void v_gradient_dir(cydui_ctx *c, int v) { cydui_draw_gradient(c, (cydui_rect){4, 4, 12, 12}, 0x1111, 0x2222, v != 0, 255); }
/* "abc" and "cba" are as wide as each other: the box is the same and only the characters differ */
static void v_text_content(cydui_ctx *c, int v) { cydui_draw_text(c, &cydui_sans_12, 4, 4, 0, CYDUI_ALIGN_LEFT, v ? "abc" : "cba", CYDUI_NTS, 0xFFFF, 255); }
static cydui_font font_copy;
static void v_text_font(cydui_ctx *c, int v)
{
    font_copy = cydui_sans_12; /* the very same metrics, another font */
    cydui_draw_text(c, v ? &font_copy : &cydui_sans_12, 4, 4, 0, CYDUI_ALIGN_LEFT, "abc", CYDUI_NTS, 0xFFFF, 255);
}
static void v_text_color(cydui_ctx *c, int v) { cydui_draw_text(c, &cydui_sans_12, 4, 4, 0, CYDUI_ALIGN_LEFT, "abc", CYDUI_NTS, (cydui_color)(0xFFFF - v), 255); }
static void v_text_wrap(cydui_ctx *c, int v) { cydui_draw_text(c, &cydui_sans_12, 4, 4, (int16_t)(40 + v), CYDUI_ALIGN_LEFT, "abc def", CYDUI_NTS, 0xFFFF, 255); }
static void v_text_align(cydui_ctx *c, int v) { cydui_draw_text(c, &cydui_sans_12, 4, 4, 40, v ? CYDUI_ALIGN_RIGHT : CYDUI_ALIGN_LEFT, "abc", CYDUI_NTS, 0xFFFF, 255); }
static void v_text_len(cydui_ctx *c, int v) { cydui_draw_text(c, &cydui_sans_12, 4, 4, 0, CYDUI_ALIGN_LEFT, "abcd", (size_t)(3 + v), 0xFFFF, 255); }
static void v_bitmap_data(cydui_ctx *c, int v) { cydui_draw_bitmap(c, (cydui_rect){4, 4, 10, 10}, v ? mask_a : mask_b, sizeof(mask_a), CYDUI_BITMAP_A4, 0xFFFF, 255); }
static void v_bitmap_tint(cydui_ctx *c, int v) { cydui_draw_bitmap(c, (cydui_rect){4, 4, 10, 10}, mask_a, sizeof(mask_a), CYDUI_BITMAP_A4, (cydui_color)(0xFFFF - v), 255); }
static void v_clip(cydui_ctx *c, int v) { cydui_set_clip(c, (cydui_rect){0, 0, (int16_t)(10 + v), 48}); cydui_draw_rect(c, (cydui_rect){4, 4, 20, 9}, 0x1111, 255); cydui_reset_clip(c); }
static void v_order(cydui_ctx *c, int v)
{
    cydui_draw_rect(c, (cydui_rect){4, 4, 9, 9}, v ? 0x1111 : 0x2222, 255);
    cydui_draw_rect(c, (cydui_rect){6, 6, 9, 9}, v ? 0x2222 : 0x1111, 255);
}

static const struct { const char *name; variant_fn fn; } fields[] = {
    {"rect color", v_rect_color}, {"rect alpha", v_rect_alpha}, {"rect position", v_rect_pos}, {"rect size", v_rect_size},
    {"rrect radius", v_rrect_radius}, {"rrect color", v_rrect_color}, {"outline thickness", v_outline_thickness},
    {"line end", v_line_end}, {"line start", v_line_start}, {"line color", v_line_color}, {"line thickness", v_line_thickness},
    {"gradient c0", v_gradient_c0}, {"gradient c1", v_gradient_c1}, {"gradient direction", v_gradient_dir},
    {"text content", v_text_content}, {"text color", v_text_color}, {"text wrap", v_text_wrap}, {"text align", v_text_align},
    {"text length", v_text_len}, {"text font", v_text_font}, {"bitmap data pointer", v_bitmap_data}, {"bitmap tint", v_bitmap_tint},
    {"clip", v_clip}, {"order of commands", v_order},
};

static void test_every_field_of_every_command_marks_its_strip(void)
{
    static sys_t s;
    size_t i;

    for (i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        int before;
        char msg[64];

        TEST_ASSERT_EQUAL_UINT32(0, sys_init(&s, false, 1, true));
        cydui_begin_frame(&s.ctx, 0);
        fields[i].fn(&s.ctx, 0);
        cydui_end_frame(&s.ctx);
        before = s.dev.flushes;
        cydui_begin_frame(&s.ctx, 0);
        fields[i].fn(&s.ctx, 0);
        cydui_end_frame(&s.ctx);
        snprintf(msg, sizeof(msg), "'%s': the same frame was sent again", fields[i].name);
        TEST_ASSERT_EQUAL_INT_MESSAGE(before, s.dev.flushes, msg);
        cydui_begin_frame(&s.ctx, 0);
        fields[i].fn(&s.ctx, 1);
        cydui_end_frame(&s.ctx);
        snprintf(msg, sizeof(msg), "'%s': a change was not sent", fields[i].name);
        TEST_ASSERT_TRUE_MESSAGE(s.dev.flushes > before, msg);
    }
}

static void test_the_padding_of_the_commands_does_not_decide(void)
{
    static sys_t a, b;
    int f;

    reset_variants();
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&a, false, 1, true));
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(&b, false, 1, true));
    memset(a.cmds, 0xAA, sizeof(a.cmds));
    memset(b.cmds, 0x00, sizeof(b.cmds));
    memset(a.text, 0x55, sizeof(a.text));
    for (f = 0; f < 4; f++) {
        frame(&a.ctx, 0);
        frame(&b.ctx, 0);
        TEST_ASSERT_EQUAL_INT(a.dev.flushes, b.dev.flushes);
    }
    TEST_ASSERT_EQUAL_INT(STRIPS, a.dev.flushes);
}

static void test_the_dirty_memory_must_fit_the_strips(void)
{
    static sys_t s;
    cydui_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    slow_init(&s.dev, false);
    cfg.backend = &s.dev.be;
    cfg.cmd_mem = s.cmds;
    cfg.cmd_mem_size = sizeof(s.cmds);
    cfg.strip_mem = s.strip;
    cfg.strip_mem_size = (size_t)ROWS * W * sizeof(cydui_color);
    cfg.dirty_mem = s.dirty;
    cfg.dirty_mem_size = (STRIPS - 1) * sizeof(uint64_t); /* one strip short */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_NO_MEMORY, cydui_init(&s.ctx, &cfg));
    cfg.dirty_mem_size = STRIPS * sizeof(uint64_t);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_init(&s.ctx, &cfg));
    cfg.dirty_mem = (char *)s.dirty + 1; /* not aligned for a uint64_t */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_NO_MEMORY, cydui_init(&s.ctx, &cfg));
}

static void test_a_height_that_is_not_a_multiple_of_the_strip_counts_the_last_partial_strip(void)
{
    static sys_t s;
    cydui_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    slow_init(&s.dev, false);
    s.dev.be.height = 45; /* 7 strips of 6 rows and one of 3 */
    cfg.backend = &s.dev.be;
    cfg.cmd_mem = s.cmds;
    cfg.cmd_mem_size = sizeof(s.cmds);
    cfg.strip_mem = s.strip;
    cfg.strip_mem_size = (size_t)ROWS * W * sizeof(cydui_color);
    cfg.dirty_mem = s.dirty;
    cfg.dirty_mem_size = 7 * sizeof(uint64_t);
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_NO_MEMORY, cydui_init(&s.ctx, &cfg));
    cfg.dirty_mem_size = 8 * sizeof(uint64_t);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_init(&s.ctx, &cfg));
    cydui_begin_frame(&s.ctx, 0);
    cydui_end_frame(&s.ctx);
    TEST_ASSERT_EQUAL_INT(8, s.dev.flushes);
    TEST_ASSERT_EQUAL_INT(42, s.dev.ys[7]);
}

/* ---- the property ---- */

static void test_with_dirty_regions_and_two_slow_buffers_the_screen_is_always_the_plain_one(void)
{
    static sys_t ref, fast;
    uint32_t seed;
    int f, sent_fast = 0;

    for (seed = 1; seed <= 4; seed++) {
        uint32_t s = seed * 7919u;

        reset_variants();
        TEST_ASSERT_EQUAL_UINT32(0, sys_init(&ref, false, 1, false));
        TEST_ASSERT_EQUAL_UINT32(0, sys_init(&fast, true, 2, true));
        for (f = 0; f < 60; f++) {
            int changes = (int)(rnd(&s) % 3u); /* often nothing changes, at times one or two shapes */
            while (changes-- > 0) {
                variant[rnd(&s) % SHAPES] += 1u + rnd(&s) % 3u;
            }
            if (f % 17 == 16) {
                cydui_invalidate(&fast.ctx);
            }
            frame(&ref.ctx, (cydui_color)(f % 23 == 22 ? 0x00F0 : 0));
            frame(&fast.ctx, (cydui_color)(f % 23 == 22 ? 0x00F0 : 0));
            TEST_ASSERT_EQUAL_MEMORY_MESSAGE(ref.dev.fb, fast.dev.fb, sizeof(ref.dev.fb), "dirty regions changed the picture");
        }
        sent_fast += fast.dev.flushes;
        TEST_ASSERT_TRUE_MESSAGE(fast.dev.flushes < ref.dev.flushes, "no strip was ever skipped: the test proves little");
    }
    TEST_ASSERT_TRUE(sent_fast > 0);
}

/* ---- the frame budget ---- */

static uirig rig;

static void budget_start(unsigned budget_ms)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    rig.ui.frame_budget_ms = (uint16_t)budget_ms;
}

static void test_a_frame_over_its_budget_skips_the_widgets_that_are_left(void)
{
    budget_start(10);
    rig.mem.now = 100;
    cydui_ui_begin(&rig.ui, 0);
    cydui_label(&rig.ui, "a");
    rig.mem.now = 110; /* exactly the budget: still fine */
    cydui_label(&rig.ui, "b");
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui) & CYDUI_ERR_FRAME_BUDGET);
    TEST_ASSERT_EQUAL_UINT16(2, rig.ui.widgets);
    rig.mem.now = 111; /* one ms over */
    TEST_ASSERT_FALSE(cydui_button(&rig.ui, "c"));
    cydui_label(&rig.ui, "d");
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_FRAME_BUDGET);
    TEST_ASSERT_EQUAL_UINT16(2, rig.ui.widgets); /* c and d were not counted or drawn */
    cydui_ui_end(&rig.ui);

    cydui_ui_clear_errors(&rig.ui);
    rig.mem.now = 500; /* the next frame has a budget of its own */
    cydui_ui_begin(&rig.ui, 0);
    cydui_label(&rig.ui, "a");
    TEST_ASSERT_EQUAL_UINT16(1, rig.ui.widgets);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    cydui_ui_end(&rig.ui);
}

static void test_without_a_budget_or_a_clock_nothing_is_skipped(void)
{
    int i;

    budget_start(0);
    rig.mem.now = 0;
    cydui_ui_begin(&rig.ui, 0);
    rig.mem.now = 100000;
    for (i = 0; i < 5; i++) cydui_label(&rig.ui, "x");
    TEST_ASSERT_EQUAL_UINT16(5, rig.ui.widgets);
    cydui_ui_end(&rig.ui);

    budget_start(10);
    rig.mem.backend.now_ms = NULL; /* a budget needs a clock */
    cydui_ui_begin(&rig.ui, 0);
    for (i = 0; i < 5; i++) cydui_label(&rig.ui, "x");
    TEST_ASSERT_EQUAL_UINT16(5, rig.ui.widgets);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    cydui_ui_end(&rig.ui);
}

static void test_the_budget_survives_the_clock_wrap(void)
{
    budget_start(10);
    rig.mem.now = 0xFFFFFFF8u;
    cydui_ui_begin(&rig.ui, 0);
    rig.mem.now = 4; /* 12 ms later, across the wrap */
    cydui_label(&rig.ui, "x");
    TEST_ASSERT_EQUAL_UINT16(0, rig.ui.widgets);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_FRAME_BUDGET);
    cydui_ui_end(&rig.ui);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_buffers_validate);
    RUN_TEST(test_two_buffers_make_a_slow_transfer_safe_and_one_buffer_does_not);
    RUN_TEST(test_an_identical_frame_sends_nothing_and_the_first_sends_everything);
    RUN_TEST(test_a_change_sends_only_the_strips_it_touches);
    RUN_TEST(test_a_command_on_the_edge_of_a_strip_belongs_to_that_strip_only);
    RUN_TEST(test_the_background_and_an_invalidation_resend_everything);
    RUN_TEST(test_every_field_of_every_command_marks_its_strip);
    RUN_TEST(test_the_padding_of_the_commands_does_not_decide);
    RUN_TEST(test_the_dirty_memory_must_fit_the_strips);
    RUN_TEST(test_a_height_that_is_not_a_multiple_of_the_strip_counts_the_last_partial_strip);
    RUN_TEST(test_with_dirty_regions_and_two_slow_buffers_the_screen_is_always_the_plain_one);
    RUN_TEST(test_a_frame_over_its_budget_skips_the_widgets_that_are_left);
    RUN_TEST(test_without_a_budget_or_a_clock_nothing_is_skipped);
    RUN_TEST(test_the_budget_survives_the_clock_wrap);
    return UNITY_END();
}
