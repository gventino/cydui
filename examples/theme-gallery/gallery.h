#ifndef GALLERY_H
#define GALLERY_H

/*
 * The screens of the theme gallery, shared by the PC program (SDL window and headless runs) and by the
 * firmware for the board: the same code, so the same pixels.
 */

#include "cydui/ui.h"

#define GALLERY_THEMES 5
#define GALLERY_TASKS 200
#define GALLERY_PAGES 6 /* controls, a list, a custom widget, a touch test, an animation, text fields */
#define GALLERY_CHECKS 4
#define GALLERY_ACCEPT_TAPS 50       /* presses whose latency is needed for the acceptance */
#define GALLERY_ACCEPT_MAX_PX 6      /* the largest error allowed on the four targets */
#define GALLERY_ACCEPT_P95_US 50000u /* the 95th percentile of the latency allowed (NFR-02) */

/** What the platform knows about the touch screen; it fills it before every frame (the PC has no raw numbers). */
typedef struct {
    bool known;            /* the platform fills this in */
    bool down;
    int16_t x, y;          /* the last position, in pixels */
    int raw_x, raw_y, raw_z; /* the controller's own numbers */
    int lat_n;             /* presses whose latency was measured, and p50 / p95 / max in microseconds */
    unsigned lat_p50_us, lat_p95_us, lat_max_us;
} gallery_touch;

typedef struct {
    cydui_theme themes[GALLERY_THEMES]; /* default, clean, clean_dark, retro, cyberpunk */
    int want_theme;                      /* the theme a button asked for */
    int tab;
    bool done[3], wifi, dark, notify;
    bool task_done[GALLERY_TASKS];
    int stars[3];
    int saves;
    uint32_t frame;                /* frames built so far: the animation page follows it */
    gallery_touch touch;           /* filled by the platform */
    bool calibrate_requested;      /* the "Calibrar" button: the platform runs its calibration and clears this */
    int check_n;                   /* touch test: targets tapped so far (GALLERY_CHECKS when done) */
    int check_err[GALLERY_CHECKS]; /* ... and how far from each target the tap was, in pixels */
    bool acceptance;               /* the platform asks for the guided test: the taps after the four do not start over */
    char name[32], secret[24], notes[160]; /* the text page */
    bool confirm_clear;            /* ... and the confirmation of the button that clears them */
} gallery;

typedef enum {
    GALLERY_ACCEPT_TARGETS, /* the four targets are not all tapped yet */
    GALLERY_ACCEPT_TAPS_LEFT, /* ... and then GALLERY_ACCEPT_TAPS presses are needed to measure the latency */
    GALLERY_ACCEPT_PASS,
    GALLERY_ACCEPT_FAIL
} gallery_accept_state;

/** Where the guided test is, and the largest error of the targets so far (`max_px` may be NULL). */
gallery_accept_state gallery_acceptance(const gallery *g, int *max_px);

/** Builds the themes and sets the state of the first screen: `theme` (0..GALLERY_THEMES - 1) and page `tab` (0..GALLERY_PAGES - 1). */
void gallery_init(gallery *g, int theme, int tab);

/** The widgets of one frame, between cydui_ui_begin and cydui_ui_end. */
void gallery_frame(cydui_ui *ui, gallery *g);

/** True while the page shows something that moves by itself: the platform keeps drawing frames then. */
bool gallery_animating(const gallery *g);

/** After the frame: switches the theme if a button asked for another one (it shows from the next frame). */
void gallery_apply(cydui_ui *ui, gallery *g);

#endif /* GALLERY_H */
