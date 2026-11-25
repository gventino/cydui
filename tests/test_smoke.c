#include <stdio.h>
#include <string.h>

#include "cydui/cydui.h"

int main(void)
{
    if (strcmp(cydui_version(), CYDUI_VERSION_STRING) != 0 || strcmp(CYDUI_VERSION_STRING, "0.7.0") != 0) {
        fprintf(stderr, "unexpected version: %s\n", cydui_version());
        return 1;
    }
    return 0;
}
