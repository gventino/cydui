/*
 * widgets: every widget and layout of the library.
 *   widgets                      window; the mouse is the touch screen
 *   widgets --frames N           window, quits by itself after N frames
 *   widgets --headless [--press X,Y] [--release] [--hash] [--png FILE]
 *                                renders into memory; --press puts a finger down at X,Y for the
 *                                last frame (and --release lifts it again before it)
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/ui.h"
#include "cydui_mem.h"
#include "cydui_sans_12.h"
#include "cydui_sans_16.h"
#ifdef CYDUI_HAVE_SDL
#include "cydui_sdl.h"
#endif

#define W 320
#define H 240

CYDUI_STORAGE(storage, W, H, 20, 128, 1024, 32);
static cydui_color framebuffer[W * H];

typedef struct {
    int tasks;
    bool done[3];
    bool wifi, dark, notify;
    int picked; /* 0 = none, 1..4 */
    char status[48];
} app_t;

static const char *const names[3] = {"Comprar leite", "Ligar para a mae", "Pagar a conta"};

static void ui_frame(cydui_ui *ui, app_t *app)
{
    char text[96];
    int i;

    CYDUI_ROW(ui) {
        if (cydui_button(ui, "Adicionar")) {
            app->tasks++;
            snprintf(app->status, sizeof(app->status), "adicionada: %d tarefa(s)", app->tasks);
        }
        if (app->tasks == 0) {
            cydui_next_disabled(ui);
        }
        if (cydui_button(ui, "Remover") && app->tasks > 0) {
            app->tasks--;
            snprintf(app->status, sizeof(app->status), "removida: %d tarefa(s)", app->tasks);
        }
        cydui_spacer_fill(ui);
        snprintf(text, sizeof(text), "%d tarefas", app->tasks);
        cydui_label(ui, text);
    }
    CYDUI_ROW(ui) {
        cydui_next_width(ui, 150);
        CYDUI_COLUMN(ui) {
            for (i = 0; i < 3; i++) { /* the same kind of row three times: ids come from push_id */
                cydui_push_id_int(ui, i);
                cydui_checkbox(ui, names[i], &app->done[i]);
                cydui_pop_id(ui);
            }
        }
        CYDUI_COLUMN(ui) {
            cydui_toggle(ui, "Wi-Fi", &app->wifi);
            cydui_toggle(ui, "Modo escuro", &app->dark);
            cydui_toggle(ui, "Avisos", &app->notify);
        }
    }
    CYDUI_GRID(ui, 4) {
        for (i = 1; i <= 4; i++) {
            char label[8];
            snprintf(label, sizeof(label), "%d", i);
            cydui_next_fill(ui);
            if (cydui_button(ui, label)) {
                app->picked = i;
            }
        }
    }
    snprintf(text, sizeof(text), "botao %d  |  %s", app->picked, app->status);
    cydui_label(ui, text);
}

static void init_app(app_t *app)
{
    memset(app, 0, sizeof(*app));
    app->tasks = 3;
    app->wifi = true;
    app->done[1] = true;
    snprintf(app->status, sizeof(app->status), "pronto");
}

static int init_ui(cydui_ui *ui, cydui_theme *theme, const cydui_backend *be)
{
    cydui_theme_default(theme, &cydui_sans_12);
    if (cydui_ui_init(ui, &CYDUI_CONFIG(storage, be, theme))) {
        fprintf(stderr, "cydui_ui_init failed\n");
        return -1;
    }
    return 0;
}

static int run_headless(int px, int py, bool press, bool release, bool print_hash, const char *png)
{
    cydui_mem mem;
    cydui_theme theme;
    cydui_ui ui;
    app_t app;
    int frame;

    cydui_mem_init(&mem, framebuffer, W, H);
    if (init_ui(&ui, &theme, &mem.backend)) {
        return 1;
    }
    init_app(&app);
    for (frame = 0; frame < 5; frame++) { /* the layout settles in the second frame */
        if (press && frame == 3) {
            cydui_mem_push_event(&mem, CYDUI_EVENT_DOWN, (int16_t)px, (int16_t)py);
        }
        if (press && release && frame == 4) {
            cydui_mem_push_event(&mem, CYDUI_EVENT_UP, (int16_t)px, (int16_t)py);
        }
        cydui_ui_begin(&ui, theme.style.bg);
        ui_frame(&ui, &app);
        cydui_ui_end(&ui);
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
    cydui_sdl *sdl = cydui_sdl_create("cydui widgets", W, H, 2);
    cydui_theme theme;
    cydui_ui ui;
    app_t app;
    long frames = 0;

    if (!sdl) {
        fprintf(stderr, "could not open a window\n");
        return 1;
    }
    if (init_ui(&ui, &theme, cydui_sdl_backend(sdl))) {
        cydui_sdl_destroy(sdl);
        return 1;
    }
    init_app(&app);
    while (cydui_ui_begin(&ui, theme.style.bg) && (max_frames < 0 || frames < max_frames)) {
        ui_frame(&ui, &app);
        cydui_ui_end(&ui);
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
    int px = 0, py = 0, i;

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
        } else if (!strcmp(argv[i], "--press") && i + 1 < argc) {
            press = sscanf(argv[++i], "%d,%d", &px, &py) == 2;
        } else {
            fprintf(stderr, "usage: %s [--headless [--press X,Y [--release]] [--hash] [--png FILE]] "
                            "[--frames N]\n", argv[0]);
            return 2;
        }
    }
    if (headless) {
        return run_headless(px, py, press, release, print_hash, png);
    }
#ifdef CYDUI_HAVE_SDL
    return run_window(frames);
#else
    (void)frames;
    fprintf(stderr, "built without the SDL backend; use --headless\n");
    return 2;
#endif
}
