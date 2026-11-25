/*
 * The touch filter (FR-43, R-04) on streams of conversions made by hand: a clean touch, tremor, a drag,
 * outliers, bouncing contacts, dropouts, hysteresis, jumps. The configuration is spelled out in each test
 * (no reliance on the defaults) so that every number in an expectation can be followed.
 */

#include <string.h>

#include "touch_filter.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static cydui_touch_filter f;
static cydui_event ev;

static cydui_touch_cfg cfg(void)
{
    cydui_touch_cfg c;
    c.z_on = 600;
    c.z_off = 350;
    c.down_ticks = 2;
    c.up_ticks = 3;
    c.move_min = 24;
    c.max_spread = 220;
    c.max_jump = 1200;
    return c;
}

static void start(void)
{
    cydui_touch_cfg c = cfg();
    cydui_touch_filter_init(&f, &c);
}

/* five conversions around (x, y, z): the jitter is -4 +3 0 +5 -2 on x, +2 -3 +4 0 -1 on y and 0 on z */
static int burst(cydui_touch_read *r, int x, int y, int z)
{
    static const int jx[5] = {-4, 3, 0, 5, -2};
    static const int jy[5] = {2, -3, 4, 0, -1};
    int i;
    for (i = 0; i < 5; i++) {
        r[i].x = (uint16_t)(x + jx[i]);
        r[i].y = (uint16_t)(y + jy[i]);
        r[i].z = (uint16_t)z;
    }
    return 5;
}

static bool touch(int x, int y, int z)
{
    cydui_touch_read r[5];
    int n = burst(r, x, y, z);
    return cydui_touch_tick(&f, r, n, true, &ev);
}

static bool release(void)
{
    return cydui_touch_tick(&f, NULL, 0, false, &ev);
}

static void test_a_touch_goes_down_after_two_ticks_stays_quiet_and_goes_up_after_three(void)
{
    int i;

    start();
    TEST_ASSERT_FALSE(touch(2000, 1500, 1500));
    TEST_ASSERT_TRUE(touch(2000, 1500, 1500));
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_DOWN, ev.type);
    TEST_ASSERT_EQUAL_INT(2000, ev.x); /* the mean of x-2, x, x+3 without the extremes: x */
    TEST_ASSERT_EQUAL_INT(1500, ev.y);
    for (i = 0; i < 20; i++) {
        TEST_ASSERT_FALSE(touch(2000, 1500, 1500)); /* a held finger says nothing */
    }
    TEST_ASSERT_FALSE(release());
    TEST_ASSERT_FALSE(release());
    TEST_ASSERT_TRUE(release());
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_UP, ev.type);
    TEST_ASSERT_EQUAL_INT(2000, ev.x); /* where the finger was */
    TEST_ASSERT_EQUAL_INT(1500, ev.y);
    TEST_ASSERT_FALSE(release()); /* and nothing more */
    TEST_ASSERT_FALSE(release());
}

static void test_the_numbers_are_taken_as_they_are(void)
{
    cydui_touch_cfg c = cfg();
    cydui_touch_read r[3];

    c.down_ticks = 1;
    cydui_touch_filter_init(&f, &c);
    r[0] = (cydui_touch_read){1000, 2000, 900};
    r[1] = (cydui_touch_read){1010, 2020, 900};
    r[2] = (cydui_touch_read){1002, 2001, 900};
    TEST_ASSERT_TRUE(cydui_touch_tick(&f, r, 3, true, &ev)); /* 3 reads: the middle one by trimming: 1002, 2001 */
    TEST_ASSERT_EQUAL_INT(1002, ev.x);
    TEST_ASSERT_EQUAL_INT(2001, ev.y);

    cydui_touch_filter_init(&f, &c);
    r[0] = (cydui_touch_read){1000, 2000, 900};
    r[1] = (cydui_touch_read){1011, 2001, 900};
    TEST_ASSERT_TRUE(cydui_touch_tick(&f, r, 2, true, &ev)); /* 2 reads: the plain mean, rounded: 1005.5 -> 1006 */
    TEST_ASSERT_EQUAL_INT(1006, ev.x);
    TEST_ASSERT_EQUAL_INT(2001, ev.y); /* 2000.5 -> 2001 */

    cydui_touch_filter_init(&f, &c);
    TEST_ASSERT_TRUE(cydui_touch_tick(&f, r, 1, true, &ev)); /* one read: itself */
    TEST_ASSERT_EQUAL_INT(1000, ev.x);
}

static void test_a_drag_sends_a_move_for_every_step_over_the_tremor(void)
{
    start();
    touch(1000, 1000, 1500);
    TEST_ASSERT_TRUE(touch(1000, 1000, 1500));
    TEST_ASSERT_FALSE(touch(1010, 995, 1500));  /* tremor of 10 raw units */
    TEST_ASSERT_FALSE(touch(990, 1008, 1500));
    TEST_ASSERT_FALSE(touch(1023, 1000, 1500)); /* 23: under the 24 */
    TEST_ASSERT_TRUE(touch(1024, 1000, 1500));  /* exactly 24: a move */
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_MOVE, ev.type);
    TEST_ASSERT_EQUAL_INT(1024, ev.x);
    TEST_ASSERT_FALSE(touch(1030, 1000, 1500)); /* the reference is now 1024 */
    TEST_ASSERT_TRUE(touch(1024, 1030, 1500));  /* the other axis counts as well */
    TEST_ASSERT_EQUAL_INT(1030, ev.y);
    TEST_ASSERT_TRUE(touch(1400, 1400, 1500));
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_MOVE, ev.type);
    TEST_ASSERT_EQUAL_INT(1400, ev.x);
    TEST_ASSERT_EQUAL_INT(1400, ev.y);
}

static void test_a_slow_drift_is_followed_because_it_is_measured_from_the_last_report(void)
{
    int i, moves = 0;

    start();
    touch(1000, 1000, 1500);
    touch(1000, 1000, 1500);
    for (i = 1; i <= 30; i++) { /* 10 raw units a tick */
        if (touch(1000 + 10 * i, 1000, 1500)) moves++;
    }
    TEST_ASSERT_EQUAL_INT(10, moves); /* 30 raw units pass the 24 at ticks 3, 6, 9 ... 30 */
}

static void test_a_burst_that_spreads_too_far_votes_neither_way(void)
{
    cydui_touch_read r[5];
    int i;

    start();
    touch(2000, 1500, 1500);
    TEST_ASSERT_TRUE(touch(2000, 1500, 1500)); /* down */
    for (i = 0; i < 5; i++) {
        r[i] = (cydui_touch_read){(uint16_t)(2000 + 100 * i), 1500, 1500}; /* a span of 400 > 220 */
    }
    for (i = 0; i < 10; i++) {
        TEST_ASSERT_FALSE(cydui_touch_tick(&f, r, 5, true, &ev)); /* ignored: no move, and not a release either */
    }
    for (i = 0; i < 5; i++) { /* a span of exactly 220 counts: the mean of the middle three is 2230, a move */
        r[i] = (cydui_touch_read){2230, 1500, 1500};
    }
    r[0].x = 2010;
    TEST_ASSERT_TRUE(cydui_touch_tick(&f, r, 5, true, &ev));
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_MOVE, ev.type);
    TEST_ASSERT_EQUAL_INT(2230, ev.x);
    for (i = 0; i < 5; i++) { /* 221 does not: the same burst, one unit wider, is thrown away */
        r[i] = (cydui_touch_read){2530, 1500, 1500};
    }
    r[0].x = 2309;
    TEST_ASSERT_FALSE(cydui_touch_tick(&f, r, 5, true, &ev));
    TEST_ASSERT_FALSE(release());
    TEST_ASSERT_FALSE(release());
    TEST_ASSERT_TRUE(release()); /* the touch was still there and ends by the normal rule */
}

static void test_one_wild_conversion_in_a_burst_does_not_move_the_position(void)
{
    cydui_touch_cfg c = cfg();
    cydui_touch_read r[5];
    int i;

    c.max_spread = 5000; /* let the outlier through to the trimming */
    cydui_touch_filter_init(&f, &c);
    for (i = 0; i < 5; i++) {
        r[i] = (cydui_touch_read){2000, 1500, 1500};
    }
    r[1].x = 3900; /* one wild x */
    r[3].z = 100;  /* and one wild pressure */
    cydui_touch_tick(&f, r, 5, true, &ev);
    TEST_ASSERT_TRUE(cydui_touch_tick(&f, r, 5, true, &ev));
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_DOWN, ev.type);
    TEST_ASSERT_EQUAL_INT(2000, ev.x);
    TEST_ASSERT_EQUAL_INT(1500, ev.y);
}

static void test_a_dropout_shorter_than_the_release_debounce_changes_nothing(void)
{
    start();
    touch(2000, 1500, 1500);
    touch(2000, 1500, 1500); /* down */
    TEST_ASSERT_FALSE(release());
    TEST_ASSERT_FALSE(release()); /* two ticks without a touch: not yet */
    TEST_ASSERT_FALSE(touch(2000, 1500, 1500)); /* and it is back: no UP, no new DOWN */
    TEST_ASSERT_FALSE(release());
    TEST_ASSERT_FALSE(release());
    TEST_ASSERT_FALSE(touch(2000, 1500, 1500)); /* the count started over */
    TEST_ASSERT_FALSE(release());
    TEST_ASSERT_FALSE(release());
    TEST_ASSERT_TRUE(release());
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_UP, ev.type);
}

static void test_a_bouncing_contact_does_not_press(void)
{
    int i;

    start();
    for (i = 0; i < 20; i++) { /* on, off, on, off: never two in a row */
        TEST_ASSERT_FALSE(i % 2 ? release() : touch(2000, 1500, 1500));
    }
    TEST_ASSERT_FALSE(touch(2000, 1500, 1500)); /* now two in a row */
    TEST_ASSERT_TRUE(touch(2000, 1500, 1500));
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_DOWN, ev.type);
}

static void test_the_pressure_has_a_hysteresis(void)
{
    int i;

    start();
    for (i = 0; i < 6; i++) {
        TEST_ASSERT_FALSE(touch(2000, 1500, 599)); /* under z_on: never starts */
    }
    touch(2000, 1500, 600); /* exactly z_on counts */
    TEST_ASSERT_TRUE(touch(2000, 1500, 600));
    for (i = 0; i < 6; i++) {
        TEST_ASSERT_FALSE(touch(2000, 1500, 350)); /* exactly z_off: still down */
    }
    TEST_ASSERT_FALSE(touch(2000, 1500, 349));
    TEST_ASSERT_FALSE(touch(2000, 1500, 349));
    TEST_ASSERT_TRUE(touch(2000, 1500, 349)); /* three ticks under z_off: up */
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_UP, ev.type);
}

static void test_a_single_tick_far_away_is_a_glitch_and_two_that_agree_are_a_move(void)
{
    start();
    touch(1000, 1000, 1500);
    touch(1000, 1000, 1500);
    TEST_ASSERT_FALSE(touch(2300, 1000, 1500)); /* 1300 away: waits for confirmation */
    TEST_ASSERT_FALSE(touch(1000, 1000, 1500)); /* and it was not confirmed: nothing happened */
    TEST_ASSERT_FALSE(touch(1000, 1000, 1500));
    TEST_ASSERT_FALSE(touch(2300, 1000, 1500));
    TEST_ASSERT_TRUE(touch(2300, 1000, 1500)); /* two in a row: it is a move, to the new place */
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_MOVE, ev.type);
    TEST_ASSERT_EQUAL_INT(2300, ev.x);

    start();
    touch(1000, 1000, 1500);
    touch(1000, 1000, 1500);
    TEST_ASSERT_FALSE(touch(2300, 1000, 1500));
    TEST_ASSERT_FALSE(touch(3600, 1000, 1500)); /* another far place: the candidate is replaced, not confirmed */
    TEST_ASSERT_FALSE(touch(1000, 1000, 1500));
    TEST_ASSERT_FALSE(touch(1000, 1000, 1500)); /* a jump of exactly max_jump is not a jump */
    TEST_ASSERT_TRUE(touch(2200, 1000, 1500));
    TEST_ASSERT_EQUAL_INT(2200, ev.x);
}

static void test_the_latest_steady_sample_is_there_for_a_calibration_to_average(void)
{
    cydui_touch_read r[5];
    int i;

    start();
    TEST_ASSERT_EQUAL_UINT32(0, f.samples);
    touch(2000, 1500, 1200);
    TEST_ASSERT_EQUAL_UINT32(1, f.samples);
    TEST_ASSERT_EQUAL_UINT16(2000, f.sample_x);
    TEST_ASSERT_EQUAL_UINT16(1500, f.sample_y);
    TEST_ASSERT_EQUAL_UINT16(1200, f.sample_z);
    touch(2100, 1400, 1300);
    TEST_ASSERT_EQUAL_UINT32(2, f.samples);
    TEST_ASSERT_EQUAL_UINT16(2100, f.sample_x);
    release(); /* no touch: nothing new */
    TEST_ASSERT_EQUAL_UINT32(2, f.samples);
    TEST_ASSERT_EQUAL_UINT16(2100, f.sample_x);
    for (i = 0; i < 5; i++) {
        r[i] = (cydui_touch_read){(uint16_t)(1000 + 300 * i), 1500, 1500}; /* a bouncing burst: thrown away */
    }
    cydui_touch_tick(&f, r, 5, true, &ev);
    TEST_ASSERT_EQUAL_UINT32(2, f.samples);
    TEST_ASSERT_EQUAL_UINT16(2100, f.sample_x);
    cydui_touch_tick(&f, r, 5, false, &ev); /* the pen interrupt says no: nothing either */
    TEST_ASSERT_EQUAL_UINT32(2, f.samples);
}

static void test_it_survives_odd_calls(void)
{
    cydui_touch_read many[20];
    int i;

    start();
    for (i = 0; i < 20; i++) many[i] = (cydui_touch_read){2000, 1500, 1500};
    TEST_ASSERT_FALSE(cydui_touch_tick(&f, NULL, 5, true, &ev));    /* no reads at all */
    TEST_ASSERT_FALSE(cydui_touch_tick(&f, many, 0, true, &ev));    /* none counted */
    TEST_ASSERT_FALSE(cydui_touch_tick(&f, many, -3, true, &ev));
    TEST_ASSERT_FALSE(cydui_touch_tick(&f, many, 20, true, &ev));   /* only the first nine are looked at */
    TEST_ASSERT_TRUE(cydui_touch_tick(&f, many, 20, true, &ev));
    TEST_ASSERT_EQUAL_INT(2000, ev.x);
    TEST_ASSERT_FALSE(cydui_touch_tick(&f, many, 5, false, &ev));   /* the pen interrupt says "not touched" */
}

static void test_the_configuration_is_made_safe(void)
{
    cydui_touch_cfg c = cfg();

    c.down_ticks = 0;
    c.up_ticks = 0;
    c.z_off = 900; /* above z_on */
    cydui_touch_filter_init(&f, &c);
    TEST_ASSERT_TRUE(touch(2000, 1500, 600)); /* one tick is enough to go down */
    TEST_ASSERT_TRUE(f.cfg.z_off <= f.cfg.z_on);
    TEST_ASSERT_TRUE(release()); /* and one to go up */
    TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_UP, ev.type);

    c = cydui_touch_cfg_default();
    TEST_ASSERT_TRUE(c.z_off <= c.z_on);
    TEST_ASSERT_TRUE(c.down_ticks >= 1 && c.up_ticks >= 1);
    TEST_ASSERT_TRUE(c.move_min > 0 && c.max_spread > c.move_min && c.max_jump > c.max_spread);
}

static void test_touches_follow_each_other(void)
{
    int round, i;

    start();
    for (round = 0; round < 3; round++) {
        touch(500 * (round + 1), 700, 1500);
        TEST_ASSERT_TRUE(touch(500 * (round + 1), 700, 1500));
        TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_DOWN, ev.type);
        TEST_ASSERT_EQUAL_INT(500 * (round + 1), ev.x);
        for (i = 0; i < 2; i++) release();
        TEST_ASSERT_TRUE(release());
        TEST_ASSERT_EQUAL_INT(CYDUI_EVENT_UP, ev.type);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_touch_goes_down_after_two_ticks_stays_quiet_and_goes_up_after_three);
    RUN_TEST(test_the_numbers_are_taken_as_they_are);
    RUN_TEST(test_a_drag_sends_a_move_for_every_step_over_the_tremor);
    RUN_TEST(test_a_slow_drift_is_followed_because_it_is_measured_from_the_last_report);
    RUN_TEST(test_a_burst_that_spreads_too_far_votes_neither_way);
    RUN_TEST(test_one_wild_conversion_in_a_burst_does_not_move_the_position);
    RUN_TEST(test_a_dropout_shorter_than_the_release_debounce_changes_nothing);
    RUN_TEST(test_a_bouncing_contact_does_not_press);
    RUN_TEST(test_the_pressure_has_a_hysteresis);
    RUN_TEST(test_a_single_tick_far_away_is_a_glitch_and_two_that_agree_are_a_move);
    RUN_TEST(test_the_latest_steady_sample_is_there_for_a_calibration_to_average);
    RUN_TEST(test_it_survives_odd_calls);
    RUN_TEST(test_the_configuration_is_made_safe);
    RUN_TEST(test_touches_follow_each_other);
    return UNITY_END();
}
