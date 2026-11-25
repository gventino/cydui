#include <stdio.h>
#include <string.h>

#include "cydui_sans_16.h"
#include "cydui_themes.h"
#include "gallery.h"
#include "rating.h"

static const char *const theme_names[GALLERY_THEMES] = {"Base", "Light", "Night", "Retro", "Cyber"};

void gallery_init(gallery *g, int theme, int tab)
{
    memset(g, 0, sizeof(*g));
    cydui_theme_default(&g->themes[0], &cydui_sans_16);
    g->themes[0].name = "default";
    cydui_theme_init_clean(&g->themes[1]);
    cydui_theme_init_clean_dark(&g->themes[2]);
    cydui_theme_init_retro(&g->themes[3]);
    cydui_theme_init_cyberpunk(&g->themes[4]);
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
        snprintf(label, sizeof(label), "Task %d", (int)i + 1);
        cydui_begin_list_item(ui, i);
        cydui_checkbox(ui, label, &app->task_done[i]);
        cydui_end_list_item(ui);
    }
    cydui_list_end(ui);
}


/* ---- the touch test: where the UI believes the finger is, and how far from four targets it taps ---- */

static int isqrt_int(int v)
{
    int r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return r;
}

gallery_accept_state gallery_acceptance(const gallery *g, int *max_px)
{
    int i, worst = 0;

    for (i = 0; i < g->check_n && i < GALLERY_CHECKS; i++) {
        worst = g->check_err[i] > worst ? g->check_err[i] : worst;
    }
    if (max_px) {
        *max_px = worst;
    }
    if (g->check_n < GALLERY_CHECKS) {
        return GALLERY_ACCEPT_TARGETS;
    }
    if (g->touch.lat_n < GALLERY_ACCEPT_TAPS) {
        return GALLERY_ACCEPT_TAPS_LEFT;
    }
    return worst <= GALLERY_ACCEPT_MAX_PX && g->touch.lat_p95_us < GALLERY_ACCEPT_P95_US ? GALLERY_ACCEPT_PASS
                                                                                         : GALLERY_ACCEPT_FAIL;
}

static void touch_page(cydui_ui *ui, gallery *g)
{
    static const int tx[GALLERY_CHECKS] = {25, 75, 25, 75}; /* the targets, in percent of the area */
    static const int ty[GALLERY_CHECKS] = {25, 25, 75, 75};
    const cydui_style *st = &cydui_theme_of(ui)->style;
    char text[72];
    cydui_rect room, area;
    cydui_id id;
    cydui_response resp;
    int i, worst = 0, sum = 0;

    CYDUI_ROW(ui) {
        cydui_set_cross_align(ui, CYDUI_PLACE_CENTER);
        if (cydui_button(ui, "Calibrate")) {
            g->calibrate_requested = true;
        }
        if (g->touch.known) {
            snprintf(text, sizeof(text), "%d,%d raw %d,%d z%d", g->touch.x, g->touch.y, g->touch.raw_x, g->touch.raw_y,
                     g->touch.raw_z);
        } else {
            snprintf(text, sizeof(text), "tap the targets");
        }
        cydui_label(ui, text);
    }
    room = cydui_layout_avail(ui);
    area = cydui_alloc_rect(ui, room.w, room.h);
    id = cydui_id_str(ui, "touch-area");
    cydui_id_claim(ui, id);
    resp = cydui_interact(ui, id, area, CYDUI_SENSE_CLICK | CYDUI_SENSE_DRAG);

    cydui_draw_rrect(&ui->ctx, area, st->radius, st->panel, 255);
    for (i = 0; i < GALLERY_CHECKS; i++) {
        int cx = area.x + area.w * tx[i] / 100, cy = area.y + area.h * ty[i] / 100;
        bool next = i == g->check_n, done = i < g->check_n;
        cydui_rect r = {(int16_t)(cx - 10), (int16_t)(cy - 10), 20, 20};
        cydui_draw_rrect_outline(&ui->ctx, r, 10, next ? 3 : 2, next ? st->accent : st->border, 255);
        if (done) {
            cydui_draw_rrect(&ui->ctx, (cydui_rect){(int16_t)(cx - 3), (int16_t)(cy - 3), 6, 6}, 3, st->accent, 255);
        }
    }
    if (g->touch.known && g->touch.lat_n > 0) { /* only where there is something to measure: the PC page is unchanged */
        snprintf(text, sizeof(text), "latencia p95 %u ms, mediana %u ms, n=%d", g->touch.lat_p95_us / 1000u,
                 g->touch.lat_p50_us / 1000u, g->touch.lat_n);
        cydui_draw_text_in(ui, area, area.x + 8, area.y + 6, text, strlen(text), st->text_muted);
    }
    if (resp.clicked && g->check_n < GALLERY_CHECKS) {
        int cx = area.x + area.w * tx[g->check_n] / 100, cy = area.y + area.h * ty[g->check_n] / 100;
        int dx = resp.x - cx, dy = resp.y - cy;
        g->check_err[g->check_n++] = isqrt_int(dx * dx + dy * dy);
    } else if (resp.clicked && !g->acceptance) {
        g->check_n = 0; /* a tap after the four: start over */
    }
    if (resp.pressed) { /* a cross under the finger, where the UI thinks it is */
        cydui_draw_line(&ui->ctx, (int16_t)(resp.x - 12), resp.y, (int16_t)(resp.x + 12), resp.y, 1, st->text, 255);
        cydui_draw_line(&ui->ctx, resp.x, (int16_t)(resp.y - 12), resp.x, (int16_t)(resp.y + 12), 1, st->text, 255);
    }
    if (g->acceptance) { /* what is left to do, at the bottom of the area */
        int max_px;
        gallery_accept_state as = gallery_acceptance(g, &max_px);
        size_t n;
        cydui_text_extent e;

        if (as == GALLERY_ACCEPT_TARGETS) {
            snprintf(text, sizeof(text), "toque os circulos: %d/%d", g->check_n, GALLERY_CHECKS);
        } else if (as == GALLERY_ACCEPT_TAPS_LEFT) {
            snprintf(text, sizeof(text), "max error %d px. Tap %d more times", max_px, GALLERY_ACCEPT_TAPS - g->touch.lat_n);
        } else {
            snprintf(text, sizeof(text), "%s: %d px, p95 %u ms", as == GALLERY_ACCEPT_PASS ? "PASSED" : "FAILED", max_px,
                     g->touch.lat_p95_us / 1000u);
        }
        n = strlen(text);
        e = cydui_text_size(st->font, text, n, 0);
        cydui_draw_text_in(ui, area, area.x + (area.w - e.w) / 2, area.y + area.h - e.h - 6, text, n, st->text);
    }
    if (g->check_n >= GALLERY_CHECKS && !g->acceptance) {
        for (i = 0; i < GALLERY_CHECKS; i++) {
            sum += g->check_err[i];
            worst = g->check_err[i] > worst ? g->check_err[i] : worst;
        }
        snprintf(text, sizeof(text), "erro medio %d px, maximo %d px", sum / GALLERY_CHECKS, worst);
        {
            size_t n = strlen(text);
            cydui_text_extent e = cydui_text_size(st->font, text, n, 0);
            cydui_draw_text_in(ui, area, area.x + (area.w - e.w) / 2, area.y + (area.h - e.h) / 2, text, n, st->text);
        }
    }
}

/* ---- the animation: two bars that move by themselves, so that only a couple of strips change per frame ---- */

/* ---- text: a field, a password, a text area, and a confirmation before everything is cleared ---- */

static void text_page(cydui_ui *ui, gallery *g)
{
    const cydui_rect avail = cydui_layout_avail(ui);
    int view = cydui_keyboard_top(ui) - avail.y - 4; /* the window ends where the keyboard begins */
    char text[64]; /* 3 numbers of up to 9 digits and the words: more than they will ever need */

    view = view > avail.h ? avail.h : (view < 60 ? 60 : view);
    cydui_scroll_begin(ui, "texto", (int16_t)view);
    cydui_text_input(ui, "Name", g->name, sizeof(g->name), 24, 0);
    cydui_text_input(ui, "Password", g->secret, sizeof(g->secret), 16, CYDUI_TEXT_PASSWORD);
    cydui_text_area(ui, "Comments", g->notes, sizeof(g->notes), 0, 3, 0);
    CYDUI_ROW(ui) {
        cydui_set_cross_align(ui, CYDUI_PLACE_CENTER);
        if (cydui_button(ui, "Clear")) {
            g->confirm_clear = true;
        }
        snprintf(text, sizeof(text), "%u + %u + %u letras", (unsigned)strlen(g->name), (unsigned)strlen(g->secret),
                 (unsigned)strlen(g->notes));
        cydui_label(ui, text);
    }
    cydui_scroll_end(ui);
    if (cydui_popup_begin(ui, "Clear all", &g->confirm_clear, 240, 110, CYDUI_POPUP_MODAL)) {
        cydui_label(ui, "Delete all three texts?");
        CYDUI_ROW(ui) {
            if (cydui_button(ui, "Cancel")) {
                g->confirm_clear = false;
            }
            if (cydui_button(ui, "Delete")) {
                g->name[0] = g->secret[0] = g->notes[0] = '\0';
                g->confirm_clear = false;
            }
        }
        cydui_popup_end(ui);
    }
}

static void anim_page(cydui_ui *ui, gallery *g)
{
    int32_t a = (int32_t)((g->frame * 3u) % 101u), b = (int32_t)((g->frame * 7u) % 101u);

    cydui_label(ui, "Animation");
    cydui_next_width(ui, 280);
    cydui_progress(ui, a, 100);
    cydui_next_width(ui, 280);
    cydui_slider(ui, "##anim", &b, 0, 100);
    cydui_label(ui, "only the strips of the bars change");
}

void gallery_frame(cydui_ui *ui, gallery *app)
{
    static const char *const names[3] = {"Wi-Fi", "Dark mode", "Locked"};
    static const char *const tabs[GALLERY_PAGES] = {"Items", "List", "Notes", "Touch", "Anim.", "Text"};
    char text[48];
    bool locked = true;

    app->frame++;
    cydui_tabs(ui, tabs, GALLERY_PAGES, &app->tab);
    if (app->tab == 3) {
        touch_page(ui, app);
        return;
    }
    if (app->tab == 4) {
        anim_page(ui, app);
        return;
    }
    if (app->tab == 5) {
        text_page(ui, app);
        return;
    }
    if (app->tab == 1) {
        task_list(ui, app);
        return;
    }
    if (app->tab == 2) {
        CYDUI_CARD(ui) {
            rating(ui, "Food", &app->stars[0]);
            rating(ui, "Service", &app->stars[1]);
            cydui_next_disabled(ui);
            rating(ui, "Price", &app->stars[2]);
        }
        return;
    }
    cydui_tabs(ui, theme_names, GALLERY_THEMES, &app->want_theme); /* the theme in use, and a tap picks another */
    CYDUI_ROW(ui) {
        cydui_next_width(ui, 150);
        CYDUI_COLUMN(ui) {
            cydui_checkbox(ui, names[0], &app->wifi);
            cydui_checkbox(ui, names[1], &app->dark);
            cydui_next_disabled(ui);
            cydui_checkbox(ui, names[2], &locked);
        }
        CYDUI_COLUMN(ui) {
            cydui_toggle(ui, "Alerts", &app->notify);
            cydui_toggle(ui, "Sounds", &app->done[0]);
            cydui_next_disabled(ui);
            cydui_toggle(ui, "Fixed", &locked);
        }
    }
    CYDUI_ROW(ui) {
        cydui_set_cross_align(ui, CYDUI_PLACE_CENTER); /* the counter sits in the middle of the buttons */
        if (cydui_button(ui, "Save")) {
            app->saves++;
        }
        cydui_next_disabled(ui);
        cydui_button(ui, "Remove");
        cydui_spacer_fill(ui);
        snprintf(text, sizeof(text), "saved %dx", app->saves);
        cydui_label(ui, text);
    }
}

bool gallery_animating(const gallery *g)
{
    return g->tab == 4;
}

void gallery_apply(cydui_ui *ui, gallery *g)
{
    if (g->want_theme >= 0 && g->want_theme < GALLERY_THEMES && &g->themes[g->want_theme] != cydui_theme_of(ui)) {
        cydui_set_theme(ui, &g->themes[g->want_theme]); /* takes effect in the next frame */
    }
}
