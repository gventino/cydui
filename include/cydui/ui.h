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

#include "cydui/anim.h"
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
/** Tweens that may run at the same time (CT-08); the extra ones jump to their target. */
#ifndef CYDUI_MAX_TWEENS
#define CYDUI_MAX_TWEENS 16
#endif
/** The longest tween in ms; a longer one is cut to this. */
#ifndef CYDUI_ANIM_MAX_MS
#define CYDUI_ANIM_MAX_MS 5000
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

/** Where a child (or the whole group of children) sits in the room its container has. */
typedef enum {
    CYDUI_PLACE_START, /**< top or left: the default */
    CYDUI_PLACE_CENTER,
    CYDUI_PLACE_END    /**< bottom or right */
} cydui_place;

typedef enum {
    CYDUI_LAYOUT_ROOT,
    CYDUI_LAYOUT_ROW,
    CYDUI_LAYOUT_COLUMN,
    CYDUI_LAYOUT_GRID,
    CYDUI_LAYOUT_PADDING,
    CYDUI_LAYOUT_SCROLL /**< a scroll area or a virtual list (see cydui_scroll_begin) */
} cydui_layout_kind;

/** Size modifiers for the next widget or container only (set with the cydui_next_* functions). */
typedef struct {
    int16_t w, h;  /**< -1 = not set */
    bool fill;     /**< take a share of the space left along the container's main axis */
    bool stretch;  /**< take all the space across the container's main axis */
    bool disabled; /**< the widget does not react and is drawn dimmed */
} cydui_mods;

/**
 * One open container: where it starts, how much room it has, and where its next child goes.
 * PRIVATE: it is declared here only because cydui_ui holds an array of them. Not part of the API.
 */
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
    cydui_id wid;          /**< scroll areas: the id of the area itself (its state holds the offset) */
    int32_t off_px;        /**< scroll areas: how far the content is scrolled, in pixels */
    int32_t content;       /**< lists: the exact height of the whole content */
    int16_t view_y;        /**< scroll areas: top of the window (y0 moves with the content) */
    int16_t item_h;        /**< lists: height of one item */
    uint8_t scroll_flags;  /**< CYDUI_SCROLL_F_* */
    uint8_t place_cross, place_main; /**< cydui_place of the children across and along the container */
    uint8_t keep_ext;      /**< the parent places its children: remember how big this container is */
    uint8_t overlay;       /**< a popup: it takes no room in its parent */
    uint8_t card;          /**< opened by cydui_begin_card */
    uint8_t known;         /**< its size from the previous frame was found in the state table */
    int16_t prev_w, prev_h; /**< size of the children as measured in the previous frame */
    uint32_t cmd_start;    /**< cards: where in the command list the card's children begin */
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
    CYDUI_DISABLED = 1u << 2,    /**< does not react and does not capture the press */
    CYDUI_CAPTURE = 1u << 3      /**< keeps the press however far it travels: nothing may take it over */
};

/** What happened to one widget in this frame. */
typedef struct {
    bool pressed;      /**< held down now (or let go this frame) with the pointer over the widget */
    bool clicked;      /**< a tap: pressed and released on the widget without travelling far or holding */
    bool long_pressed; /**< held still long enough; true in exactly one frame per press */
    bool dragging;     /**< the pointer has travelled beyond the tap slop since the press */
    bool released;     /**< the press that this widget owned ended in this frame */
    int16_t dx, dy;    /**< pointer travel in this frame (with CYDUI_SENSE_DRAG, while held) */
    int16_t x, y;      /**< where the pointer is, for the widget that owns the press (where it was let go,
                            in the frame of the release); 0 for every other widget */
} cydui_response;

/**
 * Pointer state and the events waiting for the next frame.
 * PRIVATE: the application learns what the touch did from cydui_response, not from here. Not part of the API.
 */
#ifndef CYDUI_KEY_QUEUE
#define CYDUI_KEY_QUEUE 32 /**< typed characters and keys that wait for the focused widget in one frame */
#endif

typedef struct {
    cydui_event queue[CYDUI_EVENT_QUEUE];
    uint8_t q_len;
    cydui_event keys[CYDUI_KEY_QUEUE]; /**< CHAR and KEY events of this frame; the oldest goes when it is full */
    uint8_t k_len;
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

/** One entry of the state table. `a` to `d` are free for the owner of the id. */
typedef struct {
    cydui_id id;
    uint32_t last_frame; /**< frame in which the entry was last looked up */
    int32_t a, b, c, d;
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
    /**
     * Optional time budget of a frame in ms (0 = none; needs a clock in the backend). Once a frame
     * has taken longer since cydui_ui_begin, the widgets that are still to be called do nothing and
     * CYDUI_ERR_FRAME_BUDGET is raised (CT-15): a screen that is too heavy slows down instead of
     * starving the rest of the firmware.
     */
    uint16_t frame_budget_ms;
} cydui_ui_config;

/**
 * The state of one UI. The application provides the storage (see CYDUI_STORAGE), hands the pointer to the
 * functions of this header and to nothing else: THE FIELDS ARE PRIVATE and may change in any version
 * (docs/11-api.md). Only the size of the struct is something the application depends on, through sizeof.
 */
typedef struct cydui_ui {
    /* ---- private from here ---- */
    cydui_ctx ctx;
    const cydui_theme *theme;
    cydui_state *state;
    uint32_t state_cap, sweep;
    uint32_t *idset;
    uint32_t idset_cap; /**< a power of two, or 0 */
    cydui_id id_stack[CYDUI_MAX_ID_DEPTH];
    uint8_t id_depth;
    cydui_id active; /**< the widget that owns the current press, or all zero */
    cydui_id focus;  /**< the widget that gets the typed keys, or all zero */
    bool focus_seen;    /**< the focused widget was called in this frame (else the focus is dropped) */
    bool focus_claimed; /**< a press that began in this frame was on a widget that takes the focus */
    bool popup_prev;    /**< a popup was shown in the previous frame: what is outside it does not react */
    bool kb_prev;       /**< the keyboard was shown in the previous frame: what is under it does not react */
    bool kb_off;        /**< the application turned the keyboard off (cydui_keyboard_enable) */
    bool in_kb;         /**< the keyboard's own keys are being handled */
    bool kb_shift;
    uint8_t kb_page;
    uint32_t kb_rep;    /**< repeats of a held Backspace so far */
    bool popup_now;     /**< a popup is shown in this frame */
    bool in_popup;      /**< between cydui_popup_begin and cydui_popup_end */
    uint32_t popup_start, popup_end; /**< its commands, to be moved to the end of the list */
    cydui_rect popup_saved_clip;
    bool active_capture; /**< that widget asked to keep the press (CYDUI_CAPTURE) */
    cydui_input in;
    cydui_mods mods;
    cydui_region regions[CYDUI_MAX_LAYOUT_DEPTH + 1]; /**< [0] is the whole screen */
    uint8_t depth;   /**< containers open right now */
    uint8_t skipped; /**< containers that were too deep to open, whose end must be skipped too */
    bool relayout;   /**< the layout changed shape: ask for another frame */
    uint32_t frame;
    uint16_t widgets;
    uint8_t tweens_now; /**< tweens that are still moving in this frame */
    uint8_t fx_level;   /**< cydui_set_effects_level */
    uint32_t theme_since; /**< the clock when the theme in use was put in use (stamped in the next frame) */
    bool theme_fresh;     /**< ... and it has not been stamped yet */
    uint16_t tap_slop, long_press_ms, frame_budget_ms;
    uint32_t frame_start;
    uintptr_t (*thread_id)(void);
    uintptr_t thread;
} cydui_ui;

/** Returns 0 or a CYDUI_ERR_* flag. Does not allocate. */
uint32_t cydui_ui_init(cydui_ui *ui, const cydui_ui_config *cfg);

/** Starts a frame. Returns false once the user asked to close the application. */
bool cydui_ui_begin(cydui_ui *ui, cydui_color clear);

/**
 * Ends the frame: renders it and recycles old state. Returns true if another frame should be drawn
 * soon even without new input (layout still settling, a press in progress, a tween moving). When it
 * returns false the loop may sleep until the next event (CT-08).
 */
bool cydui_ui_end(cydui_ui *ui);

/** CYDUI_ERR_* flags raised since the last clear (the draw errors of the context included). */
uint32_t cydui_ui_errors(const cydui_ui *ui);
void cydui_ui_clear_errors(cydui_ui *ui);

/** Number of frames completed. */
uint32_t cydui_ui_frame(const cydui_ui *ui);

/* ---- time and animation ---- */

/**
 * Milliseconds since the theme in use was put in use (cydui_ui_init or cydui_set_theme with another theme),
 * counted from the first frame it was used in; 0 without a clock. For a theme that plays something when it comes in.
 */
uint32_t cydui_theme_age(const cydui_ui *ui);

/** Levels of the effects of a theme (R-02): what it may spend on looking good. */
#define CYDUI_FX_NONE 0 /**< no post-effect and no extra painting */
#define CYDUI_FX_LOW 1  /**< the cheap effects: static ones such as scanlines */
#define CYDUI_FX_FULL 2 /**< everything: glow, glitch, entrance animations */
#ifndef CYDUI_FX_DEFAULT
#define CYDUI_FX_DEFAULT CYDUI_FX_FULL /**< the level a cydui_ui starts with */
#endif

/**
 * How much a theme may spend on effects, CYDUI_FX_NONE .. CYDUI_FX_FULL (more is FULL). A theme without
 * effects ignores it. Turn it down when the frame rate suffers; the next frame is drawn again in full.
 */
void cydui_set_effects_level(cydui_ui *ui, unsigned level);
unsigned cydui_effects_level(const cydui_ui *ui);

/** The clock of the frame, in ms: what the backend's now_ms gave in cydui_ui_begin (0 without a clock). */
uint32_t cydui_now(const cydui_ui *ui);

/**
 * A value that moves to its target (FR-53). `id` names the tween (any id: it is kept apart from the
 * widget's own state); the first time it is asked for, it starts at `from`, and every time the target
 * changes it goes there from wherever it is, in `duration_ms` along the curve `ease`. Returns the value
 * for this frame, in the units of `from` and `target`. Ask for it every frame while it matters. With no
 * clock in the backend, a duration of 0 or a full state table the value is the target at once. At most
 * CYDUI_MAX_TWEENS move at the same time (CYDUI_ERR_TWEEN_LIMIT), and a tween nobody asks for is recycled
 * with the other state. cydui_ui_end returns true while any tween moves.
 */
int32_t cydui_animate(cydui_ui *ui, cydui_id id, int32_t from, int32_t target, uint16_t duration_ms,
                      cydui_ease ease);

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

/**
 * How the children of the container that is open now are placed. Call these right after opening it:
 * they apply to the children placed after the call.
 *
 * cross: each child is put at the start, the middle or the end of the container's extent across its
 * main axis (a row's children vertically, a column's horizontally). The extent is the container's
 * own size when it has one (cydui_next_width/height, stretch or fill) and otherwise the size of its
 * tallest/widest child, so a label and a button in a row are centered on each other.
 *
 * main: the whole group of children is put at the start, middle or end of the container along its
 * main axis. That needs room to spare, so the container must have a size (cydui_next_width,
 * stretch, fill); a container that just wraps its children has none and nothing moves.
 *
 * The sizes come from the previous frame (the same one frame of delay as `fill`; cydui_ui_end asks
 * for another frame while they settle), so in the first frame everything sits at the start. Grids
 * ignore both. The default is CYDUI_PLACE_START. A container that places its children uses one
 * entry of the state table, and so does every container directly inside it.
 */
void cydui_set_cross_align(cydui_ui *ui, cydui_place place);
void cydui_set_main_align(cydui_ui *ui, cydui_place place);
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

/**
 * A card: a column with the theme's padding on all sides and the theme's PANEL behind it, exactly
 * as big as what is inside (or as the size given with cydui_next_width/height/stretch/fill), already
 * in the first frame. The panel is painted when the card ends and moved behind its children.
 */
void cydui_begin_card(cydui_ui *ui);
void cydui_end_card(cydui_ui *ui);
#define CYDUI_CARD(ui) CYDUI_SCOPE_(cydui_begin_card(ui), cydui_end_card(ui))

/* ---- scrolling ---- */

/** A flick below this speed (px/s) does not coast. */
#ifndef CYDUI_SCROLL_FLING_MIN
#define CYDUI_SCROLL_FLING_MIN 50
#endif
/** A coasting scroll stops when it slows below this (px/s). */
#ifndef CYDUI_SCROLL_MIN_V
#define CYDUI_SCROLL_MIN_V 10
#endif
/** Time constant of the friction (ms): the speed falls to about a third in this time. */
#ifndef CYDUI_SCROLL_FRICTION_MS
#define CYDUI_SCROLL_FRICTION_MS 325
#endif
/** The fastest a flick can start (px/s). */
#ifndef CYDUI_SCROLL_MAX_V
#define CYDUI_SCROLL_MAX_V 6000
#endif
/** A list scrolls over at most this many pixels, whatever count * item_h says. */
#define CYDUI_LIST_MAX_PX (1 << 22)

enum { CYDUI_SCROLL_F_LIST = 1u << 0, CYDUI_SCROLL_F_INERT = 1u << 1 };

/**
 * A window `view_h` pixels tall (and as wide as the room it has; see cydui_next_width) onto content
 * that is taller. Dragging inside it scrolls, and lets go into a coast that slows down by itself.
 * A button the finger lands on still clicks when the finger does not travel; once it does, the area
 * takes the touch over and the button does not click. A widget that keeps the press (a slider) is
 * not taken over. `id` must differ from the other ids at this point of the id stack. The height of
 * the content is the one of the previous frame (the same one frame of delay as `fill`). A scroll
 * area inside another one is not supported: it is a plain column of that height, and CYDUI_ERR_BAD_ARG
 * is raised. A bar shows on the right while the content is taller than the window. Whenever
 * something is still coasting cydui_ui_end returns true.
 */
void cydui_scroll_begin(cydui_ui *ui, const char *id, int16_t view_h);
void cydui_scroll_end(cydui_ui *ui);
#define CYDUI_SCROLL(ui, id, view_h) CYDUI_SCOPE_(cydui_scroll_begin((ui), (id), (view_h)), cydui_scroll_end(ui))

/** Jumps the area `id` (as spelled at this point of the id stack) to `y` pixels from the top, no coasting. */
void cydui_scroll_to(cydui_ui *ui, const char *id, int32_t y);

/**
 * A scroll area for a long list of equal items. The application only builds the ones in [*first, *last)
 * (both set by the call and within [0, count]), so a thousand items cost the same as ten:
 *
 *     int32_t first, last;
 *     cydui_list_begin(ui, "tasks", n, 36, 150, &first, &last);
 *     for (int32_t i = first; i < last; i++) {
 *         cydui_begin_list_item(ui, i);   // a 36 px row; ids inside are mixed with i
 *         cydui_checkbox(ui, tasks[i].title, &tasks[i].done);
 *         cydui_end_list_item(ui);
 *     }
 *     cydui_list_end(ui);
 *
 * The content is exactly count * item_h tall, at most CYDUI_LIST_MAX_PX; an item_h of 0 or less or a
 * count of 0 or less is an empty list.
 */
void cydui_list_begin(cydui_ui *ui, const char *id, int32_t count, int16_t item_h, int16_t view_h,
                      int32_t *first, int32_t *last);
void cydui_begin_list_item(cydui_ui *ui, int32_t index);
void cydui_end_list_item(cydui_ui *ui);
void cydui_list_end(cydui_ui *ui);

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

/**
 * A slider for an integer in [min, max]: a track with a knob, `label` (if it shows anything) to its
 * left. A touch anywhere on the track sets the value and dragging follows the finger, even when it
 * leaves the track. Returns true in a frame in which the user changed the value. A value outside the
 * range is clamped quietly; with max <= min the slider is drawn disabled and does nothing. Its
 * default width is 160 px (use cydui_next_width/stretch/fill). It keeps the press, so a scroll area
 * does not take it away.
 */
bool cydui_slider(cydui_ui *ui, const char *label, int32_t *value, int32_t min, int32_t max);

/** A progress bar, `value` out of `max` (both clamped: nothing below 0, nothing above `max`). */
void cydui_progress(cydui_ui *ui, int32_t value, int32_t max);

/** The most tabs a bar shows; `count` is clamped to this. */
#ifndef CYDUI_MAX_TABS
#define CYDUI_MAX_TABS 8
#endif

/**
 * A bar of equal-width tabs. `labels` must hold at least min(count, CYDUI_MAX_TABS) entries; a NULL
 * entry is an empty label. *selected is clamped into the range and changed by a tap; the call
 * returns true in the frame in which another tab was chosen. The content below is the application's
 * business: `if (selected == 0) { ... }`. The bar is as wide as the room it has by default.
 */
bool cydui_tabs(cydui_ui *ui, const char *const *labels, int count, int *selected);

/* ---- text ---- */

/** Flags of the text fields. */
#define CYDUI_TEXT_PASSWORD 0x100u /**< shows a bullet for every character; the buffer holds the real text */

/** What a text field reports (a bit mask; any non-zero value means something happened). */
#define CYDUI_TEXT_CHANGED 1u   /**< the text changed in this frame */
#define CYDUI_TEXT_CONFIRMED 2u /**< Enter was pressed: the field gave up the focus */

/**
 * A single line text field that edits the string in `buf` (`cap` bytes, always kept NUL terminated; a buffer the
 * application filled is made safe first, see cydui/textedit.h). `label` gives its id and, up to a "##", the hint
 * shown while the field is empty. A tap gives it the focus and puts the cursor where the finger is; only the
 * focused field receives the CYDUI_EVENT_CHAR and CYDUI_EVENT_KEY events (Backspace, Delete, Left, Right, Home,
 * End edit; Enter confirms and drops the focus; Escape drops it). The focus is also dropped by a press anywhere
 * else and when the field is not called for a frame. `max_chars` (0 = no limit) limits the characters; a refused
 * character (the limit, or no room in `cap`) outlines the field in the "selected" look for a moment. The text
 * scrolls sideways to keep the cursor in view. The field is as wide as the room it has by default and as tall as
 * its text with the theme's padding (at least min_target); a NULL `buf` or `cap` 0 makes it a disabled field.
 * Returns CYDUI_TEXT_CHANGED and CYDUI_TEXT_CONFIRMED bits.
 */
unsigned cydui_text_input(cydui_ui *ui, const char *label, char *buf, size_t cap, size_t max_chars, unsigned flags);

/**
 * A text area: several lines of text that wrap at the width of the area, edited in `buf` like cydui_text_input.
 * `rows` is how many lines show at once (1 to 16), the area is as wide as the room it has and as tall as the rows
 * need; the text scrolls by lines to keep the cursor in view. A tap puts the cursor on the line and the character
 * it is nearest to. Enter inserts a newline (there is nothing to confirm: the application closes the field), Up
 * and Down move between the wrapped lines, Home and End go to the start and the end of the line as it is shown.
 * `flags` is reserved (0). Returns CYDUI_TEXT_CHANGED when the text changed.
 */
unsigned cydui_text_area(cydui_ui *ui, const char *label, char *buf, size_t cap, size_t max_chars, int rows, unsigned flags);

/* ---- popups ---- */

/** Flags of cydui_popup_begin. */
#define CYDUI_POPUP_MODAL 1u /**< a press outside does not close it (and neither does Escape): the user has to choose */

/**
 * A panel in the middle of the screen, above everything else, that takes the touch while it is shown (FR-35):
 * the widgets outside it do not react. Call it every frame with the application's `*open`; when it returns true
 * build the content, in a column inside the panel, and then call cydui_popup_end (when it returns false do not):
 *
 *     if (cydui_popup_begin(ui, "Delete?", &confirm_open, 240, 120, CYDUI_POPUP_MODAL)) {
 *         cydui_label(ui, "Delete this task?");
 *         if (cydui_button(ui, "Delete")) { ...; confirm_open = false; }
 *         cydui_popup_end(ui);
 *     }
 *
 * `w` and `h` are the size of the panel (clamped to the screen). Without CYDUI_POPUP_MODAL a press outside
 * the panel, or Escape, sets *open to false. The label is also an id scope: the widgets inside cannot collide with
 * the ones outside. It may be called wherever in the frame, also inside a container or a scroll area (it is not
 * clipped by them and takes no room in them); one popup at a time, a second one in the same frame is refused.
 * The text field that has the focus keeps it while the popup is open, so a field in the popup works.
 */
bool cydui_popup_begin(cydui_ui *ui, const char *label, bool *open, int w, int h, unsigned flags);

/** Ends the content of the popup; only after cydui_popup_begin returned true. */
void cydui_popup_end(cydui_ui *ui);

/* ---- the keyboard on the screen ---- */

/**
 * While a text field has the focus a keyboard is shown at the bottom of the screen, above everything else, and its
 * keys give the field CYDUI_EVENT_CHAR and CYDUI_EVENT_KEY events. Pages: letters (Shift for one capital), numbers
 * and punctuation, symbols, and accented letters (Latin-1). A key that is held when it is Backspace repeats.
 * Nothing else reacts to a touch on it. The keyboard is part of the UI, you do not call it; the application only
 * has to leave the field where the keyboard does not cover it: cydui_keyboard_top says where the keyboard begins
 * (the previous frame's: the first frame of a focus still reports the height of the screen).
 */
int cydui_keyboard_top(const cydui_ui *ui);

/** Turns the keyboard on the screen on (the default) or off, for an application that has another way to type. */
void cydui_keyboard_enable(cydui_ui *ui, bool on);

/** Gives the focus to the field with this label (in the current id scope), or to nothing if `label` is NULL. */
void cydui_focus_set(cydui_ui *ui, const char *label);

/** Drops the focus. */
void cydui_focus_clear(cydui_ui *ui);

/** True while a text field has the focus (a keyboard on the screen, if there is one, is shown then). */
bool cydui_has_focus(const cydui_ui *ui);

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

/* ---- writing a widget ---- */

/*
 * A widget of your own is a function that, in this order:
 *
 *   1. cydui_widget_begin(ui)          counts it against the per-frame limit; false: return at once
 *   2. cydui_take_mods(ui)             picks up cydui_next_width/height/fill/stretch/disabled
 *   3. cydui_id_str(ui, label) and cydui_id_claim(ui, id)     its identity (label[#...] as for the others)
 *   4. cydui_label_len, cydui_text_size(font of cydui_theme_of(ui)->style, ...)    to measure itself
 *   5. cydui_alloc(ui, &mods, w, h)    its place in the layout
 *   6. cydui_interact(ui, id, rect, flags)     what the touch did (resp.x/y say where)
 *   7. paints with cydui_draw_* on ui->ctx, cydui_draw_text_in, colors from cydui_theme_of(ui)->style
 *      or cydui_wstyle_of(ui, kind, cydui_state_of(disabled, pressed), selected), or cydui_paint
 *
 * Anything it must remember between frames goes in cydui_state_get(ui, id, true). The widgets of the
 * library are written this way. examples/theme-gallery/rating.c is a complete one.
 */

/**
 * Counts a widget against the limit of CYDUI_MAX_WIDGETS per frame and checks the calling task.
 * Returns false, with CYDUI_ERR_TOO_MANY_WIDGETS, when the limit is passed: the widget must then do
 * nothing.
 */
bool cydui_widget_begin(cydui_ui *ui);

/** How much of a label is shown: up to a "##", at most CYDUI_MAX_STR bytes. NULL is empty. */
size_t cydui_label_len(const char *label);

/** Draws `n` bytes of text with the font of the theme at (x, y), showing nothing outside `box`. */
void cydui_draw_text_in(cydui_ui *ui, cydui_rect box, int x, int y, const char *s, size_t n, cydui_color color);

/** The look to ask the theme for: DISABLED wins over PRESSED, otherwise PRESSED or NORMAL. */
cydui_widget_state cydui_state_of(bool disabled, bool pressed);

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
