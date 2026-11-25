#include <stdio.h>
#include <string.h>

#include "cydui_sans_16.h"
#include "cydui_themes.h"
#include "gallery.h"
#include "rating.h"

static const char *const theme_names[GALLERY_THEMES] = {"Padrao", "Claro", "Escuro"};

void gallery_init(gallery *g, int theme, int tab)
{
    memset(g, 0, sizeof(*g));
    cydui_theme_default(&g->themes[0], &cydui_sans_16);
    g->themes[0].name = "default";
    cydui_theme_init_clean(&g->themes[1]);
    cydui_theme_init_clean_dark(&g->themes[2]);
    g->want_theme = theme < 0 ? 0 : (theme >= GALLERY_THEMES ? GALLERY_THEMES - 1 : theme);
    g->tab = tab < 0 ? 0 : (tab >= GALLERY_PAGES ? GALLERY_PAGES - 1 : tab);
    g->stars[0] = 3;
    g->stars[1] = 5;
    g->stars[2] = 2;
    g->wifi = true;
}

static void task_list(cydui_ui *ui, gallery *app)
{
    int32_t first, last, i;

    cydui_list_begin(ui, "tasks", GALLERY_TASKS, 36, 180, &first, &last);
    for (i = first; i < last; i++) {
        char label[32];
        snprintf(label, sizeof(label), "Tarefa %d", (int)i + 1);
        cydui_begin_list_item(ui, i);
        cydui_checkbox(ui, label, &app->task_done[i]);
        cydui_end_list_item(ui);
    }
    cydui_list_end(ui);
}

void gallery_frame(cydui_ui *ui, gallery *app)
{
    static const char *const names[3] = {"Wi-Fi", "Modo escuro", "Bloqueado"};
    static const char *const tabs[3] = {"Controles", "Lista", "Estrelas"};
    char text[48];
    bool locked = true;
    int i;

    cydui_tabs(ui, tabs, 3, &app->tab);
    if (app->tab == 1) {
        task_list(ui, app);
        return;
    }
    if (app->tab == 2) {
        CYDUI_CARD(ui) {
            rating(ui, "Comida", &app->stars[0]);
            rating(ui, "Servico", &app->stars[1]);
            cydui_next_disabled(ui);
            rating(ui, "Preco", &app->stars[2]);
        }
        return;
    }
    CYDUI_ROW(ui) {
        for (i = 0; i < GALLERY_THEMES; i++) {
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
        cydui_set_cross_align(ui, CYDUI_PLACE_CENTER); /* the counter sits in the middle of the buttons */
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

void gallery_apply(cydui_ui *ui, gallery *g)
{
    if (g->want_theme >= 0 && g->want_theme < GALLERY_THEMES && &g->themes[g->want_theme] != cydui_theme_of(ui)) {
        cydui_set_theme(ui, &g->themes[g->want_theme]); /* takes effect in the next frame */
    }
}
