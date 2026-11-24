#include <stdio.h>
#include "cydui_sans_16.h"
#include "cydui_sdl.h"
#include "cydui/ui.h"

CYDUI_STORAGE(mem, 320, 240, 20, 64, 512, 16);

int main(void)
{
    cydui_sdl *sdl = cydui_sdl_create("hello", 320, 240, 2);
    cydui_style style = cydui_style_default(&cydui_sans_16);
    cydui_ui ui;
    char msg[32];
    int clicks = 0;

    if (!sdl || cydui_ui_init(&ui, &CYDUI_CONFIG(mem, cydui_sdl_backend(sdl), &style))) return 1;
    while (cydui_ui_begin(&ui, style.bg)) {
        snprintf(msg, sizeof msg, "clicks: %d", clicks);
        cydui_label(&ui, msg);
        if (cydui_button(&ui, "Click me")) clicks++;
        cydui_ui_end(&ui);
    }
    cydui_sdl_destroy(sdl);
    return 0;
}
