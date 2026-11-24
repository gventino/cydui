#include "cydui/arena.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_alloc_is_aligned(void)
{
    uint8_t mem[256];
    cydui_arena a;
    uint8_t *p;
    void *q;

    cydui_arena_init(&a, mem + 1, sizeof(mem) - 1); /* deliberately misaligned base */
    p = (uint8_t *)cydui_arena_alloc(&a, 3, 1);
    TEST_ASSERT_NOT_NULL(p);
    q = cydui_arena_alloc(&a, 8, 16);
    TEST_ASSERT_NOT_NULL(q);
    TEST_ASSERT_EQUAL_UINT(0, (uintptr_t)q % 16);
    TEST_ASSERT_TRUE((uint8_t *)q >= p + 3);
}

static void test_overflow_returns_null_and_changes_nothing(void)
{
    uint8_t mem[32];
    cydui_arena a;
    size_t used;

    cydui_arena_init(&a, mem, sizeof(mem));
    TEST_ASSERT_NOT_NULL(cydui_arena_alloc(&a, 20, 1));
    used = a.used;
    TEST_ASSERT_NULL(cydui_arena_alloc(&a, 13, 1));
    TEST_ASSERT_EQUAL_UINT(used, a.used);
    TEST_ASSERT_NOT_NULL(cydui_arena_alloc(&a, 12, 1)); /* exact fit still works */
}

static void test_huge_size_does_not_wrap(void)
{
    uint8_t mem[32];
    cydui_arena a;

    cydui_arena_init(&a, mem, sizeof(mem));
    TEST_ASSERT_NOT_NULL(cydui_arena_alloc(&a, 8, 1));
    TEST_ASSERT_NULL(cydui_arena_alloc(&a, (size_t)-1, 1));
    TEST_ASSERT_NULL(cydui_arena_alloc(&a, (size_t)-4, 8));
    TEST_ASSERT_EQUAL_UINT(8, a.used);
}

static void test_invalid_arguments(void)
{
    uint8_t mem[32];
    cydui_arena a;

    cydui_arena_init(&a, mem, sizeof(mem));
    TEST_ASSERT_NULL(cydui_arena_alloc(&a, 0, 1));
    TEST_ASSERT_NULL(cydui_arena_alloc(&a, 4, 0));
    TEST_ASSERT_NULL(cydui_arena_alloc(&a, 4, 3)); /* not a power of two */
    TEST_ASSERT_EQUAL_UINT(0, a.used);
}

static void test_reset_and_null_memory(void)
{
    uint8_t mem[16];
    cydui_arena a;

    cydui_arena_init(&a, mem, sizeof(mem));
    TEST_ASSERT_NOT_NULL(cydui_arena_alloc(&a, 16, 1));
    TEST_ASSERT_NULL(cydui_arena_alloc(&a, 1, 1));
    cydui_arena_reset(&a);
    TEST_ASSERT_NOT_NULL(cydui_arena_alloc(&a, 16, 1));

    cydui_arena_init(&a, NULL, 999); /* NULL memory means empty arena */
    TEST_ASSERT_NULL(cydui_arena_alloc(&a, 1, 1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_alloc_is_aligned);
    RUN_TEST(test_overflow_returns_null_and_changes_nothing);
    RUN_TEST(test_huge_size_does_not_wrap);
    RUN_TEST(test_invalid_arguments);
    RUN_TEST(test_reset_and_null_memory);
    return UNITY_END();
}
