#ifndef CYDUI_UI_H
#define CYDUI_UI_H

/*
 * Immediate-mode UI. Every frame the application calls the widget functions again:
 *
 *     while (cydui_ui_begin(&ui, clear)) {
 *         if (cydui_button(&ui, "Add")) add();
 *         cydui_ui_end(&ui);
 *     }
 *
 * The library keeps only what it must between frames (a small state table keyed by widget id).
 * It is NOT thread-safe: call it from one task only (CT-11).
 */

#include "cydui/cydui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- limits (CT-02). Override with -D at build time. ---- */

/** Widgets per frame; the rest of the frame's widget calls are ignored (AB-07). */
#ifndef CYDUI_MAX_WIDGETS
#define CYDUI_MAX_WIDGETS 512
#endif
/** Depth of push_id nesting. */
#ifndef CYDUI_MAX_ID_DEPTH
#define CYDUI_MAX_ID_DEPTH 8
#endif
/** Frames after which a widget's state is recycled if it was not seen. */
#ifndef CYDUI_STATE_TTL
#define CYDUI_STATE_TTL 120
#endif
/** Detect two widgets with the same id in a frame (costs a small table, see idset_mem). */
#ifndef CYDUI_DEBUG_IDS
#ifndef NDEBUG
#define CYDUI_DEBUG_IDS 1
#else
#define CYDUI_DEBUG_IDS 0
#endif
#endif
/** Check that the UI is called from the task that created it (needs cydui_ui_config.thread_id). */
#ifndef CYDUI_DEBUG_THREAD
#ifndef NDEBUG
#define CYDUI_DEBUG_THREAD 1
#else
#define CYDUI_DEBUG_THREAD 0
#endif
#endif

/* ---- input ---- */

/** Events polled from the backend and not yet processed (CT-05). */
#ifndef CYDUI_EVENT_QUEUE
#define CYDUI_EVENT_QUEUE 32
#endif
/** Events taken from the queue per frame; the rest waits for the next frame. */
#ifndef CYDUI_EVENTS_PER_FRAME
#define CYDUI_EVENTS_PER_FRAME 16
#endif
/** Events asked of the backend per frame, so that a backend that never runs dry cannot stall a frame. */
#ifndef CYDUI_POLL_PER_FRAME
#define CYDUI_POLL_PER_FRAME 64
#endif

/** What a widget wants to hear about; passed to cydui_interact. */
enum {
    CYDUI_SENSE_CLICK = 1u << 0, /**< report taps */
    CYDUI_SENSE_DRAG = 1u << 1,  /**< report the pointer's travel while the widget is held */
    CYDUI_DISABLED = 1u << 2     /**< does not react and does not capture the press */
};

/** What happened to one widget in this frame. */
typedef struct {
    bool pressed;      /**< held down now (or let go this frame) with the pointer over the widget */
    bool clicked;      /**< a tap: pressed and released on the widget without travelling far or holding */
    bool long_pressed; /**< held still long enough; true in exactly one frame per press */
    bool dragging;     /**< the pointer has travelled beyond the tap slop since the press */
    bool released;     /**< the press that this widget owned ended in this frame */
    int16_t dx, dy;    /**< pointer travel in this frame (with CYDUI_SENSE_DRAG, while held) */
} cydui_response;

/** Pointer state and the events waiting for the next frame. Read it, do not write it. */
typedef struct {
    cydui_event queue[CYDUI_EVENT_QUEUE];
    uint8_t q_len;
    uint8_t last_processed; /**< events taken from the queue in the last frame */
    bool down;
    bool quit;
    bool has_clock;
    bool long_fired, moved_far; /**< for the current press */
    bool frame_down, frame_up, frame_long; /**< edges that happened in this frame */
    bool claimed;                 /**< a widget took this frame's press already */
    int16_t x, y;                 /**< pointer position (clamped to the screen) */
    int16_t press_x, press_y;     /**< where the current press started */
    int16_t up_x, up_y;           /**< where the last press ended */
    int16_t frame_x, frame_y;     /**< pointer position at the start of the frame */
    uint32_t press_ms, now;
} cydui_input;

/* ---- ids ---- */

/**
 * Identity of a widget: a 32-bit hash of its label (mixed with any pushed ids) plus a 16-bit check
 * computed a different way, which keeps two labels that collide in the hash apart in the state table.
 */
typedef struct {
    uint32_t h;
    uint16_t c;
} cydui_id;

static inline bool cydui_id_eq(cydui_id a, cydui_id b)
{
    return a.h == b.h && a.c == b.c;
}

/* ---- per-widget state ---- */

/** One entry of the state table. `a` and `b` are free for the owner of the id. */
typedef struct {
    cydui_id id;
    uint32_t last_frame; /**< frame in which the entry was last looked up */
    int32_t a, b;
    uint16_t used;
} cydui_state;

/* ---- style (provisional: iteration 4 turns this into themes) ---- */

typedef struct {
    cydui_color bg, panel, text, text_muted, accent, accent_text, border;
    const cydui_font *font;
    uint8_t radius;
    uint8_t pad_x, pad_y;
    uint8_t spacing;
    uint8_t min_target; /**< smallest touch target in pixels (R-04) */
} cydui_style;

/** A dark style around `font`. */
cydui_style cydui_style_default(const cydui_font *font);

/* ---- setup ---- */

typedef struct {
    cydui_config core;       /**< backend and the memory cydui_init needs */
    void *state_mem;         /**< memory for the state table (entries of cydui_state) */
    size_t state_mem_size;
    void *idset_mem;         /**< debug only: memory for collision detection (4 bytes per slot) */
    size_t idset_mem_size;
    const cydui_style *style; /**< required: copied; its font must be usable */
    int16_t tap_slop;        /**< pointer travel in pixels that still counts as a tap; 0 = 10 */
    uint16_t long_press_ms;  /**< hold time of a long press; 0 = 500 */
    uintptr_t (*thread_id)(void); /**< optional: identifies the calling task (CT-11) */
} cydui_ui_config;

typedef struct cydui_ui {
    cydui_ctx ctx;
    cydui_style style;
    cydui_state *state;
    uint32_t state_cap, sweep;
    uint32_t *idset;
    uint32_t idset_cap; /**< a power of two, or 0 */
    cydui_id id_stack[CYDUI_MAX_ID_DEPTH];
    uint8_t id_depth;
    cydui_id active; /**< the widget that owns the current press, or all zero */
    cydui_input in;
    uint32_t frame;
    uint16_t widgets;
    uint16_t tap_slop, long_press_ms;
    uintptr_t (*thread_id)(void);
    uintptr_t thread;
} cydui_ui;

/** Returns 0 or a CYDUI_ERR_* flag. Does not allocate. */
uint32_t cydui_ui_init(cydui_ui *ui, const cydui_ui_config *cfg);

/** Starts a frame. Returns false once the user asked to close the application. */
bool cydui_ui_begin(cydui_ui *ui, cydui_color clear);

/**
 * Ends the frame: renders it and recycles old state. Returns true if another frame should be drawn
 * soon even without new input (layout still settling, a press in progress).
 */
bool cydui_ui_end(cydui_ui *ui);

/** CYDUI_ERR_* flags raised since the last clear (the draw errors of the context included). */
uint32_t cydui_ui_errors(const cydui_ui *ui);
void cydui_ui_clear_errors(cydui_ui *ui);

/** Number of frames completed. */
uint32_t cydui_ui_frame(const cydui_ui *ui);

/* ---- ids ---- */

/**
 * The id a widget with this label gets at this point of the frame. Only the text before a "##" is
 * shown by widgets, but all of it is hashed: "Done##3" and "Done##4" are different widgets that both
 * read "Done". At most CYDUI_MAX_STR bytes are used.
 */
cydui_id cydui_id_str(const cydui_ui *ui, const char *label);

/** Mixes a value into the ids of everything up to the matching cydui_pop_id (for lists). */
void cydui_push_id_str(cydui_ui *ui, const char *s);
void cydui_push_id_int(cydui_ui *ui, int32_t v);
void cydui_push_id_ptr(cydui_ui *ui, const void *p);
void cydui_pop_id(cydui_ui *ui);

/**
 * Records that a widget with this id exists in this frame and checks for a second one (debug
 * builds). Returns false, raising CYDUI_ERR_ID_COLLISION, if it was already used this frame.
 * Widgets call this; custom widgets should too.
 */
bool cydui_id_claim(cydui_ui *ui, cydui_id id);

/* ---- interaction ---- */

/**
 * The building block of every widget that reacts to touch (and of custom ones). Call it once per
 * frame with the widget's id and rectangle, after claiming the id.
 *
 * A press belongs to the first widget, in call order, whose rectangle (cut by the current clip)
 * contains the point where it started. A tap needs the press and the release on the same widget,
 * without the pointer travelling more than the tap slop and without turning into a long press, so
 * pressing one widget and letting go on another cancels both. A press and its release in the same
 * frame give one tap. Part of the API that may still change before it is frozen.
 */
cydui_response cydui_interact(cydui_ui *ui, cydui_id id, cydui_rect r, unsigned flags);

/* ---- state table ---- */

/**
 * Finds the state of `id`, creating a zeroed entry if `create` is set. Returns NULL if there is no
 * such entry and none could be created (the table is full of entries in use, or has no memory).
 * Entries not looked up for CYDUI_STATE_TTL frames are recycled; when the table is full the entry
 * unused for the longest time is replaced, never the one of the active widget.
 */
cydui_state *cydui_state_get(cydui_ui *ui, cydui_id id, bool create);

#ifdef __cplusplus
}
#endif

#endif /* CYDUI_UI_H */
