/*
 * The latency ring and its percentiles (NFR-02). The expectations are worked out by hand with the nearest-rank
 * rule: the p-th percentile of n sorted values is the one at position ceil(p * n / 100).
 */

#include "latency.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static cydui_lat l;
static cydui_lat_stats st;

static void test_nothing_gives_zeros(void)
{
    cydui_lat_init(&l);
    cydui_lat_stats_get(&l, &st);
    TEST_ASSERT_EQUAL_INT(0, st.n);
    TEST_ASSERT_EQUAL_UINT32(0, st.p50);
    TEST_ASSERT_EQUAL_UINT32(0, st.p95);
    TEST_ASSERT_EQUAL_UINT32(0, st.max);
}

static void test_one_sample_is_every_percentile(void)
{
    cydui_lat_init(&l);
    cydui_lat_add(&l, 4321);
    cydui_lat_stats_get(&l, &st);
    TEST_ASSERT_EQUAL_INT(1, st.n);
    TEST_ASSERT_EQUAL_UINT32(4321, st.p50);
    TEST_ASSERT_EQUAL_UINT32(4321, st.p95);
    TEST_ASSERT_EQUAL_UINT32(4321, st.max);
}

static void test_the_percentiles_of_one_to_a_hundred(void)
{
    int i;

    cydui_lat_init(&l);
    for (i = 100; i >= 1; i--) { /* added in descending order: the sort has to work */
        cydui_lat_add(&l, (uint32_t)i);
    }
    cydui_lat_stats_get(&l, &st);
    TEST_ASSERT_EQUAL_INT(100, st.n);
    TEST_ASSERT_EQUAL_UINT32(50, st.p50); /* ceil(0.50 * 100) = 50th */
    TEST_ASSERT_EQUAL_UINT32(95, st.p95); /* ceil(0.95 * 100) = 95th */
    TEST_ASSERT_EQUAL_UINT32(100, st.max);
}

static void test_the_rank_is_rounded_up(void)
{
    static const uint32_t v[10] = {70, 10, 90, 30, 50, 20, 100, 60, 40, 80};
    int i;

    cydui_lat_init(&l);
    for (i = 0; i < 10; i++) cydui_lat_add(&l, v[i]);
    cydui_lat_stats_get(&l, &st);
    TEST_ASSERT_EQUAL_UINT32(50, st.p50);  /* ceil(5.0) = 5th of 10..100 */
    TEST_ASSERT_EQUAL_UINT32(100, st.p95); /* ceil(9.5) = 10th: the largest */

    cydui_lat_init(&l);
    for (i = 0; i < 3; i++) cydui_lat_add(&l, v[i]); /* 70 10 90 -> 10 70 90 */
    cydui_lat_stats_get(&l, &st);
    TEST_ASSERT_EQUAL_UINT32(70, st.p50);  /* ceil(1.5) = 2nd */
    TEST_ASSERT_EQUAL_UINT32(90, st.p95);  /* ceil(2.85) = 3rd */

    cydui_lat_init(&l);
    for (i = 0; i < 20; i++) cydui_lat_add(&l, (uint32_t)(i + 1));
    cydui_lat_stats_get(&l, &st);
    TEST_ASSERT_EQUAL_UINT32(10, st.p50); /* ceil(10.0) = 10th */
    TEST_ASSERT_EQUAL_UINT32(19, st.p95); /* ceil(19.0) = 19th, not the 20th */
}

static void test_duplicates_are_fine(void)
{
    int i;

    cydui_lat_init(&l);
    for (i = 0; i < 40; i++) cydui_lat_add(&l, 25000);
    cydui_lat_add(&l, 90000);
    cydui_lat_stats_get(&l, &st);
    TEST_ASSERT_EQUAL_UINT32(25000, st.p50);
    TEST_ASSERT_EQUAL_UINT32(25000, st.p95); /* ceil(0.95 * 41) = 39th */
    TEST_ASSERT_EQUAL_UINT32(90000, st.max);
}

static void test_the_ring_keeps_the_last_128(void)
{
    int i;

    cydui_lat_init(&l);
    for (i = 1; i <= 200; i++) cydui_lat_add(&l, (uint32_t)i); /* 73..200 remain */
    cydui_lat_stats_get(&l, &st);
    TEST_ASSERT_EQUAL_INT(CYDUI_LAT_MAX, st.n);
    TEST_ASSERT_EQUAL_UINT32(200, st.max);
    TEST_ASSERT_EQUAL_UINT32(73 + 63, st.p50);   /* the 64th of 73..200 */
    TEST_ASSERT_EQUAL_UINT32(73 + 121, st.p95);  /* ceil(0.95 * 128) = 122nd */
    for (i = 0; i < 128; i++) cydui_lat_add(&l, 5); /* now only fives */
    cydui_lat_stats_get(&l, &st);
    TEST_ASSERT_EQUAL_UINT32(5, st.max);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_nothing_gives_zeros);
    RUN_TEST(test_one_sample_is_every_percentile);
    RUN_TEST(test_the_percentiles_of_one_to_a_hundred);
    RUN_TEST(test_the_rank_is_rounded_up);
    RUN_TEST(test_duplicates_are_fine);
    RUN_TEST(test_the_ring_keeps_the_last_128);
    return UNITY_END();
}
