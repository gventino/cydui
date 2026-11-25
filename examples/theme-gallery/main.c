/*
 * theme-gallery: the widgets in every theme, with a switch at run time.
 *   theme-gallery                       window; the mouse is the touch screen
 *   theme-gallery --frames N            window, quits by itself after N frames
 *   theme-gallery --headless [--theme 0..4] [--tab 0..5] [--press X,Y [--release]] [--at MS] [--hash] [--png FILE]
 *                                       renders into memory (themes: 0 default, 1 clean, 2 clean_dark, 3 retro, 4 cyberpunk)
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/ui.h"
#include "cydui_mem.h"
#include "gallery.h"
#ifdef CYDUI_HAVE_SDL
#include "cydui_sdl.h"
#endif

#define W 320
#define H 240

CYDUI_STORAGE(storage, W, H, 20, 320, 1536, 48); /* the keyboard on the screen needs about 120 commands */
static cydui_color framebuffer[W * H];

static int init_ui(cydui_ui *ui, const cydui_backend *be, const gallery *g)
{
    if (cydui_ui_init(ui, &CYDUI_CONFIG(storage, be, &g->themes[g->want_theme]))) {
        fprintf(stderr, "cydui_ui_init failed\n");
        return -1;
    }
    return 0;
}

static void one_frame(cydui_ui *ui, gallery *g)
{
    cydui_ui_begin(ui, cydui_theme_of(ui)->style.bg);
    gallery_frame(ui, g);
    cydui_ui_end(ui);
    gallery_apply(ui, g);
}

static int run_headless(int theme, int tab, int px, int py, bool press, bool release, bool print_hash, const char *png, long at_ms)
{
    cydui_mem mem;
    cydui_ui ui;
    static gallery g;
    int frame;

    cydui_mem_init(&mem, framebuffer, W, H);
    gallery_init(&g, theme, tab);
    if (init_ui(&ui, &mem.backend, &g)) {
        return 1;
    }
    for (frame = 0; frame < 6; frame++) {
        if (press && frame == 3) {
            cydui_mem_push_event(&mem, CYDUI_EVENT_DOWN, (int16_t)px, (int16_t)py);
        }
        if (press && release && frame == 4) {
            cydui_mem_push_event(&mem, CYDUI_EVENT_UP, (int16_t)px, (int16_t)py);
        }
        if (frame == 5 && at_ms >= 0) {
            mem.now = (uint32_t)at_ms; /* the last frame at this time since the first one (for the effects that follow the clock) */
        }
        one_frame(&ui, &g);
        mem.now += 16;
    }
    if (cydui_ui_errors(&ui)) {
        fprintf(stderr, "warning: cydui errors 0x%x\n", (unsigned)cydui_ui_errors(&ui));
    }
    if (print_hash) {
        printf("%016" PRIx64 "\n", cydui_mem_hash(&mem));
    }
    if (png && cydui_mem_write_png(&mem, png) != 0) {
        fprintf(stderr, "could not write %s\n", png);
        return 1;
    }
    return 0;
}

#ifdef CYDUI_HAVE_SDL
static int run_window(long max_frames)
{
    cydui_sdl *sdl = cydui_sdl_create("cydui theme gallery", W, H, 2);
    cydui_ui ui;
    static gallery g;
    long frames = 0;

    if (!sdl) {
        fprintf(stderr, "could not open a window\n");
        return 1;
    }
    gallery_init(&g, 1, 0);
    if (init_ui(&ui, cydui_sdl_backend(sdl), &g)) {
        cydui_sdl_destroy(sdl);
        return 1;
    }
    while (max_frames < 0 || frames < max_frames) {
        one_frame(&ui, &g);
        if (ui.in.quit) {
            break;
        }
        frames++;
    }
    cydui_sdl_destroy(sdl);
    return 0;
}
#endif

int main(int argc, char **argv)
{
    bool headless = false, print_hash = false, press = false, release = false;
    const char *png = NULL;
    long frames = -1;
    int theme = 1, tab = 0, px = 0, py = 0, i;
    long at_ms = -1;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless")) {
            headless = true;
        } else if (!strcmp(argv[i], "--hash")) {
            print_hash = true;
        } else if (!strcmp(argv[i], "--release")) {
            release = true;
        } else if (!strcmp(argv[i], "--png") && i + 1 < argc) {
            png = argv[++i];
        } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            frames = atol(argv[++i]);
        } else if (!strcmp(argv[i], "--theme") && i + 1 < argc) {
            theme = atoi(argv[++i]);
            theme = theme < 0 ? 0 : (theme >= GALLERY_THEMES ? GALLERY_THEMES - 1 : theme);
        } else if (!strcmp(argv[i], "--tab") && i + 1 < argc) {
            tab = atoi(argv[++i]);
            tab = tab < 0 ? 0 : (tab >= GALLERY_PAGES ? GALLERY_PAGES - 1 : tab);
        } else if (!strcmp(argv[i], "--at") && i + 1 < argc) {
            at_ms = atol(argv[++i]);
        } else if (!strcmp(argv[i], "--press") && i + 1 < argc) {
            press = sscanf(argv[++i], "%d,%d", &px, &py) == 2;
        } else {
            fprintf(stderr, "usage: %s [--headless [--theme N] [--tab N] [--press X,Y [--release]] [--at MS] [--hash] [--png FILE]] "
                            "[--frames N]\n", argv[0]);
            return 2;
        }
    }
    if (headless) {
        return run_headless(theme, tab, px, py, press, release, print_hash, png, at_ms);
    }
#ifdef CYDUI_HAVE_SDL
    return run_window(frames);
#else
    (void)frames;
    fprintf(stderr, "built without the SDL backend; use --headless\n");
    return 2;
#endif
}
