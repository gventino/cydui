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
    CYDUI_EVENT_QUIT,
    CYDUI_EVENT_CHAR, /**< a character was typed: `key` is its code point */
    CYDUI_EVENT_KEY   /**< an editing key was pressed: `key` is a cydui_key */
} cydui_event_type;

/** The keys of CYDUI_EVENT_KEY. */
typedef enum {
    CYDUI_KEY_NONE,
    CYDUI_KEY_BACKSPACE,
    CYDUI_KEY_DELETE,
    CYDUI_KEY_LEFT,
    CYDUI_KEY_RIGHT,
    CYDUI_KEY_UP,
    CYDUI_KEY_DOWN,
    CYDUI_KEY_HOME,
    CYDUI_KEY_END,
    CYDUI_KEY_ENTER,
    CYDUI_KEY_ESCAPE
} cydui_key;

/**
 * Normalized input event. DOWN, MOVE and UP carry a position in x/y (screen pixels); CHAR and KEY carry `key`
 * (and ignore x/y). Any source may produce the keys: a keyboard on the screen, a physical keyboard, an
 * application that receives text from somewhere else.
 */
typedef struct {
    cydui_event_type type;
    int16_t x, y;
    uint32_t key;
} cydui_event;

/**
 * What the core needs from a platform. The core includes no SDL or ESP-IDF headers; every
 * platform provides one of these.
 */
typedef struct {
    void *user;
    int16_t width, height;

    /**
     * Receives a rendered area: area->w * area->h pixels, row-major, stride area->w. With
     * strip_buffers = 2 (cydui_config) the call may return BEFORE the area has been sent, as long as
     * the pixels stay untouched until the next call: the core draws the following strip into the
     * other buffer, and calls flush_strip (or frame_end) only after the previous one is safe to
     * reuse, which is why this function must not return before the transfer of the PREVIOUS call is
     * complete. With one buffer the pixels are only valid until this function returns.
     */
    void (*flush_strip)(void *user, const cydui_rect *area, const cydui_color *pixels);

    /** Returns true and fills `ev` if an event is pending. May be NULL. */
    bool (*poll_event)(void *user, cydui_event *ev);

    /** Monotonic milliseconds. May be NULL: without a clock nothing animates and a long press or the repeat of a key never fires. */
    uint32_t (*now_ms)(void *user);

    /** Called after the last strip of a frame (e.g. present the window); every transfer must be
     *  complete when it returns. May be NULL. */
    void (*frame_end)(void *user);
} cydui_backend;

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_BACKEND_H */
