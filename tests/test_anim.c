/*
 * Animation (FR-53, FR-55, CT-08): the easing curves, a tween that follows the injected clock, its
 * limits, and the widgets that change their look over the theme's duration.
 */

#include <string.h>

#include "cydui_sans_12.h"
#include "uirig.h"
#include "unity.h"

#define ONE CYDUI_ANIM_ONE

void setUp(void) {}
void tearDown(void) {}

/* ---- curves ---- */

void test_every_curve_starts_at_zero_and_ends_at_one(void)
{
    int e;
    for (e = 0; e < CYDUI_EASE_COUNT; e++) {
        TEST_ASSERT_EQUAL_INT32(0, cydui_ease_apply((cydui_ease)e, 0));
        TEST_ASSERT_EQUAL_INT32(ONE, cydui_ease_apply((cydui_ease)e, ONE));
        TEST_ASSERT_EQUAL_INT32(0, cydui_ease_apply((cydui_ease)e, -500));
        TEST_ASSERT_EQUAL_INT32(ONE, cydui_ease_apply((cydui_ease)e, ONE + 500));
    }
    TEST_ASSERT_EQUAL_INT32(ONE / 3, cydui_ease_apply(CYDUI_EASE_LINEAR, ONE / 3));
    TEST_ASSERT_EQUAL_INT32(ONE / 2, cydui_ease_apply((cydui_ease)99, ONE / 2)); /* an unknown curve is linear */
}

void test_curves_without_a_spring_never_go_back_or_past_the_end(void)
{
    static const cydui_ease plain[] = {CYDUI_EASE_LINEAR,     CYDUI_EASE_IN_QUAD,     CYDUI_EASE_OUT_QUAD,
                                       CYDUI_EASE_IN_OUT_QUAD, CYDUI_EASE_IN_CUBIC,    CYDUI_EASE_OUT_CUBIC,
                                       CYDUI_EASE_IN_OUT_CUBIC};
    size_t i;
    int t;
    for (i = 0; i < sizeof(plain) / sizeof(plain[0]); i++) {
        int32_t last = 0;
        for (t = 0; t <= ONE; t++) {
            int32_t v = cydui_ease_apply(plain[i], t);
            TEST_ASSERT_TRUE(v >= last && v <= ONE);
            last = v;
        }
    }
}

void test_curves_have_their_known_shape(void)
{
    TEST_ASSERT_EQUAL_INT32(ONE / 4, cydui_ease_apply(CYDUI_EASE_IN_QUAD, ONE / 2));
    TEST_ASSERT_EQUAL_INT32(ONE * 3 / 4, cydui_ease_apply(CYDUI_EASE_OUT_QUAD, ONE / 2));
    TEST_ASSERT_EQUAL_INT32(ONE / 8, cydui_ease_apply(CYDUI_EASE_IN_CUBIC, ONE / 2));
    TEST_ASSERT_EQUAL_INT32(ONE * 7 / 8, cydui_ease_apply(CYDUI_EASE_OUT_CUBIC, ONE / 2));
    TEST_ASSERT_EQUAL_INT32(ONE / 2, cydui_ease_apply(CYDUI_EASE_IN_OUT_QUAD, ONE / 2));
    TEST_ASSERT_EQUAL_INT32(ONE / 2, cydui_ease_apply(CYDUI_EASE_IN_OUT_CUBIC, ONE / 2));
    /* the in-out curves are symmetric around the middle */
    TEST_ASSERT_INT32_WITHIN(1, ONE, cydui_ease_apply(CYDUI_EASE_IN_OUT_CUBIC, 1000) + cydui_ease_apply(CYDUI_EASE_IN_OUT_CUBIC, ONE - 1000));
    TEST_ASSERT_INT32_WITHIN(1, ONE, cydui_ease_apply(CYDUI_EASE_IN_OUT_QUAD, 700) + cydui_ease_apply(CYDUI_EASE_IN_OUT_QUAD, ONE - 700));
}

void test_back_and_elastic_overshoot_and_come_back(void)
{
    int32_t max_back = 0, max_el = 0, min_el_late = ONE;
    int t;
    for (t = 0; t <= ONE; t++) {
        int32_t b = cydui_ease_apply(CYDUI_EASE_OUT_BACK, t), e = cydui_ease_apply(CYDUI_EASE_OUT_ELASTIC, t);
        max_back = b > max_back ? b : max_back;
        max_el = e > max_el ? e : max_el;
        if (t > ONE / 4 && e < min_el_late) min_el_late = e;
    }
    TEST_ASSERT_TRUE(max_back > ONE && max_back < ONE + ONE / 4); /* about 10 % over */
    TEST_ASSERT_TRUE(max_el > ONE + ONE / 8);
    TEST_ASSERT_TRUE(min_el_late < ONE); /* it swings to both sides of the end */
}

void test_lerp_is_safe_over_the_whole_range(void)
{
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, cydui_lerp(INT32_MIN, INT32_MAX, ONE));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, cydui_lerp(INT32_MAX, INT32_MIN, ONE));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, cydui_lerp(INT32_MIN, INT32_MAX, 0));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, cydui_lerp(INT32_MIN, INT32_MAX, ONE * 4)); /* extrapolation is clamped */
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, cydui_lerp(INT32_MIN, INT32_MAX, -ONE * 4));
    TEST_ASSERT_EQUAL_INT32(150, cydui_lerp(100, 200, ONE / 2));
    TEST_ASSERT_EQUAL_INT32(-150, cydui_lerp(-100, -200, ONE / 2));
}

void test_the_look_of_a_widget_in_transit_is_between_its_two_ends(void)
{
    cydui_theme t;
    cydui_paint_info pi;
    cydui_wstyle a, b, w;

    cydui_theme_default(&t, &cydui_sans_12);
    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_BUTTON;
    pi.state = CYDUI_STATE_PRESSED; /* heading for the pressed look, from the normal one */
    pi.from_state = CYDUI_STATE_NORMAL;
    a = t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL];
    b = t.table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_PRESSED];
    TEST_ASSERT_NOT_EQUAL(a.fill, b.fill);

    w = cydui_paint_look(&t, &pi); /* mix 0: settled, the target as it is */
    TEST_ASSERT_EQUAL_HEX16(b.fill, w.fill);
    pi.mix = 1;
    w = cydui_paint_look(&t, &pi);
    TEST_ASSERT_EQUAL_HEX16(a.fill, w.fill); /* hardly started: still the old look */
    pi.mix = ONE - 1;
    w = cydui_paint_look(&t, &pi);
    TEST_ASSERT_EQUAL_HEX16(b.fill, w.fill);
    pi.mix = ONE / 2;
    w = cydui_paint_look(&t, &pi);
    TEST_ASSERT_EQUAL_HEX16(cydui_blend(a.fill, b.fill, 127), w.fill);
    TEST_ASSERT_EQUAL_HEX16(cydui_blend(a.text, b.text, 127), w.text);
    pi.mix = ONE; /* a whole one is settled too */
    TEST_ASSERT_EQUAL_HEX16(b.fill, cydui_paint_look(&t, &pi).fill);
}

void test_how_selected_a_widget_is_follows_its_direction(void)
{
    cydui_paint_info pi;
    memset(&pi, 0, sizeof(pi));
    TEST_ASSERT_EQUAL_INT32(0, cydui_paint_amount(&pi));
    pi.selected = true;
    TEST_ASSERT_EQUAL_INT32(ONE, cydui_paint_amount(&pi));
    pi.mix = 1000; /* heading for selected: 1000 of the way */
    TEST_ASSERT_EQUAL_INT32(1000, cydui_paint_amount(&pi));
    pi.selected = false; /* heading for not selected: 1000 of the way down from selected */
    TEST_ASSERT_EQUAL_INT32(ONE - 1000, cydui_paint_amount(&pi));
}

/* ---- tweens ---- */

static uirig rig;
static int32_t value;
static int32_t target;
static uint16_t dur;
static cydui_ease ease;

static bool frame_tween(void)
{
    cydui_ui_begin(&rig.ui, 0);
    value = cydui_animate(&rig.ui, cydui_id_str(&rig.ui, "t"), 0, target, dur, ease);
    return cydui_ui_end(&rig.ui);
}

static void start(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    target = 1000;
    dur = 100;
    ease = CYDUI_EASE_LINEAR;
}

void test_a_tween_starts_at_from_and_follows_the_clock(void)
{
    start();
    TEST_ASSERT_TRUE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(0, value);
    rig_advance(&rig, 25);
    TEST_ASSERT_TRUE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(250, value);
    rig_advance(&rig, 50);
    TEST_ASSERT_TRUE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(750, value);
    rig_advance(&rig, 25); /* the last ms: it is there, and the frame no longer asks for more */
    TEST_ASSERT_FALSE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(1000, value);
    rig_advance(&rig, 5000);
    TEST_ASSERT_FALSE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(1000, value);
}

void test_a_new_target_goes_on_from_where_the_value_is(void)
{
    int32_t at;
    start();
    frame_tween();
    rig_advance(&rig, 50);
    frame_tween();
    at = value;
    TEST_ASSERT_EQUAL_INT32(500, at);
    target = -1000; /* the same frame: nothing jumps */
    TEST_ASSERT_TRUE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(at, value);
    rig_advance(&rig, 50);
    frame_tween();
    TEST_ASSERT_EQUAL_INT32(-250, value); /* halfway from 500 to -1000 */
    rig_advance(&rig, 50);
    TEST_ASSERT_FALSE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(-1000, value);
}

void test_no_duration_or_a_bad_curve_means_there_at_once(void)
{
    start();
    dur = 0;
    TEST_ASSERT_FALSE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(1000, value);
    dur = 100;
    ease = (cydui_ease)42;
    TEST_ASSERT_FALSE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(1000, value);
}

void test_a_duration_is_cut_to_the_maximum(void)
{
    start();
    dur = 7000; /* above the maximum, below twice of it */
    frame_tween();
    rig_advance(&rig, CYDUI_ANIM_MAX_MS - 1);
    TEST_ASSERT_TRUE(frame_tween());
    TEST_ASSERT_TRUE(value < 1000);
    rig_advance(&rig, 1);
    TEST_ASSERT_FALSE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(1000, value);
}

void test_the_clock_wrapping_or_going_back_does_not_break_a_tween(void)
{
    start();
    rig.mem.now = 0xFFFFFFF0u;
    frame_tween();
    rig.mem.now = 0x00000014u; /* 36 ms later, across the wrap */
    frame_tween();
    TEST_ASSERT_EQUAL_INT32(359, value); /* 36 ms of 100: 1474 / 4096, rounded down */
    rig.mem.now = 0xFFFFFF00u; /* the clock jumps back: the tween waits at its start */
    TEST_ASSERT_TRUE(frame_tween());
    TEST_ASSERT_TRUE(value >= 0 && value <= 1000);
    rig.mem.now += 1000;
    TEST_ASSERT_FALSE(frame_tween());
    TEST_ASSERT_EQUAL_INT32(1000, value);
}

void test_the_elastic_curve_stays_in_a_sane_range_and_lands_on_the_target(void)
{
    start();
    ease = CYDUI_EASE_OUT_ELASTIC;
    frame_tween();
    {
        int i;
        for (i = 0; i < 20; i++) {
            rig_advance(&rig, 5);
            frame_tween();
            TEST_ASSERT_TRUE(value > -1000 && value < 2000);
        }
    }
    TEST_ASSERT_EQUAL_INT32(1000, value);
}

void test_the_tweens_that_run_together_are_limited(void)
{
    int i, last = 0;
    start();
    for (i = 0; i < 2; i++) { /* the first frame creates them, the second moves them */
        int k;
        cydui_ui_begin(&rig.ui, 0);
        for (k = 0; k < CYDUI_MAX_TWEENS + 4; k++) {
            int32_t v;
            cydui_push_id_int(&rig.ui, k);
            v = cydui_animate(&rig.ui, cydui_id_str(&rig.ui, "t"), 0, 1000, 100, CYDUI_EASE_LINEAR);
            cydui_pop_id(&rig.ui);
            if (i == 1 && v == 1000) last++; /* the ones that did not get to move jumped */
        }
        cydui_ui_end(&rig.ui);
        rig_advance(&rig, 10);
    }
    TEST_ASSERT_EQUAL_INT(4, last);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_TWEEN_LIMIT);
    cydui_ui_clear_errors(&rig.ui);
    rig_advance(&rig, 500); /* all done: tweens may start again */
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_EQUAL_INT32(0, cydui_animate(&rig.ui, cydui_id_str(&rig.ui, "again"), 0, 1000, 100, CYDUI_EASE_LINEAR));
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

void test_a_new_ui_over_old_memory_does_not_inherit_the_state_of_the_old_one(void)
{
    cydui_id id;
    int32_t v;
    start();
    frame_tween();
    rig_advance(&rig, 60);
    frame_tween(); /* a tween halfway to 1000 */
    TEST_ASSERT_TRUE(value > 0 && value < 1000);
    TEST_ASSERT_EQUAL_UINT32(0, rig_restart(&rig)); /* the same memory again, full of what the first ui left in it */
    cydui_ui_begin(&rig.ui, 0);
    id = cydui_id_str(&rig.ui, "t");
    v = cydui_animate(&rig.ui, id, 0, 1000, 100, CYDUI_EASE_LINEAR);
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_INT32(0, v); /* it starts at `from`: nobody was moving it */
}

void test_a_tween_never_takes_the_place_of_the_state_of_something_else(void)
{
    cydui_id a, b;
    cydui_state *sa, *sb;
    int32_t first, second;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&rig, 3, 0)); /* three entries */
    cydui_ui_begin(&rig.ui, 0);
    a = cydui_id_str(&rig.ui, "scroll");
    b = cydui_id_str(&rig.ui, "field");
    sa = cydui_state_get(&rig.ui, a, true);
    sb = cydui_state_get(&rig.ui, b, true);
    TEST_ASSERT_NOT_NULL(sa);
    TEST_ASSERT_NOT_NULL(sb);
    sa->a = 77;
    sb->a = 88;
    first = cydui_animate(&rig.ui, cydui_id_str(&rig.ui, "one"), 0, 1000, 100, CYDUI_EASE_LINEAR);   /* takes the free entry */
    second = cydui_animate(&rig.ui, cydui_id_str(&rig.ui, "two"), 0, 1000, 100, CYDUI_EASE_LINEAR);  /* none left: no motion */
    TEST_ASSERT_EQUAL_INT32(0, first);
    TEST_ASSERT_EQUAL_INT32(1000, second);
    TEST_ASSERT_EQUAL_PTR(sa, cydui_state_get(&rig.ui, a, false));
    TEST_ASSERT_EQUAL_PTR(sb, cydui_state_get(&rig.ui, b, false));
    TEST_ASSERT_EQUAL_INT32(77, sa->a);
    TEST_ASSERT_EQUAL_INT32(88, sb->a);
    cydui_ui_end(&rig.ui);
}

void test_tweens_that_nobody_asks_for_are_recycled_and_nothing_grows(void)
{
    int frame;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&rig, 8, 0)); /* a table of 8 entries, far fewer than the tweens made */
    for (frame = 0; frame < 3000; frame++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_push_id_int(&rig.ui, frame); /* a new tween in every frame, never asked for again */
        cydui_animate(&rig.ui, cydui_id_str(&rig.ui, "t"), 0, 1000, 50, CYDUI_EASE_OUT_BACK);
        cydui_pop_id(&rig.ui);
        cydui_ui_end(&rig.ui);
        rig_advance(&rig, 16);
    }
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_FALSE(rig_empty_frame(&rig)); /* and nothing is left moving */
}

void test_the_age_of_a_theme_counts_from_the_first_frame_it_is_used(void)
{
    cydui_theme other;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    other = rig.theme;
    rig.mem.now = 5000;
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_theme_age(&rig.ui)); /* the first frame of the theme */
    cydui_ui_end(&rig.ui);
    rig_advance(&rig, 250);
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_EQUAL_UINT32(250, cydui_theme_age(&rig.ui));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_set_theme(&rig.ui, &other)); /* a new theme starts again, from the next frame */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_theme_age(&rig.ui));
    cydui_ui_end(&rig.ui);
    rig_advance(&rig, 70);
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_theme_age(&rig.ui));
    cydui_ui_end(&rig.ui);
    rig_advance(&rig, 70);
    cydui_ui_begin(&rig.ui, 0);
    TEST_ASSERT_EQUAL_UINT32(70, cydui_theme_age(&rig.ui));
    cydui_ui_end(&rig.ui);
}

/* ---- widgets over the duration of the theme ---- */

static bool on_;

static bool toggle_frame(void)
{
    cydui_ui_begin(&rig.ui, 0);
    cydui_toggle(&rig.ui, "Wi-Fi", &on_);
    return cydui_ui_end(&rig.ui);
}

static uint64_t toggle_hash(bool is_on, uint16_t anim_ms)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    rig.theme.style.anim_ms = anim_ms;
    on_ = is_on;
    toggle_frame();
    toggle_frame();
    return cydui_mem_hash(&rig.mem);
}

void test_a_switch_slides_over_the_duration_and_lands_on_the_plain_look(void)
{
    uint64_t off = toggle_hash(false, 0), on = toggle_hash(true, 0), mid, settled;

    TEST_ASSERT_NOT_EQUAL(off, on);
    TEST_ASSERT_EQUAL_UINT64(off, toggle_hash(false, 200)); /* a theme with a duration, nothing changed: the same */
    rig_down(&rig, 20, 20);
    toggle_frame();
    rig_up(&rig, 20, 20);
    toggle_frame(); /* the click: the value flips */
    TEST_ASSERT_TRUE(on_);
    rig_advance(&rig, 60);
    TEST_ASSERT_TRUE(toggle_frame()); /* still moving: the loop must go on */
    mid = cydui_mem_hash(&rig.mem);
    TEST_ASSERT_NOT_EQUAL(off, mid);
    TEST_ASSERT_NOT_EQUAL(on, mid);
    rig_advance(&rig, 400);
    TEST_ASSERT_FALSE(toggle_frame());
    settled = cydui_mem_hash(&rig.mem);
    TEST_ASSERT_EQUAL_UINT64(on, settled);
}

void test_a_theme_without_a_duration_changes_at_once(void)
{
    uint64_t on = toggle_hash(true, 0);
    toggle_hash(false, 0);
    rig_down(&rig, 20, 20);
    toggle_frame();
    rig_up(&rig, 20, 20);
    toggle_frame();
    toggle_frame();
    TEST_ASSERT_EQUAL_UINT64(on, cydui_mem_hash(&rig.mem));
}

static int tab_sel;

static void tabs_frame(void)
{
    static const char *const labels[] = {"One", "Two", "Three"};
    cydui_ui_begin(&rig.ui, 0);
    cydui_tabs(&rig.ui, labels, 3, &tab_sel);
    cydui_ui_end(&rig.ui);
}

void test_a_tab_changes_its_look_over_the_duration(void)
{
    uint64_t before, after, mid;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    rig.theme.style.anim_ms = 200;
    tab_sel = 0;
    tabs_frame();
    tabs_frame();
    before = cydui_mem_hash(&rig.mem);
    rig_down(&rig, 160, 20); /* the middle of the second tab */
    tabs_frame();
    rig_up(&rig, 160, 20);
    tabs_frame();
    TEST_ASSERT_EQUAL_INT(1, tab_sel);
    rig_advance(&rig, 80);
    tabs_frame();
    mid = cydui_mem_hash(&rig.mem);
    rig_advance(&rig, 600);
    tabs_frame();
    after = cydui_mem_hash(&rig.mem);
    TEST_ASSERT_NOT_EQUAL(before, mid);
    TEST_ASSERT_NOT_EQUAL(after, mid);
    TEST_ASSERT_NOT_EQUAL(before, after);
}


/* How far apart two RGB565 colors are: the sum of the differences of the three channels. */
static int color_dist(cydui_color a, cydui_color b)
{
    int dr = (a >> 11) - (b >> 11), dg = ((a >> 5) & 63) - ((b >> 5) & 63), db = (a & 31) - (b & 31);
    return (dr < 0 ? -dr : dr) + (dg < 0 ? -dg : dg) + (db < 0 ? -db : db);
}

static bool found_in_row(int y, cydui_color c)
{
    int x;
    for (x = 0; x < RIG_W; x++) {
        if (rig.fb[y * RIG_W + x] == c) return true;
    }
    return false;
}

static bool found_anywhere(cydui_color c, int y0, int y1)
{
    int y;
    for (y = y0; y < y1; y++) {
        if (found_in_row(y, c)) return true;
    }
    return false;
}

static void button_frame(void)
{
    cydui_ui_begin(&rig.ui, 0);
    cydui_button(&rig.ui, "Go");
    cydui_ui_end(&rig.ui);
}

void test_a_pressed_button_goes_from_its_normal_look_to_the_pressed_one(void)
{
    cydui_color normal, pressed, early, late;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    rig.theme.style.anim_ms = 200;
    button_frame();
    button_frame();
    normal = rig.fb[24 * RIG_W + 14]; /* inside the button, left of its label */
    rig_down(&rig, 20, 24);
    button_frame(); /* the press starts here */
    rig_advance(&rig, 10);
    button_frame();
    early = rig.fb[24 * RIG_W + 14];
    rig_advance(&rig, 600);
    button_frame();
    pressed = rig.fb[24 * RIG_W + 14];
    rig_advance(&rig, 1);
    TEST_ASSERT_NOT_EQUAL(normal, pressed);
    TEST_ASSERT_TRUE(color_dist(early, normal) < color_dist(early, pressed)); /* 10 ms in: still the old look */
    rig_up(&rig, 20, 24);
    button_frame(); /* the frame of the release still shows it pressed */
    button_frame(); /* and now it goes back */
    rig_advance(&rig, 190);
    button_frame();
    late = rig.fb[24 * RIG_W + 14];
    TEST_ASSERT_TRUE(color_dist(late, normal) < color_dist(late, pressed)); /* 190 of 200 ms back: nearly normal */
}

void test_a_tween_of_the_application_on_the_id_of_a_widget_does_not_fight_the_look_of_the_widget(void)
{
    int i;
    bool more = true;
    int32_t mine = -1;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    rig.theme.style.anim_ms = 200;
    for (i = 0; i < 60 && more; i++) {
        cydui_ui_begin(&rig.ui, 0);
        cydui_button(&rig.ui, "Go");
        mine = cydui_animate(&rig.ui, cydui_id_str(&rig.ui, "Go"), 0, 1000, 100, CYDUI_EASE_LINEAR); /* the id of the button */
        more = cydui_ui_end(&rig.ui);
        rig_advance(&rig, 16);
    }
    TEST_ASSERT_FALSE(more);
    TEST_ASSERT_EQUAL_INT32(1000, mine);
}

void test_a_settled_widget_does_not_keep_the_loop_awake_when_the_theme_animates(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    rig.theme.style.anim_ms = 200;
    on_ = true;
    toggle_frame();
    toggle_frame();
    TEST_ASSERT_FALSE(toggle_frame());
    button_frame();
    TEST_ASSERT_FALSE(rig_empty_frame(&rig));
}

void test_a_switch_track_blends_from_one_end_look_to_the_other(void)
{
    cydui_color off_fill, on_fill, expect_fwd, expect_back;
    int32_t k;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    rig.theme.style.anim_ms = 200;
    off_fill = rig.theme.table[CYDUI_WIDGET_TOGGLE][0][CYDUI_STATE_NORMAL].fill;
    on_fill = rig.theme.table[CYDUI_WIDGET_TOGGLE][1][CYDUI_STATE_NORMAL].fill;
    on_ = false;
    toggle_frame();
    toggle_frame();
    rig_down(&rig, 20, 20);
    toggle_frame();
    rig_up(&rig, 20, 20);
    toggle_frame();
    TEST_ASSERT_TRUE(on_);
    rig_advance(&rig, 20);
    toggle_frame();
    k = cydui_ease_apply(CYDUI_EASE_OUT_QUAD, 20 * ONE / 200); /* how far along it is at 20 of 200 ms */
    expect_fwd = cydui_blend(off_fill, on_fill, (uint8_t)(k * 255 / ONE));
    expect_back = cydui_blend(on_fill, off_fill, (uint8_t)(k * 255 / ONE));
    TEST_ASSERT_NOT_EQUAL(expect_fwd, expect_back);
    TEST_ASSERT_TRUE(found_anywhere(expect_fwd, 0, 40));
    TEST_ASSERT_FALSE(found_anywhere(expect_back, 0, 40));
}

static int first_accent_x(cydui_color accent)
{
    int x, y;
    for (x = 110; x < 215; x++) { /* the columns of the second of the three tabs */
        for (y = 0; y < 60; y++) {
            if (rig.fb[y * RIG_W + x] == accent) return x;
        }
    }
    return -1;
}

void test_the_bar_of_a_tab_grows_from_its_middle(void)
{
    cydui_color accent;
    int mid_x, settled_x;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    rig.theme.style.anim_ms = 200;
    accent = rig.theme.table[CYDUI_WIDGET_TAB][1][CYDUI_STATE_NORMAL].accent;
    tab_sel = 0;
    tabs_frame();
    tabs_frame();
    TEST_ASSERT_EQUAL_INT(-1, first_accent_x(accent)); /* the second tab has no bar yet */
    rig_down(&rig, 160, 20);
    tabs_frame();
    rig_up(&rig, 160, 20);
    tabs_frame();
    rig_advance(&rig, 60);
    tabs_frame();
    mid_x = first_accent_x(accent);
    rig_advance(&rig, 600);
    tabs_frame();
    settled_x = first_accent_x(accent);
    TEST_ASSERT_TRUE(settled_x > 0);
    TEST_ASSERT_TRUE(mid_x > settled_x + 5); /* still short of the ends */
}

static bool popup_open;

static void popup_frame(void)
{
    cydui_ui_begin(&rig.ui, 0xFFFF);
    if (cydui_popup_begin(&rig.ui, "Sure?", &popup_open, 200, 100, CYDUI_POPUP_MODAL)) {
        cydui_label(&rig.ui, "Delete all");
        cydui_popup_end(&rig.ui);
    }
    cydui_ui_end(&rig.ui);
}

void test_the_dimming_of_a_popup_comes_in_every_time_it_opens(void)
{
    int round;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    rig.theme.style.anim_ms = 200;
    for (round = 0; round < 2; round++) {
        cydui_color bg, first, settled;
        popup_open = false;
        popup_frame();
        bg = rig.fb[2 * RIG_W + 2];
        popup_open = true;
        popup_frame();
        first = rig.fb[2 * RIG_W + 2];
        rig_advance(&rig, 100);
        popup_frame();
        rig_advance(&rig, 500);
        popup_frame();
        settled = rig.fb[2 * RIG_W + 2];
        TEST_ASSERT_EQUAL_HEX16(bg, first);   /* the first frame is not dimmed yet */
        TEST_ASSERT_NOT_EQUAL(bg, settled);   /* and then it is */
        popup_open = false;
        popup_frame();
        rig_advance(&rig, 50);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_every_curve_starts_at_zero_and_ends_at_one);
    RUN_TEST(test_curves_without_a_spring_never_go_back_or_past_the_end);
    RUN_TEST(test_curves_have_their_known_shape);
    RUN_TEST(test_back_and_elastic_overshoot_and_come_back);
    RUN_TEST(test_lerp_is_safe_over_the_whole_range);
    RUN_TEST(test_the_look_of_a_widget_in_transit_is_between_its_two_ends);
    RUN_TEST(test_how_selected_a_widget_is_follows_its_direction);
    RUN_TEST(test_a_tween_starts_at_from_and_follows_the_clock);
    RUN_TEST(test_a_new_target_goes_on_from_where_the_value_is);
    RUN_TEST(test_no_duration_or_a_bad_curve_means_there_at_once);
    RUN_TEST(test_a_duration_is_cut_to_the_maximum);
    RUN_TEST(test_the_clock_wrapping_or_going_back_does_not_break_a_tween);
    RUN_TEST(test_the_elastic_curve_stays_in_a_sane_range_and_lands_on_the_target);
    RUN_TEST(test_the_tweens_that_run_together_are_limited);
    RUN_TEST(test_a_new_ui_over_old_memory_does_not_inherit_the_state_of_the_old_one);
    RUN_TEST(test_a_tween_never_takes_the_place_of_the_state_of_something_else);
    RUN_TEST(test_tweens_that_nobody_asks_for_are_recycled_and_nothing_grows);
    RUN_TEST(test_the_age_of_a_theme_counts_from_the_first_frame_it_is_used);
    RUN_TEST(test_a_switch_slides_over_the_duration_and_lands_on_the_plain_look);
    RUN_TEST(test_a_theme_without_a_duration_changes_at_once);
    RUN_TEST(test_a_tab_changes_its_look_over_the_duration);
    RUN_TEST(test_a_pressed_button_goes_from_its_normal_look_to_the_pressed_one);
    RUN_TEST(test_a_tween_of_the_application_on_the_id_of_a_widget_does_not_fight_the_look_of_the_widget);
    RUN_TEST(test_a_settled_widget_does_not_keep_the_loop_awake_when_the_theme_animates);
    RUN_TEST(test_a_switch_track_blends_from_one_end_look_to_the_other);
    RUN_TEST(test_the_bar_of_a_tab_grows_from_its_middle);
    RUN_TEST(test_the_dimming_of_a_popup_comes_in_every_time_it_opens);
    return UNITY_END();
}
