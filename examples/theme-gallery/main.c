/*
 * theme-gallery: the widgets in every theme, with a switch at run time.
 *   theme-gallery                       window; the mouse is the touch screen
 *   theme-gallery --frames N            window, quits by itself after N frames
 *   theme-gallery --headless [--theme 0|1|2] [--tab 0|1] [--press X,Y [--release]] [--hash] [--png FILE]
 *                                       renders into memory (themes: 0 default, 1 clean, 2 clean_dark)
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/ui.h"
#include "cydui_mem.h"
#include "cydui_sans_16.h"
#include "cydui_themes.h"
#ifdef CYDUI_HAVE_SDL
#include "cydui_sdl.h"
#endif

#define W 320
#define H 240

CYDUI_STORAGE(storage, W, H, 20, 192, 1024, 32);
static cydui_color framebuffer[W * H];

#define N_THEMES 3
static cydui_theme themes[N_THEMES];
static const char *const theme_names[N_THEMES] = {"Padrao", "Claro", "Escuro"};

typedef struct {
    int want_theme;
    int tab;
    bool done[3], wifi, dark, notify;
    bool task_done[200];
    int saves;
} app_t;

static void build_themes(void)
{
    cydui_theme_default(&themes[0], &cydui_sans_16);
    themes[0].name = "default";
    cydui_theme_init_clean(&themes[1]);
    cydui_theme_init_clean_dark(&themes[2]);
}

static void task_list(cydui_ui *ui, app_t *app)
{
    int32_t first, last, i;

    cydui_list_begin(ui, "tasks", 200, 36, 180, &first, &last);
    for (i = first; i < last; i++) {
        char label[32];
        snprintf(label, sizeof(label), "Tarefa %d", (int)i + 1);
        cydui_begin_list_item(ui, i);
        cydui_checkbox(ui, label, &app->task_done[i]);
        cydui_end_list_item(ui);
    }
    cydui_list_end(ui);
}

static void ui_frame(cydui_ui *ui, app_t *app)
{
    static const char *const names[3] = {"Wi-Fi", "Modo escuro", "Bloqueado"};
    static const char *const tabs[2] = {"Controles", "Lista"};
    char text[48];
    bool locked = true;
    int i;

    cydui_tabs(ui, tabs, 2, &app->tab);
    if (app->tab == 1) {
        task_list(ui, app);
        return;
    }
    CYDUI_ROW(ui) {
        for (i = 0; i < N_THEMES; i++) {
            cydui_push_id_int(ui, i);
            if (cydui_button(ui, theme_names[i])) {
                app->want_theme = i;
            }
            cydui_pop_id(ui);
        }
    }
    CYDUI_ROW(ui) {
        cydui_next_width(ui, 150);
        CYDUI_COLUMN(ui) {
            cydui_checkbox(ui, names[0], &app->wifi);
            cydui_checkbox(ui, names[1], &app->dark);
            cydui_next_disabled(ui);
            cydui_checkbox(ui, names[2], &locked);
        }
        CYDUI_COLUMN(ui) {
            cydui_toggle(ui, "Avisos", &app->notify);
            cydui_toggle(ui, "Sons", &app->done[0]);
            cydui_next_disabled(ui);
            cydui_toggle(ui, "Fixo", &locked);
        }
    }
    CYDUI_ROW(ui) {
        if (cydui_button(ui, "Salvar")) {
            app->saves++;
        }
        cydui_next_disabled(ui);
        cydui_button(ui, "Remover");
        cydui_spacer_fill(ui);
        snprintf(text, sizeof(text), "salvo %dx", app->saves);
        cydui_label(ui, text);
    }
}

static int init_ui(cydui_ui *ui, const cydui_backend *be, int theme)
{
    build_themes();
    if (cydui_ui_init(ui, &CYDUI_CONFIG(storage, be, &themes[theme]))) {
        fprintf(stderr, "cydui_ui_init failed\n");
        return -1;
    }
    return 0;
}

static void one_frame(cydui_ui *ui, app_t *app)
{
    cydui_ui_begin(ui, cydui_theme_of(ui)->style.bg);
    ui_frame(ui, app);
    cydui_ui_end(ui);
    if (app->want_theme >= 0 && app->want_theme < N_THEMES && &themes[app->want_theme] != cydui_theme_of(ui)) {
        cydui_set_theme(ui, &themes[app->want_theme]); /* takes effect in the next frame */
    }
}

static int run_headless(int theme, int tab, int px, int py, bool press, bool release, bool print_hash, const char *png)
{
    cydui_mem mem;
    cydui_ui ui;
    app_t app;
    int frame;

    cydui_mem_init(&mem, framebuffer, W, H);
    if (init_ui(&ui, &mem.backend, theme)) {
        return 1;
    }
    memset(&app, 0, sizeof(app));
    app.want_theme = theme;
    app.tab = tab;
    app.wifi = true;
    for (frame = 0; frame < 6; frame++) {
        if (press && frame == 3) {
            cydui_mem_push_event(&mem, CYDUI_EVENT_DOWN, (int16_t)px, (int16_t)py);
        }
        if (press && release && frame == 4) {
            cydui_mem_push_event(&mem, CYDUI_EVENT_UP, (int16_t)px, (int16_t)py);
        }
        one_frame(&ui, &app);
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
    app_t app;
    long frames = 0;

    if (!sdl) {
        fprintf(stderr, "could not open a window\n");
        return 1;
    }
    if (init_ui(&ui, cydui_sdl_backend(sdl), 1)) {
        cydui_sdl_destroy(sdl);
        return 1;
    }
    memset(&app, 0, sizeof(app));
    app.want_theme = 1;
    app.wifi = true;
    while (max_frames < 0 || frames < max_frames) {
        cydui_ui_begin(&ui, cydui_theme_of(&ui)->style.bg);
        ui_frame(&ui, &app);
        cydui_ui_end(&ui);
        if (app.want_theme >= 0 && app.want_theme < N_THEMES && &themes[app.want_theme] != cydui_theme_of(&ui)) {
            cydui_set_theme(&ui, &themes[app.want_theme]);
        }
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
            theme = theme < 0 ? 0 : (theme >= N_THEMES ? N_THEMES - 1 : theme);
        } else if (!strcmp(argv[i], "--tab") && i + 1 < argc) {
            tab = atoi(argv[++i]) ? 1 : 0;
        } else if (!strcmp(argv[i], "--press") && i + 1 < argc) {
            press = sscanf(argv[++i], "%d,%d", &px, &py) == 2;
        } else {
            fprintf(stderr, "usage: %s [--headless [--theme N] [--tab N] [--press X,Y [--release]] [--hash] [--png FILE]] "
                            "[--frames N]\n", argv[0]);
            return 2;
        }
    }
    if (headless) {
        return run_headless(theme, tab, px, py, press, release, print_hash, png);
    }
#ifdef CYDUI_HAVE_SDL
    return run_window(frames);
#else
    (void)frames;
    fprintf(stderr, "built without the SDL backend; use --headless\n");
    return 2;
#endif
}
