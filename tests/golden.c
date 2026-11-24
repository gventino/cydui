#include "golden.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "unity.h"

#ifndef GOLDEN_DIR
#error "GOLDEN_DIR must point at tests/golden"
#endif
#ifndef GOLDEN_OUT_DIR
#error "GOLDEN_OUT_DIR must point at the build output directory for PNGs"
#endif

void golden_check_mem(const char *name, const cydui_mem *mem, uint32_t errors)
{
    char path[512], msg[256];
    uint64_t hash = cydui_mem_hash(mem);
    FILE *f;
    unsigned long long want = 0;

    snprintf(path, sizeof(path), "%s/%s.png", GOLDEN_OUT_DIR, name);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, cydui_mem_write_png(mem, path), "could not write the PNG");

    /* "text was cut" is informative; any other flag means the scene did something wrong. */
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, errors & ~(uint32_t)CYDUI_ERR_TEXT_TRUNCATED,
                                     "the scene raised library errors");

    snprintf(path, sizeof(path), "%s/%s.hash", GOLDEN_DIR, name);
    if (getenv("CYDUI_UPDATE_GOLDEN")) {
        f = fopen(path, "w");
        TEST_ASSERT_NOT_NULL_MESSAGE(f, "could not write the golden hash");
        fprintf(f, "%016" PRIx64 "\n", hash);
        fclose(f);
        return;
    }

    f = fopen(path, "r");
    snprintf(msg, sizeof(msg), "no golden hash for '%s' (run with CYDUI_UPDATE_GOLDEN=1 after "
                               "checking build/golden/%s.png)", name, name);
    TEST_ASSERT_NOT_NULL_MESSAGE(f, msg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, fscanf(f, "%llx", &want), "malformed golden hash file");
    fclose(f);

    snprintf(msg, sizeof(msg), "'%s': got %016" PRIx64 ", golden %016llx (see build/golden/%s.png)",
             name, hash, want, name);
    TEST_ASSERT_TRUE_MESSAGE(hash == (uint64_t)want, msg);
}

void golden_check(const char *name, tu_scene scene)
{
    cydui_mem mem;

    (void)tu_render(scene, TU_H, 0);
    cydui_mem_init(&mem, tu_fb, TU_W, TU_H);
    golden_check_mem(name, &mem, tu_errors);
}
