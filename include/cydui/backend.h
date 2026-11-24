#ifndef CYDUI_BACKEND_H
#define CYDUI_BACKEND_H

#include "cydui/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CYDUI_EVENT_DOWN,
    CYDUI_EVENT_MOVE,
    CYDUI_EVENT_UP,
    CYDUI_EVENT_QUIT
} cydui_event_type;

/** Normalized input event; x/y are screen pixels. */
typedef struct {
    cydui_event_type type;
    int16_t x, y;
} cydui_event;

/**
 * What the core needs from a platform. The core includes no SDL or ESP-IDF headers; every
 * platform provides one of these.
 */
typedef struct {
    void *user;
    int16_t width, height;

    /** Receives a rendered area: area->w * area->h pixels, row-major, stride area->w. */
    void (*flush_strip)(void *user, const cydui_rect *area, const cydui_color *pixels);

    /** Returns true and fills `ev` if an event is pending. May be NULL. */
    bool (*poll_event)(void *user, cydui_event *ev);

    /** Monotonic milliseconds. May be NULL (animations are not used before It7). */
    uint32_t (*now_ms)(void *user);

    /** Called after the last strip of a frame (e.g. present the window). May be NULL. */
    void (*frame_end)(void *user);
} cydui_backend;

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_BACKEND_H */
