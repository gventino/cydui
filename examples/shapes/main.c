/*
 * shapes: draws the demo scene.
 *   shapes                       window (needs the SDL backend); close it to quit
 *   shapes --frames N            window, quits by itself after N frames
 *   shapes --headless [--hash] [--png FILE]
 *                                renders once into memory; prints the framebuffer hash
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cydui_mem.h"
#include "scene.h"
#ifdef CYDUI_HAVE_SDL
#include "cydui_sdl.h"
#endif

#define W 320
#define H 240
#define STRIP_ROWS 40

/* All memory the library uses is provided here; it never calls malloc. */
static cydui_cmd cmd_mem[64];
static cydui_color strip_mem[W * STRIP_ROWS];
static cydui_color framebuffer[W * H];

static int init_ctx(cydui_ctx *ctx, const cydui_backend *be)
{
    cydui_config cfg;
    uint32_t err;

    memset(&cfg, 0, sizeof(cfg));
    cfg.backend = be;
    cfg.cmd_mem = cmd_mem;
    cfg.cmd_mem_size = sizeof(cmd_mem);
    cfg.strip_mem = strip_mem;
    cfg.strip_mem_size = sizeof(strip_mem);
    err = cydui_init(ctx, &cfg);
    if (err) {
        fprintf(stderr, "cydui_init failed (0x%x)\n", (unsigned)err);
        return -1;
    }
    return 0;
}

static void frame(cydui_ctx *ctx)
{
    cydui_begin_frame(ctx, CYDUI_RGB(0, 0, 0));
    shapes_draw(ctx);
    cydui_end_frame(ctx);
    if (cydui_errors(ctx)) {
        fprintf(stderr, "warning: cydui errors 0x%x\n", (unsigned)cydui_errors(ctx));
        cydui_clear_errors(ctx);
    }
}

static int run_headless(bool print_hash, const char *png_path)
{
    cydui_mem mem;
    cydui_ctx ctx;

    cydui_mem_init(&mem, framebuffer, W, H);
    if (init_ctx(&ctx, &mem.backend)) {
        return 1;
    }
    frame(&ctx);
    if (print_hash) {
        printf("%016" PRIx64 "\n", cydui_mem_hash(&mem));
    }
    if (png_path && cydui_mem_write_png(&mem, png_path) != 0) {
        fprintf(stderr, "could not write %s\n", png_path);
        return 1;
    }
    return 0;
}

#ifdef CYDUI_HAVE_SDL
static int run_window(long max_frames)
{
    cydui_sdl *sdl = cydui_sdl_create("cydui shapes", W, H, 2);
    cydui_ctx ctx;
    cydui_event ev;
    long frames = 0;
    bool quit = false;

    if (!sdl) {
        fprintf(stderr, "could not open a window\n");
        return 1;
    }
    if (init_ctx(&ctx, cydui_sdl_backend(sdl))) {
        cydui_sdl_destroy(sdl);
        return 1;
    }
    while (!quit && (max_frames < 0 || frames < max_frames)) {
        const cydui_backend *be = cydui_sdl_backend(sdl);
        while (be->poll_event(be->user, &ev)) {
            if (ev.type == CYDUI_EVENT_QUIT) {
                quit = true;
            }
        }
        frame(&ctx);
        frames++;
    }
    cydui_sdl_destroy(sdl);
    return 0;
}
#endif

int main(int argc, char **argv)
{
    bool headless = false, print_hash = false;
    const char *png = NULL;
    long frames = -1;
    int i;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless")) {
            headless = true;
        } else if (!strcmp(argv[i], "--hash")) {
            print_hash = true;
        } else if (!strcmp(argv[i], "--png") && i + 1 < argc) {
            png = argv[++i];
        } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            frames = atol(argv[++i]);
        } else {
            fprintf(stderr, "usage: %s [--headless [--hash] [--png FILE]] [--frames N]\n", argv[0]);
            return 2;
        }
    }

    if (headless) {
        return run_headless(print_hash, png);
    }
#ifdef CYDUI_HAVE_SDL
    return run_window(frames);
#else
    (void)frames;
    fprintf(stderr, "built without the SDL backend; use --headless\n");
    return 2;
#endif
}
