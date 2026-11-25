/*
 * The post-effect hook (FR-07): called once for every strip that is sent, with the rows of the strip in
 * screen coordinates, after the strip is drawn and before it is handed to the backend; independent of how
 * the screen is cut; a part of what decides that a strip is sent again (dirty regions); removable.
 */

#include <string.h>

#include "cydui_sans_12.h"
#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define W 64
#define H 48

typedef struct {
    cydui_backend be;
    cydui_color fb[W * H];
    int flushes, flushed_rows[H];
} dev_t;

static void dev_flush(void *u, const cydui_rect *a, const cydui_color *px)
{
    dev_t *d = (dev_t *)u;
    int y;
    for (y = 0; y < a->h; y++) {
        memcpy(&d->fb[(a->y + y) * W], &px[y * a->w], (size_t)a->w * sizeof(cydui_color));
        d->flushed_rows[a->y + y]++;
    }
    d->flushes++;
}

typedef struct {
    dev_t dev;
    cydui_ctx ctx;
    cydui_cmd cmds[32];
    cydui_color strip[W * H];
    char text[64];
    uint64_t dirty[H];
} sys_t;

static sys_t s;

static uint32_t sys_init(int rows, bool dirty)
{
    cydui_config cfg;

    memset(&s, 0, sizeof(s));
    s.dev.be.user = &s.dev;
    s.dev.be.width = W;
    s.dev.be.height = H;
    s.dev.be.flush_strip = dev_flush;
    memset(&cfg, 0, sizeof(cfg));
    cfg.backend = &s.dev.be;
    cfg.cmd_mem = s.cmds;
    cfg.cmd_mem_size = sizeof(s.cmds);
    cfg.strip_mem = s.strip;
    cfg.strip_mem_size = (size_t)rows * W * sizeof(cydui_color);
    cfg.text_mem = s.text;
    cfg.text_mem_size = sizeof(s.text);
    cfg.dirty_mem = dirty ? s.dirty : NULL;
    cfg.dirty_mem_size = dirty ? sizeof(s.dirty) : 0;
    return cydui_init(&s.ctx, &cfg);
}

/* ---- effects to try ---- */

static int calls, rows_seen[H], bad_geometry;
static uint32_t key_value; /* what the key says for the rows [key_from, key_to) */
static int key_from, key_to, key_calls;

static void reset_counters(void)
{
    calls = key_calls = bad_geometry = 0;
    memset(rows_seen, 0, sizeof(rows_seen));
}

/* Darkens every row that is even on the screen: depends on y0 + row, not on the strip. */
static void scan_apply(void *user, cydui_color *buf, int16_t width, int16_t y0, int16_t h)
{
    int row, x;
    (void)user;
    calls++;
    if (width != W || y0 < 0 || h < 1 || y0 + h > H) {
        bad_geometry++;
        return;
    }
    for (row = 0; row < h; row++) {
        rows_seen[y0 + row]++;
        if (((y0 + row) & 1) == 0) {
            for (x = 0; x < width; x++) {
                buf[row * width + x] = cydui_blend(buf[row * width + x], 0, 128);
            }
        }
    }
}

static uint32_t scan_key(void *user, int16_t y0, int16_t h)
{
    (void)user;
    key_calls++;
    return (y0 < key_to && y0 + h > key_from) ? key_value : 1u;
}

static const cydui_post_effect scan = {scan_key, scan_apply, NULL};

static void scene(cydui_ctx *c, cydui_color clear)
{
    cydui_begin_frame(c, clear);
    cydui_draw_rect(c, (cydui_rect){4, 3, 40, 30}, 0xF800, 255);
    cydui_draw_rrect(c, (cydui_rect){10, 20, 50, 20}, 6, 0x07E0, 255);
    cydui_draw_text(c, &cydui_sans_12, 6, 6, 0, CYDUI_ALIGN_LEFT, "Hi", CYDUI_NTS, 0xFFFF, 255);
    cydui_end_frame(c);
}

static uint64_t fb_hash(const cydui_color *fb)
{
    uint64_t h = 1469598103934665603ull;
    int i;
    for (i = 0; i < W * H; i++) {
        h = (h ^ fb[i]) * 1099511628211ull;
    }
    return h;
}

/* ---- tests ---- */

void test_the_effect_sees_every_strip_once_with_its_rows(void)
{
    int y;
    reset_counters();
    key_from = key_to = 0;
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(7, false)); /* 48 rows in strips of 7: the last one has 6 */
    cydui_set_post_effect(&s.ctx, &scan);
    scene(&s.ctx, 0x001F);
    TEST_ASSERT_EQUAL_INT(0, bad_geometry);
    TEST_ASSERT_EQUAL_INT(7, calls);
    TEST_ASSERT_EQUAL_INT(7, s.dev.flushes);
    for (y = 0; y < H; y++) {
        TEST_ASSERT_EQUAL_INT(1, rows_seen[y]);
    }
}

void test_the_effect_changes_the_pixels_that_are_sent_and_not_the_strip_split(void)
{
    static const int rows[] = {1, 5, 7, 16, 48};
    cydui_color plain[W * H];
    uint64_t first = 0;
    size_t i;
    int y, x;

    TEST_ASSERT_EQUAL_UINT32(0, sys_init(48, false));
    scene(&s.ctx, 0x001F);
    memcpy(plain, s.dev.fb, sizeof(plain));
    for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        reset_counters();
        TEST_ASSERT_EQUAL_UINT32(0, sys_init(rows[i], false));
        cydui_set_post_effect(&s.ctx, &scan);
        scene(&s.ctx, 0x001F);
        if (i == 0) first = fb_hash(s.dev.fb);
        TEST_ASSERT_EQUAL_UINT64(first, fb_hash(s.dev.fb));
    }
    for (y = 0; y < H; y++) { /* even rows are the plain ones darkened, odd rows are untouched */
        for (x = 0; x < W; x++) {
            cydui_color want = (y & 1) ? plain[y * W + x] : cydui_blend(plain[y * W + x], 0, 128);
            TEST_ASSERT_EQUAL_HEX16(want, s.dev.fb[y * W + x]);
        }
    }
}

void test_without_an_effect_nothing_changes_and_one_can_be_removed(void)
{
    uint64_t plain, with, again;
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(8, false));
    scene(&s.ctx, 0x001F);
    plain = fb_hash(s.dev.fb);
    cydui_set_post_effect(&s.ctx, &scan);
    scene(&s.ctx, 0x001F);
    with = fb_hash(s.dev.fb);
    TEST_ASSERT_NOT_EQUAL(plain, with);
    cydui_set_post_effect(&s.ctx, NULL);
    scene(&s.ctx, 0x001F);
    again = fb_hash(s.dev.fb);
    TEST_ASSERT_EQUAL_UINT64(plain, again);
    {
        cydui_post_effect half = {scan_key, NULL, NULL}; /* no apply: no effect */
        cydui_set_post_effect(&s.ctx, &scan);
        cydui_set_post_effect(&s.ctx, &half);
        reset_counters();
        scene(&s.ctx, 0x001F);
        TEST_ASSERT_EQUAL_INT(0, calls);
        TEST_ASSERT_EQUAL_INT(0, key_calls);
    }
}

void test_with_dirty_regions_a_constant_key_sends_nothing_again(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(8, true));
    key_from = key_to = 0;
    cydui_set_post_effect(&s.ctx, &scan);
    reset_counters();
    scene(&s.ctx, 0x001F);
    TEST_ASSERT_EQUAL_INT(6, calls);
    reset_counters();
    s.dev.flushes = 0;
    scene(&s.ctx, 0x001F); /* the same frame: the strips are the same and so is what the effect does to them */
    TEST_ASSERT_EQUAL_INT(0, calls);
    TEST_ASSERT_EQUAL_INT(0, s.dev.flushes);
    TEST_ASSERT_TRUE(key_calls > 0); /* but it was asked */
}

void test_a_key_that_changes_for_some_rows_sends_only_those_strips(void)
{
    int y;
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(8, true));
    key_from = 16;
    key_to = 24; /* exactly the third strip */
    key_value = 100;
    cydui_set_post_effect(&s.ctx, &scan);
    scene(&s.ctx, 0x001F);
    reset_counters();
    memset(s.dev.flushed_rows, 0, sizeof(s.dev.flushed_rows));
    key_value = 101; /* the glitch moved */
    scene(&s.ctx, 0x001F);
    TEST_ASSERT_EQUAL_INT(1, calls);
    for (y = 0; y < H; y++) {
        TEST_ASSERT_EQUAL_INT(y >= 16 && y < 24 ? 1 : 0, s.dev.flushed_rows[y]);
    }
}

void test_the_effect_is_applied_to_a_fresh_strip_every_time_never_stacked(void)
{
    uint64_t once;
    int i;
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(8, true));
    key_from = 0;
    key_to = H;
    key_value = 1;
    cydui_set_post_effect(&s.ctx, &scan);
    scene(&s.ctx, 0x001F);
    once = fb_hash(s.dev.fb);
    for (i = 0; i < 5; i++) { /* the key changes every frame: everything is drawn and sent again */
        key_value++;
        scene(&s.ctx, 0x001F);
        TEST_ASSERT_EQUAL_UINT64(once, fb_hash(s.dev.fb));
    }
}

void test_setting_or_removing_an_effect_sends_everything_again(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(8, true));
    key_from = key_to = 0;
    scene(&s.ctx, 0x001F);
    s.dev.flushes = 0;
    scene(&s.ctx, 0x001F);
    TEST_ASSERT_EQUAL_INT(0, s.dev.flushes);
    cydui_set_post_effect(&s.ctx, &scan);
    scene(&s.ctx, 0x001F);
    TEST_ASSERT_EQUAL_INT(6, s.dev.flushes);
    s.dev.flushes = 0;
    cydui_set_post_effect(&s.ctx, NULL);
    scene(&s.ctx, 0x001F);
    TEST_ASSERT_EQUAL_INT(6, s.dev.flushes);
}

static void invert_apply(void *user, cydui_color *buf, int16_t width, int16_t y0, int16_t h)
{
    int i;
    (void)user;
    (void)y0;
    for (i = 0; i < width * h; i++) buf[i] = (cydui_color)~buf[i];
}

void test_swapping_one_effect_for_another_with_the_same_key_sends_everything_again(void)
{
    static const cydui_post_effect invert = {scan_key, invert_apply, NULL}; /* the same key function as scan */
    uint64_t a;
    TEST_ASSERT_EQUAL_UINT32(0, sys_init(8, true));
    key_from = key_to = 0;
    cydui_set_post_effect(&s.ctx, &scan);
    scene(&s.ctx, 0x001F);
    a = fb_hash(s.dev.fb);
    s.dev.flushes = 0;
    cydui_set_post_effect(&s.ctx, &invert);
    scene(&s.ctx, 0x001F);
    TEST_ASSERT_EQUAL_INT(6, s.dev.flushes);
    TEST_ASSERT_NOT_EQUAL(a, fb_hash(s.dev.fb));
}

/* ---- the one of a theme ---- */

static void *seen_user;

static void theme_apply(void *user, cydui_color *buf, int16_t width, int16_t y0, int16_t h)
{
    (void)buf;
    (void)width;
    (void)y0;
    (void)h;
    seen_user = user;
}

static const cydui_post_effect theme_fx = {NULL, theme_apply, (void *)1};

void test_the_effect_of_a_theme_gets_the_ui_and_follows_the_theme(void)
{
    static uirig rig;
    cydui_theme with_fx;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    with_fx = rig.theme;
    with_fx.post = &theme_fx;
    seen_user = NULL;
    rig_empty_frame(&rig);
    TEST_ASSERT_NULL(seen_user); /* the theme in use has none */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &with_fx));
    rig_empty_frame(&rig);
    TEST_ASSERT_TRUE(seen_user == &rig.ui); /* the template's own user is replaced by the ui */
    seen_user = NULL;
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &rig.theme));
    rig_empty_frame(&rig);
    TEST_ASSERT_NULL(seen_user);
}

void test_the_effects_level_is_clamped_and_remembered(void)
{
    static uirig rig;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    TEST_ASSERT_EQUAL_UINT(CYDUI_FX_DEFAULT, cydui_effects_level(&rig.ui));
    cydui_set_effects_level(&rig.ui, CYDUI_FX_LOW);
    TEST_ASSERT_EQUAL_UINT(CYDUI_FX_LOW, cydui_effects_level(&rig.ui));
    cydui_set_effects_level(&rig.ui, 99);
    TEST_ASSERT_EQUAL_UINT(CYDUI_FX_FULL, cydui_effects_level(&rig.ui));
    cydui_set_effects_level(&rig.ui, 0);
    TEST_ASSERT_EQUAL_UINT(CYDUI_FX_NONE, cydui_effects_level(&rig.ui));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_effect_sees_every_strip_once_with_its_rows);
    RUN_TEST(test_the_effect_changes_the_pixels_that_are_sent_and_not_the_strip_split);
    RUN_TEST(test_without_an_effect_nothing_changes_and_one_can_be_removed);
    RUN_TEST(test_with_dirty_regions_a_constant_key_sends_nothing_again);
    RUN_TEST(test_a_key_that_changes_for_some_rows_sends_only_those_strips);
    RUN_TEST(test_the_effect_is_applied_to_a_fresh_strip_every_time_never_stacked);
    RUN_TEST(test_setting_or_removing_an_effect_sends_everything_again);
    RUN_TEST(test_swapping_one_effect_for_another_with_the_same_key_sends_everything_again);
    RUN_TEST(test_the_effect_of_a_theme_gets_the_ui_and_follows_the_theme);
    RUN_TEST(test_the_effects_level_is_clamped_and_remembered);
    return UNITY_END();
}
