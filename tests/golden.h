#ifndef GOLDEN_H
#define GOLDEN_H

#include "testutil.h"

/*
 * Golden-image check. Renders `scene` and compares the framebuffer hash with
 * tests/golden/<name>.hash. Always writes <build>/golden/<name>.png so the image can be looked at.
 *
 * CYDUI_UPDATE_GOLDEN=1 in the environment writes the hash file instead of comparing; a missing
 * hash file otherwise fails the test.
 */
void golden_check(const char *name, tu_scene scene);

#endif /* GOLDEN_H */
