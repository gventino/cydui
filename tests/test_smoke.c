#include <stdio.h>
#include <string.h>

#include "cydui/cydui.h"

int main(void)
{
    if (strcmp(cydui_version(), "0.0.0") != 0) {
        fprintf(stderr, "unexpected version: %s\n", cydui_version());
        return 1;
    }
    return 0;
}
