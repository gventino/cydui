/* Widget ids, collision detection, the state table (FR-21, FR-22, CT-07) and the thread check (CT-11). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uirig.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uirig rig;

static cydui_id id_of(const char *label)
{
    return cydui_id_str(&rig.ui, label);
}

static void begin(void)
{
    TEST_ASSERT_TRUE(cydui_ui_begin(&rig.ui, 0));
}

static void test_init_validates_its_inputs(void)
{
    cydui_ui_config cfg;
    cydui_ui ui;

    memset(&cfg, 0, sizeof(cfg));
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_ui_init(&ui, NULL));
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_ui_init(&ui, &cfg)); /* no style */

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));

    rig.style.font = NULL; /* a style without a usable font is refused, not trusted */
    memset(&cfg, 0, sizeof(cfg));
    cfg.core.backend = &rig.mem.backend;
    cfg.core.cmd_mem = rig.cmds;
    cfg.core.cmd_mem_size = sizeof(rig.cmds);
    cfg.core.strip_mem = rig.strip;
    cfg.core.strip_mem_size = sizeof(rig.strip);
    cfg.style = &rig.style;
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_BAD_ARG, cydui_ui_init(&ui, &cfg));
}

static void test_ids_are_deterministic_and_distinct(void)
{
    cydui_id a, b;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    a = id_of("Add");
    b = id_of("Add");
    TEST_ASSERT_TRUE(cydui_id_eq(a, b));
    TEST_ASSERT_FALSE(cydui_id_eq(a, id_of("Remove")));
    TEST_ASSERT_FALSE(cydui_id_eq(a, id_of("add"))); /* case matters */
    TEST_ASSERT_FALSE(cydui_id_eq(a, id_of("Add "))); /* so does a trailing space */
    TEST_ASSERT_FALSE(cydui_id_eq(id_of(""), id_of(" ")));
    TEST_ASSERT_FALSE(cydui_id_eq(id_of("x"), id_of(NULL)) && id_of(NULL).h == 0);
    cydui_ui_end(&rig.ui);

    /* the same label gives the same id in every frame and in a different rig */
    begin();
    TEST_ASSERT_TRUE(cydui_id_eq(a, id_of("Add")));
    cydui_ui_end(&rig.ui);
    {
        static uirig other;
        TEST_ASSERT_EQUAL_UINT32(0, rig_init(&other));
        cydui_ui_begin(&other.ui, 0);
        TEST_ASSERT_TRUE(cydui_id_eq(a, cydui_id_str(&other.ui, "Add")));
    }
}

static void test_known_id_values_do_not_drift(void)
{
    /* FNV-1a (32 bit) of the label, seeded with the offset basis; computed independently in Python.
     * Pinned so that ids, and the state stored under them, keep matching across versions. */
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    TEST_ASSERT_EQUAL_HEX32(0x9DC3AA14u, id_of("Add").h);
    TEST_ASSERT_EQUAL_HEX32(0x811C9DC5u, id_of("").h);
    TEST_ASSERT_EQUAL_HEX32(0xD303E674u, id_of("Done##3").h);

    /* The 16-bit check is a different function (sdbm style), so it must differ per label and not
     * just follow the hash. Computed independently as well. */
    TEST_ASSERT_EQUAL_HEX16(0xCBCF, id_of("Add").c);
    TEST_ASSERT_EQUAL_HEX16(0x001D, id_of("").c);
    TEST_ASSERT_EQUAL_HEX16(0x69B2, id_of("Done##3").c);
    TEST_ASSERT_TRUE(id_of("Done##3").c != id_of("Done##4").c);
}

static void test_push_id_scopes_the_labels_inside(void)
{
    cydui_id plain, in1, in2, nested;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    plain = id_of("Done");
    cydui_push_id_int(&rig.ui, 1);
    in1 = id_of("Done");
    cydui_pop_id(&rig.ui);
    cydui_push_id_int(&rig.ui, 2);
    in2 = id_of("Done");
    cydui_push_id_str(&rig.ui, "inner");
    nested = id_of("Done");
    cydui_pop_id(&rig.ui);
    cydui_pop_id(&rig.ui);

    TEST_ASSERT_FALSE(cydui_id_eq(plain, in1));
    TEST_ASSERT_FALSE(cydui_id_eq(in1, in2));
    TEST_ASSERT_FALSE(cydui_id_eq(in2, nested));
    TEST_ASSERT_TRUE(cydui_id_eq(plain, id_of("Done"))); /* popped: back to the plain id */
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));

    /* push_ptr: equal pointers give equal ids, different ones differ */
    {
        int x = 1, y = 2;
        cydui_id px1, px2, py;
        cydui_push_id_ptr(&rig.ui, &x);
        px1 = id_of("Done");
        cydui_pop_id(&rig.ui);
        cydui_push_id_ptr(&rig.ui, &x);
        px2 = id_of("Done");
        cydui_pop_id(&rig.ui);
        cydui_push_id_ptr(&rig.ui, &y);
        py = id_of("Done");
        cydui_pop_id(&rig.ui);
        TEST_ASSERT_TRUE(cydui_id_eq(px1, px2));
        TEST_ASSERT_FALSE(cydui_id_eq(px1, py));
    }
}

static void test_push_id_int_depends_on_the_value_not_the_machine(void)
{
    cydui_id a, b, c;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    cydui_push_id_int(&rig.ui, 0x01020304);
    a = id_of("x");
    cydui_pop_id(&rig.ui);
    cydui_push_id_int(&rig.ui, 0x04030201);
    b = id_of("x");
    cydui_pop_id(&rig.ui);
    cydui_push_id_int(&rig.ui, -1);
    c = id_of("x");
    cydui_pop_id(&rig.ui);
    TEST_ASSERT_FALSE(cydui_id_eq(a, b));
    TEST_ASSERT_FALSE(cydui_id_eq(a, c));
    TEST_ASSERT_FALSE(cydui_id_eq(b, c));

    /* every byte of the value counts: values that differ in one byte only */
    {
        static const int32_t v[] = {0, 0x100, 0x200, 0x10000, 0x20000, 0x1000000, 0x2000000};
        size_t i, k;
        for (i = 0; i < sizeof(v) / sizeof(v[0]); i++) {
            cydui_id ids[8];
            cydui_push_id_int(&rig.ui, v[i]);
            ids[i] = id_of("x");
            cydui_pop_id(&rig.ui);
            for (k = 0; k < i; k++) {
                cydui_push_id_int(&rig.ui, v[k]);
                TEST_ASSERT_FALSE_MESSAGE(cydui_id_eq(ids[i], id_of("x")), "ids ignore a byte of the value");
                cydui_pop_id(&rig.ui);
            }
        }
    }
}

static void test_hash_suffix_separates_widgets_that_read_the_same(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    TEST_ASSERT_FALSE(cydui_id_eq(id_of("Done##3"), id_of("Done##4")));
    TEST_ASSERT_FALSE(cydui_id_eq(id_of("Done##3"), id_of("Done")));
    TEST_ASSERT_TRUE(cydui_id_eq(id_of("Done##3"), id_of("Done##3")));
}

static void test_id_stack_is_bounded(void)
{
    int i;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    for (i = 0; i < CYDUI_MAX_ID_DEPTH; i++) {
        cydui_push_id_int(&rig.ui, i);
    }
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    cydui_push_id_int(&rig.ui, 99); /* one too many: ignored, flagged, nothing breaks */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_LAYOUT, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_EQUAL_UINT8(CYDUI_MAX_ID_DEPTH, rig.ui.id_depth);
    cydui_ui_clear_errors(&rig.ui);

    for (i = 0; i < CYDUI_MAX_ID_DEPTH; i++) {
        cydui_pop_id(&rig.ui);
    }
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    cydui_pop_id(&rig.ui); /* one too many: ignored and flagged */
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_LAYOUT, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_EQUAL_UINT8(0, rig.ui.id_depth);
}

static void test_unbalanced_push_is_dropped_at_the_end_of_the_frame(void)
{
    cydui_id before;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    before = id_of("x");
    cydui_push_id_int(&rig.ui, 7);
    cydui_ui_end(&rig.ui); /* the push was never popped */
    begin();
    TEST_ASSERT_TRUE(cydui_id_eq(before, id_of("x")));
}

static void test_long_labels_are_bounded(void)
{
    static char big[CYDUI_MAX_STR * 4];
    char *exact = (char *)malloc(CYDUI_MAX_STR); /* no NUL: reading beyond would be out of bounds */
    cydui_id a, b;

    TEST_ASSERT_NOT_NULL(exact);
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    memset(exact, 'x', CYDUI_MAX_STR);
    a = id_of(exact);
    memset(big, 'x', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';
    b = id_of(big); /* the same first CYDUI_MAX_STR bytes: the same id, by design */
    TEST_ASSERT_TRUE(cydui_id_eq(a, b));
    free(exact);
}

/* ---- collisions (CT-07) ---- */

static void test_second_use_of_an_id_in_a_frame_is_flagged(void)
{
    cydui_id a;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    a = id_of("Done");
    TEST_ASSERT_TRUE(cydui_id_claim(&rig.ui, a));
    TEST_ASSERT_TRUE(cydui_id_claim(&rig.ui, id_of("Other")));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
    TEST_ASSERT_FALSE(cydui_id_claim(&rig.ui, a));
    TEST_ASSERT_EQUAL_UINT32(CYDUI_ERR_ID_COLLISION, cydui_ui_errors(&rig.ui));
    cydui_ui_end(&rig.ui);

    /* a new frame starts with an empty set */
    cydui_ui_clear_errors(&rig.ui);
    begin();
    TEST_ASSERT_TRUE(cydui_id_claim(&rig.ui, a));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_push_id_avoids_the_collision(void)
{
    cydui_id a;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    cydui_push_id_int(&rig.ui, 1);
    a = id_of("Done");
    TEST_ASSERT_TRUE(cydui_id_claim(&rig.ui, a));
    cydui_pop_id(&rig.ui);
    cydui_push_id_int(&rig.ui, 2);
    TEST_ASSERT_TRUE(cydui_id_claim(&rig.ui, id_of("Done")));
    cydui_pop_id(&rig.ui);
    TEST_ASSERT_TRUE(cydui_id_claim(&rig.ui, id_of("Done##x")));
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui));
}

static void test_a_full_collision_set_stays_quiet(void)
{
    int i;
    char label[16];

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    for (i = 0; i < 2000; i++) { /* far more ids than the set (1024 slots) can hold */
        snprintf(label, sizeof(label), "w%d", i);
        cydui_id_claim(&rig.ui, id_of(label));
    }
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui)); /* no false collision reports */
}

/* ---- the state table (FR-22) ---- */

static void test_state_is_created_found_and_zeroed(void)
{
    cydui_state *s, *again;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    TEST_ASSERT_NULL(cydui_state_get(&rig.ui, id_of("a"), false)); /* not there, not created */
    s = cydui_state_get(&rig.ui, id_of("a"), true);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_INT32(0, s->a);
    TEST_ASSERT_EQUAL_INT32(0, s->b);
    s->a = 42;
    s->b = -7;
    cydui_ui_end(&rig.ui);

    begin(); /* it survives to the next frame */
    again = cydui_state_get(&rig.ui, id_of("a"), false);
    TEST_ASSERT_TRUE(again == s);
    TEST_ASSERT_EQUAL_INT32(42, again->a);
    TEST_ASSERT_EQUAL_INT32(-7, again->b);
    TEST_ASSERT_TRUE(cydui_state_get(&rig.ui, id_of("a"), true) == s); /* create finds it too */
}

static void test_ids_with_equal_hash_but_different_check_are_separate(void)
{
    cydui_id x, y;
    cydui_state *sx, *sy;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    begin();
    x = id_of("same hash");
    y = x;
    y.c = (uint16_t)(x.c + 1u); /* a forced collision of the 32-bit hash */
    sx = cydui_state_get(&rig.ui, x, true);
    sy = cydui_state_get(&rig.ui, y, true);
    TEST_ASSERT_NOT_NULL(sx);
    TEST_ASSERT_NOT_NULL(sy);
    TEST_ASSERT_TRUE(sx != sy);
    sx->a = 1;
    sy->a = 2;
    TEST_ASSERT_EQUAL_INT32(1, cydui_state_get(&rig.ui, x, false)->a);
    TEST_ASSERT_EQUAL_INT32(2, cydui_state_get(&rig.ui, y, false)->a);
}

static void test_unseen_state_is_recycled_after_the_ttl(void)
{
    int i;
    cydui_state *s;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&rig, 8, 0));
    begin();
    s = cydui_state_get(&rig.ui, id_of("gone"), true);
    TEST_ASSERT_NOT_NULL(s);
    s->a = 5;
    cydui_ui_end(&rig.ui);

    for (i = 0; i < CYDUI_STATE_TTL - 2; i++) { /* still within the time to live */
        rig_empty_frame(&rig);
    }
    begin();
    TEST_ASSERT_NOT_NULL_MESSAGE(cydui_state_get(&rig.ui, id_of("gone"), false),
                                 "recycled too early");
    cydui_ui_end(&rig.ui);

    for (i = 0; i < CYDUI_STATE_TTL + 20; i++) { /* now it is not looked up for longer than the ttl */
        rig_empty_frame(&rig);
    }
    begin();
    TEST_ASSERT_NULL_MESSAGE(cydui_state_get(&rig.ui, id_of("gone"), false), "never recycled");
}

static void test_state_that_is_used_is_never_recycled(void)
{
    int i;
    cydui_state *s;

    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&rig, 8, 0));
    begin();
    s = cydui_state_get(&rig.ui, id_of("busy"), true);
    s->a = 9;
    cydui_ui_end(&rig.ui);
    for (i = 0; i < CYDUI_STATE_TTL * 3; i++) {
        begin();
        TEST_ASSERT_NOT_NULL(cydui_state_get(&rig.ui, id_of("busy"), false)); /* seen every frame */
        cydui_ui_end(&rig.ui);
    }
    begin();
    TEST_ASSERT_EQUAL_INT32(9, cydui_state_get(&rig.ui, id_of("busy"), false)->a);
}

static void test_full_table_replaces_the_oldest_and_not_the_active_one(void)
{
    cydui_state *s[8], *extra;
    int i;
    char label[8];

    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&rig, 4, 0));
    TEST_ASSERT_EQUAL_UINT32(4, rig.ui.state_cap);
    for (i = 0; i < 4; i++) {
        begin();
        snprintf(label, sizeof(label), "e%d", i);
        s[i] = cydui_state_get(&rig.ui, id_of(label), true);
        TEST_ASSERT_NOT_NULL(s[i]);
        s[i]->a = i + 1;
        cydui_ui_end(&rig.ui);
    }
    begin();
    cydui_state_get(&rig.ui, id_of("e0"), false); /* touching e0 makes e1 the oldest */
    extra = cydui_state_get(&rig.ui, id_of("new"), true);
    TEST_ASSERT_NOT_NULL(extra);
    TEST_ASSERT_TRUE_MESSAGE(extra == s[1], "did not replace the oldest entry");
    TEST_ASSERT_NULL(cydui_state_get(&rig.ui, id_of("e1"), false));
    TEST_ASSERT_NOT_NULL(cydui_state_get(&rig.ui, id_of("e0"), false));
    TEST_ASSERT_NOT_NULL(cydui_state_get(&rig.ui, id_of("e2"), false));
    TEST_ASSERT_NOT_NULL(cydui_state_get(&rig.ui, id_of("e3"), false));

    /* the active widget is protected even when it is the oldest */
    rig.ui.active = id_of("e2");
    cydui_ui_end(&rig.ui);
    begin();
    cydui_state_get(&rig.ui, id_of("e0"), false);
    cydui_state_get(&rig.ui, id_of("e3"), false);
    cydui_state_get(&rig.ui, id_of("new"), false);
    extra = cydui_state_get(&rig.ui, id_of("another"), true);
    TEST_ASSERT_NOT_NULL(extra);
    TEST_ASSERT_NOT_NULL_MESSAGE(cydui_state_get(&rig.ui, id_of("e2"), false), "evicted the active widget");
}

static void test_a_table_without_memory_degrades_to_no_state(void)
{
    TEST_ASSERT_EQUAL_UINT32(0, rig_init_with(&rig, 0, 0));
    begin();
    TEST_ASSERT_NULL(cydui_state_get(&rig.ui, id_of("a"), true));
    TEST_ASSERT_NULL(cydui_state_get(&rig.ui, id_of("a"), false));
    cydui_ui_end(&rig.ui); /* and nothing else minds */
}

/* ---- thread check (CT-11) ---- */

static uintptr_t fake_thread;
static uintptr_t current_thread(void)
{
    return fake_thread;
}

static void test_a_call_from_another_task_is_flagged(void)
{
    cydui_ui_config cfg;

    fake_thread = 1;
    TEST_ASSERT_EQUAL_UINT32(0, rig_init(&rig));
    memset(&cfg, 0, sizeof(cfg));
    cfg.core.backend = &rig.mem.backend;
    cfg.core.cmd_mem = rig.cmds;
    cfg.core.cmd_mem_size = sizeof(rig.cmds);
    cfg.core.strip_mem = rig.strip;
    cfg.core.strip_mem_size = sizeof(rig.strip);
    cfg.style = &rig.style;
    cfg.thread_id = current_thread;
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_init(&rig.ui, &cfg));

    begin();
    cydui_ui_end(&rig.ui);
    TEST_ASSERT_EQUAL_UINT32(0, cydui_ui_errors(&rig.ui)); /* the creating task: fine */

    fake_thread = 2;
    begin();
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_THREAD);
    cydui_ui_clear_errors(&rig.ui);
    cydui_push_id_int(&rig.ui, 1);
    TEST_ASSERT_TRUE(cydui_ui_errors(&rig.ui) & CYDUI_ERR_THREAD);
    cydui_ui_end(&rig.ui);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_validates_its_inputs);
    RUN_TEST(test_ids_are_deterministic_and_distinct);
    RUN_TEST(test_known_id_values_do_not_drift);
    RUN_TEST(test_push_id_scopes_the_labels_inside);
    RUN_TEST(test_push_id_int_depends_on_the_value_not_the_machine);
    RUN_TEST(test_hash_suffix_separates_widgets_that_read_the_same);
    RUN_TEST(test_id_stack_is_bounded);
    RUN_TEST(test_unbalanced_push_is_dropped_at_the_end_of_the_frame);
    RUN_TEST(test_long_labels_are_bounded);
    RUN_TEST(test_second_use_of_an_id_in_a_frame_is_flagged);
    RUN_TEST(test_push_id_avoids_the_collision);
    RUN_TEST(test_a_full_collision_set_stays_quiet);
    RUN_TEST(test_state_is_created_found_and_zeroed);
    RUN_TEST(test_ids_with_equal_hash_but_different_check_are_separate);
    RUN_TEST(test_unseen_state_is_recycled_after_the_ttl);
    RUN_TEST(test_state_that_is_used_is_never_recycled);
    RUN_TEST(test_full_table_replaces_the_oldest_and_not_the_active_one);
    RUN_TEST(test_a_table_without_memory_degrades_to_no_state);
    RUN_TEST(test_a_call_from_another_task_is_flagged);
    return UNITY_END();
}
