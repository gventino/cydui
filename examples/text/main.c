/*
 * text: the bundled fonts at three sizes, and a paragraph that wraps inside a box.
 *   text                         window: press or drag the mouse to resize the box; each press
 *                                also cycles left / center / right alignment
 *   text --frames N              window, quits by itself after N frames
 *   text --headless [--width W] [--align left|center|right] [--hash] [--png FILE]
 *                                renders once into memory
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/cydui.h"
#include "cydui_mem.h"
#include "cydui_sans_12.h"
#include "cydui_sans_16.h"
#include "cydui_sans_24.h"
#ifdef CYDUI_HAVE_SDL
#include "cydui_sdl.h"
#endif

#define W 320
#define H 240
#define STRIP_ROWS 40

static cydui_cmd cmd_mem[96];
static cydui_color strip_mem[W * STRIP_ROWS];
static char text_mem[2048];
static cydui_color framebuffer[W * H];

static const char paragraph[] =
    "Keeping a short list of tasks in focus helps to finish the day with a sense of a job well "
    "done. Drag the mouse to change the width of this box and watch the text reflow.";

static const char *align_name(cydui_align a)
{
    return a == CYDUI_ALIGN_LEFT ? "left" : a == CYDUI_ALIGN_CENTER ? "center" : "right";
}

static void draw_scene(cydui_ctx *ctx, int box_w, cydui_align align)
{
    char info[64];
    cydui_text_extent e = cydui_text_size(&cydui_sans_12, paragraph, CYDUI_NTS, (int16_t)(box_w - 8));
    cydui_rect box;

    cydui_draw_rect(ctx, (cydui_rect){0, 0, W, H}, CYDUI_RGB(16, 20, 32), 255);
    cydui_draw_text(ctx, &cydui_sans_24, 8, 4, 0, CYDUI_ALIGN_LEFT, "cydui text", CYDUI_NTS,
                    CYDUI_RGB(255, 210, 90), 255);
    cydui_draw_text(ctx, &cydui_sans_16, 8, 34, 0, CYDUI_ALIGN_LEFT, "Café, naïve Zoë! Ångström €5.",
                    CYDUI_NTS, CYDUI_RGB(255, 255, 255), 255);

    box.x = 8;
    box.y = 62;
    box.w = (int16_t)box_w;
    box.h = (int16_t)(e.h + 8);
    cydui_draw_rrect_outline(ctx, box, 6, 1, CYDUI_RGB(90, 110, 170), 255);
    cydui_draw_text(ctx, &cydui_sans_12, (int16_t)(box.x + 4), (int16_t)(box.y + 4),
                    (int16_t)(box_w - 8), align, paragraph, sizeof(paragraph) - 1,
                    CYDUI_RGB(235, 238, 250), 255);

    /* The info line is built in a temporary buffer: the library copies what it is given. */
    snprintf(info, sizeof(info), "box %d px, %s, %u lines, %d px high", box_w,
             align_name(align), (unsigned)e.lines, (int)e.h);
    cydui_draw_text(ctx, &cydui_sans_12, 8, 222, 0, CYDUI_ALIGN_LEFT, info, CYDUI_NTS,
                    CYDUI_RGB(120, 220, 160), 255);
}

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
    cfg.text_mem = text_mem;
    cfg.text_mem_size = sizeof(text_mem);
    err = cydui_init(ctx, &cfg);
    if (err) {
        fprintf(stderr, "cydui_init failed (0x%x)\n", (unsigned)err);
        return -1;
    }
    return 0;
}

static void frame(cydui_ctx *ctx, int box_w, cydui_align align)
{
    cydui_begin_frame(ctx, CYDUI_RGB(0, 0, 0));
    draw_scene(ctx, box_w, align);
    cydui_end_frame(ctx);
    if (cydui_errors(ctx)) {
        fprintf(stderr, "warning: cydui errors 0x%x\n", (unsigned)cydui_errors(ctx));
        cydui_clear_errors(ctx);
    }
}

static int clamp_width(int w)
{
    return w < 40 ? 40 : (w > W - 16 ? W - 16 : w);
}

static int run_headless(int box_w, cydui_align align, bool print_hash, const char *png_path)
{
    cydui_mem mem;
    cydui_ctx ctx;

    cydui_mem_init(&mem, framebuffer, W, H);
    if (init_ctx(&ctx, &mem.backend)) {
        return 1;
    }
    frame(&ctx, clamp_width(box_w), align);
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
    cydui_sdl *sdl = cydui_sdl_create("cydui text", W, H, 2);
    cydui_ctx ctx;
    cydui_event ev;
    long frames = 0;
    bool quit = false;
    int box_w = 170;
    cydui_align align = CYDUI_ALIGN_LEFT;

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
            } else if (ev.type == CYDUI_EVENT_DOWN || ev.type == CYDUI_EVENT_MOVE) {
                box_w = clamp_width(ev.x - 8);
                if (ev.type == CYDUI_EVENT_DOWN) {
                    align = (cydui_align)((align + 1) % 3);
                }
            }
        }
        frame(&ctx, box_w, align);
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
    int width = 170, i;
    cydui_align align = CYDUI_ALIGN_LEFT;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--headless")) {
            headless = true;
        } else if (!strcmp(argv[i], "--hash")) {
            print_hash = true;
        } else if (!strcmp(argv[i], "--png") && i + 1 < argc) {
            png = argv[++i];
        } else if (!strcmp(argv[i], "--frames") && i + 1 < argc) {
            frames = atol(argv[++i]);
        } else if (!strcmp(argv[i], "--width") && i + 1 < argc) {
            width = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--align") && i + 1 < argc) {
            i++;
            align = !strcmp(argv[i], "center") ? CYDUI_ALIGN_CENTER
                    : !strcmp(argv[i], "right") ? CYDUI_ALIGN_RIGHT : CYDUI_ALIGN_LEFT;
        } else {
            fprintf(stderr, "usage: %s [--headless [--width W] [--align left|center|right] [--hash] "
                            "[--png FILE]] [--frames N]\n", argv[0]);
            return 2;
        }
    }
    if (headless) {
        return run_headless(width, align, print_hash, png);
    }
#ifdef CYDUI_HAVE_SDL
    return run_window(frames);
#else
    (void)frames;
    fprintf(stderr, "built without the SDL backend; use --headless\n");
    return 2;
#endif
}
