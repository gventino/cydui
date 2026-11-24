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
#include "cydui/theme.h"

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

/* ---- layout ---- */

/** Containers nested inside one another; deeper ones are ignored (CT-02). */
#ifndef CYDUI_MAX_LAYOUT_DEPTH
#define CYDUI_MAX_LAYOUT_DEPTH 8
#endif

typedef enum {
    CYDUI_LAYOUT_ROOT,
    CYDUI_LAYOUT_ROW,
    CYDUI_LAYOUT_COLUMN,
    CYDUI_LAYOUT_GRID,
    CYDUI_LAYOUT_PADDING
} cydui_layout_kind;

/** Size modifiers for the next widget or container only (set with the cydui_next_* functions). */
typedef struct {
    int16_t w, h;  /**< -1 = not set */
    bool fill;     /**< take a share of the space left along the container's main axis */
    bool stretch;  /**< take all the space across the container's main axis */
    bool disabled; /**< the widget does not react and is drawn dimmed */
} cydui_mods;

/** One open container: where it starts, how much room it has, and where its next child goes. */
typedef struct {
    uint8_t kind;
    int16_t x0, y0;        /**< top-left corner */
    int16_t avail_w, avail_h; /**< room it was given */
    int16_t cx, cy;        /**< where the next child starts */
    int16_t ext_w, ext_h;  /**< extent of the children placed so far, from (x0, y0) */
    int16_t spacing;
    int16_t pad;           /**< padding containers */
    int16_t cell_w, row_h; /**< grids */
    int16_t fix_w, fix_h;  /**< size the container takes in its parent; -1 = its content */
    uint16_t cols, count, child_idx;
    bool was_fill; /**< the container asked to fill its parent's main axis */
    uint16_t fills_prev, fills_now, fill_idx; /**< `fill` children: last frame, this frame, handed out so far */
    int16_t fixed_prev, fixed_now; /**< main-axis space taken by the others (and the gaps), same two frames */
    cydui_id sid;          /**< key of the container's entry in the state table */
} cydui_region;

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

/* ---- per-widget state ---- */

/** One entry of the state table. `a` and `b` are free for the owner of the id. */
typedef struct {
    cydui_id id;
    uint32_t last_frame; /**< frame in which the entry was last looked up */
    int32_t a, b;
    uint16_t used;
} cydui_state;

/* ---- setup ---- */

typedef struct {
    cydui_config core;       /**< backend and the memory cydui_init needs */
    void *state_mem;         /**< memory for the state table (entries of cydui_state) */
    size_t state_mem_size;
    void *idset_mem;         /**< debug only: memory for collision detection (4 bytes per slot) */
    size_t idset_mem_size;
    const cydui_theme *theme; /**< required, and must outlive the UI; its font must be usable */
    int16_t tap_slop;        /**< pointer travel in pixels that still counts as a tap; 0 = 10 */
    uint16_t long_press_ms;  /**< hold time of a long press; 0 = 500 */
    uintptr_t (*thread_id)(void); /**< optional: identifies the calling task (CT-11) */
} cydui_ui_config;

typedef struct cydui_ui {
    cydui_ctx ctx;
    const cydui_theme *theme;
    cydui_state *state;
    uint32_t state_cap, sweep;
    uint32_t *idset;
    uint32_t idset_cap; /**< a power of two, or 0 */
    cydui_id id_stack[CYDUI_MAX_ID_DEPTH];
    uint8_t id_depth;
    cydui_id active; /**< the widget that owns the current press, or all zero */
    cydui_input in;
    cydui_mods mods;
    cydui_region regions[CYDUI_MAX_LAYOUT_DEPTH + 1]; /**< [0] is the whole screen */
    uint8_t depth;   /**< containers open right now */
    uint8_t skipped; /**< containers that were too deep to open, whose end must be skipped too */
    bool relayout;   /**< the layout changed shape: ask for another frame */
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

/* ---- layout functions ---- */

/**
 * Containers place their children one after the other. A row goes left to right, a column (and the
 * screen itself) top to bottom, a grid fills rows of `cols` equal cells. Positions are final when
 * a widget is placed. Each begin needs its end; a container left open is closed at the end of the
 * frame (CYDUI_ERR_LAYOUT), and one nested beyond CYDUI_MAX_LAYOUT_DEPTH is ignored together with
 * its end, its children flowing into the enclosing container.
 */
void cydui_begin_row(cydui_ui *ui);
void cydui_end_row(cydui_ui *ui);
void cydui_begin_column(cydui_ui *ui);
void cydui_end_column(cydui_ui *ui);
void cydui_begin_grid(cydui_ui *ui, int cols);
void cydui_end_grid(cydui_ui *ui);
/** A column whose children are inset by `px` on every side. */
void cydui_begin_padding(cydui_ui *ui, int px);
void cydui_end_padding(cydui_ui *ui);

/** Gap before each of the next children of the current container (the style's spacing by default). */
void cydui_set_spacing(cydui_ui *ui, int px);
/** A gap of `px` along the container's main axis. */
void cydui_spacer(cydui_ui *ui, int px);
/** A gap that takes a share of the space left along the main axis. */
void cydui_spacer_fill(cydui_ui *ui);

/* One-shot modifiers: they apply to the next widget or container and are then forgotten. */
void cydui_next_width(cydui_ui *ui, int px);
void cydui_next_height(cydui_ui *ui, int px);
/**
 * Fill the main axis (a row's width, a column's height): the space left, shared by all the `fill`
 * children. How many there are is known from the previous frame, so the first frame of a changed
 * layout is approximate; cydui_ui_end then asks for another frame.
 */
void cydui_next_fill(cydui_ui *ui);
/** Take all the room across the main axis (a column's width, a row's height). */
void cydui_next_stretch(cydui_ui *ui);
void cydui_next_disabled(cydui_ui *ui);

/** Reads and clears the pending modifiers. For widgets; widgets that ignore them can skip this. */
cydui_mods cydui_take_mods(cydui_ui *ui);

/** Places a w x h child, honoring (and clearing) the pending modifiers. */
cydui_rect cydui_alloc_rect(cydui_ui *ui, int w, int h);
/** The same with modifiers already taken by cydui_take_mods. */
cydui_rect cydui_alloc(cydui_ui *ui, const cydui_mods *mods, int w, int h);

/** The room left in the current container, from where the next child would start. */
cydui_rect cydui_layout_avail(const cydui_ui *ui);

/*
 * Scope sugar over begin/end:  CYDUI_ROW(ui) { ... }
 * A `break` or `return` inside the block skips the end call; the container is then closed at the
 * end of the frame, with a CYDUI_ERR_LAYOUT warning.
 */
#define CYDUI_CAT2_(a, b) a##b
#define CYDUI_CAT_(a, b) CYDUI_CAT2_(a, b)
#define CYDUI_SCOPE_(begin, end) \
    for (int CYDUI_CAT_(cydui_once_, __LINE__) = ((begin), 1); CYDUI_CAT_(cydui_once_, __LINE__); \
         CYDUI_CAT_(cydui_once_, __LINE__) = ((end), 0))
#define CYDUI_ROW(ui) CYDUI_SCOPE_(cydui_begin_row(ui), cydui_end_row(ui))
#define CYDUI_COLUMN(ui) CYDUI_SCOPE_(cydui_begin_column(ui), cydui_end_column(ui))
#define CYDUI_GRID(ui, cols) CYDUI_SCOPE_(cydui_begin_grid((ui), (cols)), cydui_end_grid(ui))
#define CYDUI_PADDING(ui, px) CYDUI_SCOPE_(cydui_begin_padding((ui), (px)), cydui_end_padding(ui))

/* ---- widgets ---- */

/*
 * Each widget takes the pending cydui_next_* modifiers, is placed by the current container and drawn
 * with the style. Labels are UTF-8; text after a "##" is not shown (it only tells widgets apart).
 * A widget call beyond CYDUI_MAX_WIDGETS in a frame does nothing (CYDUI_ERR_TOO_MANY_WIDGETS).
 */

/** One line of text. */
void cydui_label(cydui_ui *ui, const char *text);

/** Text that wraps at the room left in the current container. */
void cydui_label_wrapped(cydui_ui *ui, const char *text);

/** A button. Returns true in the frame in which it is tapped. */
bool cydui_button(cydui_ui *ui, const char *label);

/** A check box bound to *value. Returns true in the frame in which it was toggled. */
bool cydui_checkbox(cydui_ui *ui, const char *label, bool *value);

/** A switch bound to *value. Returns true in the frame in which it was toggled. */
bool cydui_toggle(cydui_ui *ui, const char *label, bool *value);

/* ---- setting up with static memory ---- */

/**
 * Declares, as one static object, all the memory a UI needs for a `w` x `h` screen: `cmds` draw
 * commands, `strip_rows` rows of render strip, `text_bytes` for the strings of one frame and
 * `state_entries` state entries.
 *
 *     CYDUI_STORAGE(mem, 320, 240, 20, 64, 512, 16);
 *     ...
 *     cydui_ui_init(&ui, &CYDUI_CONFIG(mem, backend, &theme));
 */
#define CYDUI_STORAGE(name, w, h, strip_rows, cmds, text_bytes, state_entries) \
    static struct {                                                            \
        cydui_cmd cmd[cmds];                                                   \
        cydui_color strip[(w) * (strip_rows)];                                 \
        char text[text_bytes];                                                 \
        cydui_state state[state_entries];                                      \
        uint32_t ids[(cmds) * 2];                                              \
    } name

/** A cydui_ui_config over a CYDUI_STORAGE object. */
#define CYDUI_CONFIG(storage, backend_ptr, theme_ptr)                                           \
    ((cydui_ui_config){.core = {.backend = (backend_ptr),                                       \
                                .cmd_mem = (storage).cmd,                                       \
                                .cmd_mem_size = sizeof((storage).cmd),                          \
                                .strip_mem = (storage).strip,                                   \
                                .strip_mem_size = sizeof((storage).strip),                      \
                                .text_mem = (storage).text,                                     \
                                .text_mem_size = sizeof((storage).text)},                       \
                       .state_mem = (storage).state,                                            \
                       .state_mem_size = sizeof((storage).state),                               \
                       .idset_mem = (storage).ids,                                              \
                       .idset_mem_size = sizeof((storage).ids),                                 \
                       .theme = (theme_ptr)})

/* ---- themes ---- */

/**
 * Switches the theme (FR-52). The theme must stay valid for as long as it is in use. An invalid one
 * is refused with CYDUI_ERR_BAD_ARG and the current theme stays. Asks for another frame, because
 * fonts and metrics may change the size of everything.
 */
uint32_t cydui_set_theme(cydui_ui *ui, const cydui_theme *t);

/** The theme in use. */
const cydui_theme *cydui_theme_of(const cydui_ui *ui);

/** How a widget looks in a state, from the theme in use. Never NULL. */
const cydui_wstyle *cydui_wstyle_of(const cydui_ui *ui, cydui_widget_kind kind, cydui_widget_state state,
                                    bool selected);

/** Draws a widget with the painter of the theme in use (its own, or the default). */
void cydui_paint(cydui_ui *ui, const cydui_paint_info *info);

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
