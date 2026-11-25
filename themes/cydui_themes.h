#ifndef CYDUI_THEMES_H
#define CYDUI_THEMES_H

/* The themes shipped with the library. Each one is filled into a cydui_theme owned by the caller. */

#include "cydui/theme.h"

#ifdef __cplusplus
extern "C" {
#endif

/** "clean": light, flat controls, white cards with a soft shadow, a blue accent. 16 px body font. */
void cydui_theme_init_clean(cydui_theme *t);

/** "clean_dark": the same language on a dark background. */
void cydui_theme_init_clean_dark(cydui_theme *t);

/**
 * "retro": a silver page, square corners, a monospaced 1-bit font (cydui_mono_16), a navy accent and 3D
 * edges on everything: raised to push, sunken to type into. Needs more room for commands than the clean
 * themes: a slab is five of them (the keyboard on the screen about 300).
 */
void cydui_theme_init_retro(cydui_theme *t);

/**
 * "cyberpunk": a near black page, neon cyan and magenta, glowing controls, and a post-effect with
 * scanlines, a short glitch every few seconds and a sweep that draws the screen when the theme comes in
 * (it needs a clock in the backend; cydui_set_effects_level turns it down). 16 px body font.
 */
void cydui_theme_init_cyberpunk(cydui_theme *t);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_THEMES_H */
