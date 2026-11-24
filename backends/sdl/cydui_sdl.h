#ifndef CYDUI_SDL_H
#define CYDUI_SDL_H

/* SDL2 backend: a window showing the rendered frame, and the mouse as a touch screen. */

#include "cydui/backend.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cydui_sdl cydui_sdl;

/** Opens a window of width*scale x height*scale. Returns NULL on failure. */
cydui_sdl *cydui_sdl_create(const char *title, int16_t width, int16_t height, int scale);

/** The backend to pass to cydui_init. Valid until cydui_sdl_destroy. */
const cydui_backend *cydui_sdl_backend(const cydui_sdl *s);

void cydui_sdl_destroy(cydui_sdl *s);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_SDL_H */
