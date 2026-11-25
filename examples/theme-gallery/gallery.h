#ifndef GALLERY_H
#define GALLERY_H

/*
 * The screens of the theme gallery, shared by the PC program (SDL window and headless runs) and by the
 * firmware for the board: the same code, so the same pixels.
 */

#include "cydui/ui.h"

#define GALLERY_THEMES 3
#define GALLERY_TASKS 200
#define GALLERY_PAGES 3

typedef struct {
    cydui_theme themes[GALLERY_THEMES]; /* default, clean, clean_dark */
    int want_theme;                      /* the theme a button asked for */
    int tab;
    bool done[3], wifi, dark, notify;
    bool task_done[GALLERY_TASKS];
    int stars[3];
    int saves;
} gallery;

/** Builds the themes and sets the state of the first screen: `theme` (0..2) and page `tab` (0..2). */
void gallery_init(gallery *g, int theme, int tab);

/** The widgets of one frame, between cydui_ui_begin and cydui_ui_end. */
void gallery_frame(cydui_ui *ui, gallery *g);

/** After the frame: switches the theme if a button asked for another one (it shows from the next frame). */
void gallery_apply(cydui_ui *ui, gallery *g);

#endif /* GALLERY_H */
