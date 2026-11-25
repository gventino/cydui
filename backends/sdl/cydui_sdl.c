#include "cydui_sdl.h"

#include <SDL.h>
#include <stdlib.h>
#include <string.h>

#include "cydui/utf8.h"

struct cydui_sdl {
    cydui_backend backend;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int scale;
    bool pressed;
    uint32_t typed[64]; /* characters of a text input event that wait to be handed out one by one */
    int typed_n, typed_i;
};

static bool key_of(SDL_Keycode k, uint32_t *out)
{
    switch (k) {
    case SDLK_BACKSPACE: *out = CYDUI_KEY_BACKSPACE; return true;
    case SDLK_DELETE: *out = CYDUI_KEY_DELETE; return true;
    case SDLK_LEFT: *out = CYDUI_KEY_LEFT; return true;
    case SDLK_RIGHT: *out = CYDUI_KEY_RIGHT; return true;
    case SDLK_UP: *out = CYDUI_KEY_UP; return true;
    case SDLK_DOWN: *out = CYDUI_KEY_DOWN; return true;
    case SDLK_HOME: *out = CYDUI_KEY_HOME; return true;
    case SDLK_END: *out = CYDUI_KEY_END; return true;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: *out = CYDUI_KEY_ENTER; return true;
    case SDLK_ESCAPE: *out = CYDUI_KEY_ESCAPE; return true;
    default: return false;
    }
}

static void sdl_flush(void *user, const cydui_rect *area, const cydui_color *pixels)
{
    cydui_sdl *s = (cydui_sdl *)user;
    SDL_Rect r;
    r.x = area->x;
    r.y = area->y;
    r.w = area->w;
    r.h = area->h;
    SDL_UpdateTexture(s->texture, &r, pixels, area->w * (int)sizeof(cydui_color));
}

static void sdl_frame_end(void *user)
{
    cydui_sdl *s = (cydui_sdl *)user;
    SDL_RenderClear(s->renderer);
    SDL_RenderCopy(s->renderer, s->texture, NULL, NULL);
    SDL_RenderPresent(s->renderer);
}

static bool sdl_poll(void *user, cydui_event *ev)
{
    cydui_sdl *s = (cydui_sdl *)user;
    SDL_Event e;

    if (s->typed_i < s->typed_n) { /* the rest of a text input event */
        memset(ev, 0, sizeof(*ev));
        ev->type = CYDUI_EVENT_CHAR;
        ev->key = s->typed[s->typed_i++];
        return true;
    }
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            ev->type = CYDUI_EVENT_QUIT;
            ev->x = ev->y = 0;
            return true;
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT) {
                s->pressed = true;
                ev->type = CYDUI_EVENT_DOWN;
                ev->x = (int16_t)(e.button.x / s->scale);
                ev->y = (int16_t)(e.button.y / s->scale);
                return true;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT) {
                s->pressed = false;
                ev->type = CYDUI_EVENT_UP;
                ev->x = (int16_t)(e.button.x / s->scale);
                ev->y = (int16_t)(e.button.y / s->scale);
                return true;
            }
            break;
        case SDL_TEXTINPUT: { /* what the keyboard types, as UTF-8 */
            size_t len = strlen(e.text.text), i = 0;

            s->typed_n = s->typed_i = 0;
            while (i < len && s->typed_n < (int)(sizeof(s->typed) / sizeof(s->typed[0]))) {
                i += cydui_utf8_decode(e.text.text + i, len - i, &s->typed[s->typed_n++]);
            }
            if (s->typed_n > 0) {
                memset(ev, 0, sizeof(*ev));
                ev->type = CYDUI_EVENT_CHAR;
                ev->key = s->typed[s->typed_i++];
                return true;
            }
            break;
        }
        case SDL_KEYDOWN: { /* the editing keys, with the repeat of the system */
            uint32_t key;

            if (key_of(e.key.keysym.sym, &key)) {
                memset(ev, 0, sizeof(*ev));
                ev->type = CYDUI_EVENT_KEY;
                ev->key = key;
                return true;
            }
            break;
        }
        case SDL_MOUSEMOTION:
            if (s->pressed) {
                ev->type = CYDUI_EVENT_MOVE;
                ev->x = (int16_t)(e.motion.x / s->scale);
                ev->y = (int16_t)(e.motion.y / s->scale);
                return true;
            }
            break;
        default:
            break;
        }
    }
    return false;
}

static uint32_t sdl_now(void *user)
{
    (void)user;
    return SDL_GetTicks();
}

cydui_sdl *cydui_sdl_create(const char *title, int16_t width, int16_t height, int scale)
{
    cydui_sdl *s;

    if (width <= 0 || height <= 0 || scale <= 0) {
        return NULL;
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return NULL;
    }
    s = (cydui_sdl *)calloc(1, sizeof(*s));
    if (!s) {
        SDL_Quit();
        return NULL;
    }
    s->scale = scale;
    s->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 width * scale, height * scale, 0);
    s->renderer = s->window ? SDL_CreateRenderer(s->window, -1, SDL_RENDERER_PRESENTVSYNC) : NULL;
    s->texture = s->renderer ? SDL_CreateTexture(s->renderer, SDL_PIXELFORMAT_RGB565,
                                                 SDL_TEXTUREACCESS_STREAMING, width, height)
                             : NULL;
    if (!s->texture) {
        cydui_sdl_destroy(s);
        return NULL;
    }
    s->backend.user = s;
    s->backend.width = width;
    s->backend.height = height;
    s->backend.flush_strip = sdl_flush;
    s->backend.poll_event = sdl_poll;
    s->backend.now_ms = sdl_now;
    s->backend.frame_end = sdl_frame_end;
    SDL_StartTextInput(); /* the keyboard of the computer types into the text fields */
    return s;
}

const cydui_backend *cydui_sdl_backend(const cydui_sdl *s)
{
    return &s->backend;
}

void cydui_sdl_destroy(cydui_sdl *s)
{
    if (!s) {
        return;
    }
    if (s->texture) SDL_DestroyTexture(s->texture);
    if (s->renderer) SDL_DestroyRenderer(s->renderer);
    if (s->window) SDL_DestroyWindow(s->window);
    free(s);
    SDL_Quit();
}
