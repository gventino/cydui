# 12 — Guide

A short, practical guide. `11-api.md` is the contract (what is stable), the comments in `include/cydui/*.h` are the reference for every function, and this page is how the pieces fit. Every code block here is compiled and run by the author before it goes in (it is one file, `-std=c99 -pedantic -Wall -Wextra -Werror`, with ASan and UBSan).

## 1. Five minutes on the PC

```sh
cmake -S . -B build -G Ninja && cmake --build build
./build/examples/hello/hello                      # a button and a counter, 22 lines of C (needs SDL2)
./build/examples/theme-gallery/theme-gallery      # every widget, five themes, text fields, a keyboard on the screen
./build/examples/theme-gallery/theme-gallery --headless --theme 3 --tab 0 --png out.png   # no window: a picture
```

The smallest program is `examples/hello/main.c`: a `CYDUI_STORAGE` (all the memory), a theme, `cydui_ui_init`, and a loop of `cydui_ui_begin` / widgets / `cydui_ui_end`. The library never allocates; what you give it at init is all it will ever use.

### How much memory
`CYDUI_STORAGE(name, w, h, strip_rows, cmds, text_bytes, state_entries)`:
- `strip_rows`: rows rendered at a time (20 rows of 320 pixels = 12.8 KB; with two buffers on a board, twice that). A taller strip is faster, the picture is the same.
- `cmds`: draw commands per frame. A screen of the gallery needs 40 to 80; **the keyboard on the screen about 120 more** (about 300 in total in the `retro` theme, where every slab is five rectangles). 320 is a good number when text fields are used. A command that does not fit is dropped and `CYDUI_ERR_CMD_OVERFLOW` is raised.
- `text_bytes`: the strings of one frame (the library copies what it draws; 1.5 KB is enough for the gallery).
- `state_entries`: one per widget that remembers something (a scroll area, a text field, a tween). Each widget of a theme with `anim_ms` > 0 that changes its look uses one for its transition: give one entry for every such widget on a screen (32 bytes each). When the table is full a widget just changes at once.

Ask `cydui_ui_errors(&ui)` now and then in development: every limit that was hit leaves a bit there.

**Measure instead of guessing.** `cydui_ui_stats_get` reports the largest frame so far (commands, text bytes, tweens, widgets), how many state entries are in use, and what was configured; `cydui_ui_stats_reset` starts the peaks over so that one screen at a time can be measured. Run the busiest screens of the application, read the numbers, and size `CYDUI_STORAGE` from them with some room:

```c
/* ---- measuring the memory of a screen ---- */
void report(const cydui_ui *ui)
{
    cydui_ui_stats s;

    cydui_ui_stats_get(ui, &s);
    printf("commands %u of %u, text %u of %u bytes, state %u of %u, tweens %u of %u, widgets %u\n", (unsigned)s.cmds_peak, (unsigned)s.cmds_cap,
           (unsigned)s.text_peak, (unsigned)s.text_cap, (unsigned)s.state_used, (unsigned)s.state_cap, (unsigned)s.tweens_peak, (unsigned)s.tweens_cap,
           (unsigned)s.widgets_peak);
}
```

A state table that has to replace an entry that is still in use (a scroll offset, a text field's cursor) raises `CYDUI_ERR_STATE_FULL`: a bigger `state_entries` is the cure. A table of no entries is a choice and raises nothing, and a tween that finds no room just jumps to its target.

## 2. On the board

`examples/theme-gallery/idf` is a complete ESP-IDF 5.5 project for the CYD (`source tools/env.sh`, `idf.py build flash`); `backends/esp32/Kconfig` has the pins, the SPI clock (40 MHz by default, 80 works), the rotation and the touch options; `hw-probe` checks a new board without the library. A touch calibration is asked at the first boot and kept in the flash. At 40 MHz sending a whole screen takes 34.5 ms (26 fps) however fast the drawing is, so keep animations to what changes (the library only draws and sends the strips that changed when you give it `dirty_mem`, as the gallery does) or use 80 MHz.

## 3. Writing a theme

A theme is data: a palette and metrics (`cydui_style`), a table with the look of every kind of widget in every state (`table[kind][selected][state]`: fill, border, text, accent, border width, radius, shadow), and, only for what a table cannot say, painters and a post-effect. Start from the default and change what you want:

```c
/* ---- a theme: a palette and a table ---- */
void my_theme(cydui_theme *t)
{
    cydui_theme_default(t, &cydui_sans_16);           /* a sane table for every widget and state */
    t->name = "mine";
    t->style.bg = CYDUI_RGB(20, 24, 20);
    t->style.panel = CYDUI_RGB(30, 40, 30);
    t->style.text = CYDUI_RGB(210, 240, 210);
    t->style.accent = CYDUI_RGB(90, 200, 90);
    t->style.anim_ms = 120;                           /* looks change over 120 ms (0 = at once) */
    cydui_theme_derive_table(t);                      /* the table follows the palette */
    t->table[CYDUI_WIDGET_BUTTON][0][CYDUI_STATE_NORMAL].radius = 0; /* then tune single entries */
}
```

A theme may carry more than one font: `style.font` is the body (what the widgets of the library use), `style.font_small` and `style.font_title` are optional (NULL means the body font), and `cydui_theme_font(theme, CYDUI_FONT_SMALL)` finds them. `theme->user` is a pointer of the application's own that the library never reads, so a painter finds the palette its theme was built from without a lookup by name (section 9). `cydui_theme_derive_table` builds the table from the palette (pressed = darker, disabled = faded); after it you can set single entries. `cydui_theme_validate` (run by `cydui_ui_init` and `cydui_set_theme`) refuses a theme without a usable font and keeps the one in use.

### A painter
A painter draws one kind of widget. It may call `cydui_paint_default` and add to it, or draw everything with the `cydui_draw_*` functions of `ui->ctx`. Use `cydui_paint_look` (the table entry, blended while the widget changes) and `cydui_paint_amount` (how selected it is, 0 to `CYDUI_ANIM_ONE`) to take part in the transitions of a theme with `anim_ms`.

```c
/* ---- a painter: draw with the table and add to it ---- */
static void paint_button(cydui_ui *ui, const cydui_theme *t, const cydui_paint_info *p)
{
    cydui_paint_default(ui, t, p);                    /* the look of the table (also while it changes) */
    if (p->state == CYDUI_STATE_PRESSED) {            /* ... and a ring round a pressed button */
        cydui_rect q = p->rect;
        q.x = (int16_t)(q.x - 2); q.y = (int16_t)(q.y - 2); q.w = (int16_t)(q.w + 4); q.h = (int16_t)(q.h + 4);
        cydui_draw_rrect_outline(&ui->ctx, q, 4, 1, cydui_paint_look(t, p).accent, 120);
    }
}
```

`themes/retro.c` (3D edges as stacks of rectangles, a 1-bit font) and `themes/cyberpunk.c` (glow rings that fade with the transition, a gradient bar) are complete worked examples, 290 and 400 lines.

### A post-effect
A post-effect changes each strip after it is drawn and before it is sent: scanlines, a tint, a glitch. Two rules: it must depend on the **absolute row** (`y0 + row`), not on the strip it happens to be in (the picture must not depend on how the screen is cut), and `key` must say what the effect will do to those rows, so that a frame with dirty regions resends only the strips whose key changed (a constant for something static). It runs with the `cydui_ui` as `user` when it comes from a theme: ask `cydui_now(ui)` and `cydui_effects_level(ui)`, and stay cheap (the scanlines of `cyberpunk` cost 1.3 ms of a 17 ms frame on the board). An effect is trusted code: it receives a pointer into the strip and nothing stops it writing past it.

```c
/* ---- a post-effect: depends on the absolute row, never on the strip ---- */
static uint32_t fx_key(void *user, int16_t y0, int16_t h) { (void)user; (void)y0; (void)h; return 1; }
static void fx_apply(void *user, cydui_color *buf, int16_t width, int16_t y0, int16_t h)
{
    int row, x;
    (void)user;
    for (row = 0; row < h; row++) {
        if ((y0 + row) & 1) {
            for (x = 0; x < width; x++) buf[row * width + x] = cydui_blend(buf[row * width + x], 0, 80);
        }
    }
}
static const cydui_post_effect fx = {fx_key, fx_apply, NULL};
```

## 4. Writing a widget

A widget is a function that counts itself, picks up the modifiers (`cydui_next_*`), makes an id, asks the layout for a rectangle, reads the touch for it, and draws through the theme. `examples/theme-gallery/rating.c` is a complete one (a row of five stars with a label, 95 lines, public headers only); this is the skeleton:

```c
/* ---- a widget: a toggle button that remembers nothing, built from the public parts ---- */
bool pill(cydui_ui *ui, const char *label, bool *on)
{
    cydui_mods m;
    cydui_id id;
    cydui_rect r;
    cydui_response resp;
    cydui_paint_info pi;
    cydui_text_extent e;
    size_t n;

    if (!cydui_widget_begin(ui)) return false;                 /* counts it against CYDUI_MAX_WIDGETS */
    m = cydui_take_mods(ui);                                   /* cydui_next_width/height/disabled ... */
    id = cydui_id_str(ui, label);
    cydui_id_claim(ui, id);
    n = cydui_label_len(label);
    e = cydui_text_size(cydui_theme_of(ui)->style.font, label, n, 0);
    r = cydui_alloc(ui, &m, e.w + 24, 32);                     /* where it goes: the layout decides */
    resp = cydui_interact(ui, id, r, CYDUI_SENSE_CLICK | (m.disabled ? CYDUI_DISABLED : 0u));
    if (resp.clicked) *on = !*on;
    memset(&pi, 0, sizeof(pi));
    pi.kind = CYDUI_WIDGET_BUTTON;                             /* paint it as a button (or add a kind of your own to a theme) */
    pi.state = cydui_state_of(m.disabled, resp.pressed);
    pi.selected = *on;
    pi.rect = r; pi.text = label; pi.text_len = n; pi.text_extent = e;
    cydui_paint(ui, &pi);
    return resp.clicked;
}
```

The order matters: `cydui_widget_begin` first (it enforces `CYDUI_MAX_WIDGETS`), `cydui_take_mods` before anything else that places or draws, `cydui_id_claim` for the debug collision check, `cydui_alloc` for the place. A widget in a list or loop needs `cydui_push_id_int` so that its id is its own.

## 5. Writing a backend

A backend is a `cydui_backend`: `width`, `height`, `flush_strip(user, area, pixels)` (receives `area->w * area->h` RGB565 pixels in native byte order; convert for the panel yourself), `poll_event(user, ev)` (DOWN/MOVE/UP in screen pixels, CHAR and KEY for typing, QUIT), `now_ms(user)` (monotonic; without it nothing animates and a long press never fires) and `frame_end(user)` (called after the last strip: present the window, wait for the DMA). With `strip_buffers = 2` the core draws the next strip while the previous one is still being sent, so `flush_strip` may return before the transfer ends **but must not return before the transfer of the previous call has**. `backends/mem` (an in-memory framebuffer with synthetic events and a hash: the tests use it) and `backends/sdl` are the two short ones; `backends/esp32` is the real one.

Events from a source of your own (a keyboard, text from a phone) are plain `CYDUI_EVENT_CHAR` / `CYDUI_EVENT_KEY`: the library queues 32 of them per frame, filters controls and bidi marks, and gives them to the focused field.

## 6. Animation and effects

`cydui_animate(ui, id, from, target, ms, ease)` is a value that moves: ask for it every frame, it starts at `from` the first time and goes to every new target from where it is. At most 16 move together, none for more than 5 s, and `cydui_ui_end` returns true while one moves, so the loop can sleep when it returns false:

```c
while (cydui_ui_begin(&ui, bg)) {
    build_the_screen(&ui);
    if (!cydui_ui_end(&ui)) wait_for_the_next_event();   /* nothing moves: sleep */
}
```

`cydui_set_effects_level(ui, CYDUI_FX_LOW)` (or `_NONE`) turns down what a theme spends on glow and glitch when the frame rate suffers. Everything follows the clock of the backend, so a test with a made-up clock sees the frame the board sees.

## 7. Text fields and the keyboard

`cydui_text_input(ui, "Name", buf, sizeof buf, max_chars, flags)` edits a buffer of yours (UTF-8, always closed by a NUL, never cut inside a character), returns `CYDUI_TEXT_CHANGED` / `CYDUI_TEXT_CONFIRMED`, and `CYDUI_TEXT_PASSWORD` draws bullets. A tap gives it the focus and the keyboard of the screen appears; leave the field above `cydui_keyboard_top(ui)` or put it in a scroll area (it is scrolled into view). `cydui_text_area` is the several-lines version. A popup (`cydui_popup_begin(ui, "id", &open, w, h, CYDUI_POPUP_MODAL)`) is the confirmation for anything destructive: the touch is taken from everything under it. Your password buffer is yours: the library does not wipe it.

## 8. When something does not work
- A widget shows nothing or too late: `cydui_ui_errors` (`CYDUI_ERR_CMD_OVERFLOW`: more commands; `CYDUI_ERR_TOO_MANY_WIDGETS`; `CYDUI_ERR_LAYOUT`: an unbalanced container, popup or id).
- Two widgets act together: the same label twice in one scope. Use `"Label##2"` or `cydui_push_id_int`.
- The screen does not settle: the loop keeps getting `true` from `cydui_ui_end`. A tween that moves for ever, or two widgets sharing an id and fighting over their state.
- Not the same picture on the PC and on the board: look for a `float` in your own drawing code, or a clock that is not the one of the frame.

## 9. Patterns from a real application

These come from `cyd-todo`, a quest log on the CYD (the first application written on the library), and each one cost its author an afternoon. All the code below is compiled with `-std=c99 -pedantic -Wall -Wextra -Werror` and the sanitizers.

### A bitmap that changes in the same buffer
The dirty regions decide what to send by hashing the commands of each strip, and for a bitmap the hash takes **the address and the size**, not the pixels. A QR code drawn again with new content into the same buffer, at the same size, looks unchanged and is not sent. Call `cydui_invalidate(&ui.ctx)` when the content of a buffer you keep drawing changes. (A new size is noticed by itself, which hides the problem until two codes happen to have the same size.)

### A mask of one bit per pixel, at a whole-number scale
A QR code is one bit per module. `CYDUI_BITMAP_A1` and `cydui_draw_bitmap_a1` draw such a mask with every bit as a `scale` x `scale` square (1 to 8), so the code costs its own size (165 bytes for the 33 modules of a version 4 code, 246 with a quiet zone of four modules in the mask) and not that of a 4-bit mask of the drawn picture (13.4 KB for the same code with its quiet zone at scale 4). The result is pixel for pixel what the 4-bit mask gave.

```c
/* ---- a 1 bit mask drawn at a whole-number scale: a QR code ---- */
void draw_qr(cydui_ui *ui, int x, int y, int modules, const uint8_t *mask, size_t mask_len, cydui_color ink)
{
    cydui_draw_bitmap_a1(&ui->ctx, (int16_t)x, (int16_t)y, (int16_t)modules, (int16_t)modules, 4, mask, mask_len, ink, 255);
}
```

### Text that does not fit
`cydui_text_fit(font, s, len, max_w)` is the number of bytes of the longest start of a string whose width **as drawn** fits `max_w` (the font rounds the advance, so a width is not the sum of the widths of the letters, and a cut found by adding them up is a few pixels off), and `cydui_text_ellipsize` writes that start plus an ellipsis into a buffer of yours:

```c
/* ---- text that does not fit ---- */
void title_that_fits(cydui_ui *ui, const char *title, int room)
{
    char buf[64];
    bool cut;

    cydui_text_ellipsize(cydui_theme_of(ui)->style.font, title, CYDUI_NTS, (int16_t)room, buf, sizeof buf, &cut);
    cydui_label(ui, buf);
}
```

### Labels in another font, color or alpha
`cydui_next_font`, `cydui_next_color` and `cydui_next_alpha` are modifiers for the next label (or wrapped label) only, like `cydui_next_width`; any other widget forgets them. Combined with the fonts of the theme they make a heading, a caption or a faded line without a widget of your own:

```c
/* ---- a bold, colored, wrapped label ---- */
void heading(cydui_ui *ui, const char *text, cydui_color gold)
{
    cydui_next_font(ui, cydui_theme_font(cydui_theme_of(ui), CYDUI_FONT_TITLE));
    cydui_next_color(ui, gold);
    cydui_label_wrapped(ui, text);
}
```

### The theme's own pointer, and painters built from rectangles
A painter gets the theme it belongs to, and the theme carries the application's data in `user`:

```c
/* ---- the application's own pointer in the theme ---- */
typedef struct { cydui_color gold, wood; } palette;
static const palette PARCHMENT = {0xFEA0, 0x8A22};

void set_up_the_theme(cydui_theme *t)
{
    t->user = &PARCHMENT;
}

void my_painter(cydui_ui *ui, const cydui_theme *theme, const cydui_paint_info *info)
{
    const palette *p = (const palette *)theme->user;

    cydui_paint_default(ui, theme, info);
    cydui_draw_rect(&ui->ctx, info->rect, p->gold, 64);
}
```

When a table cannot say what a widget looks like (a card with a gold bar on one side, an edge of two colors, a tab whose active state is the page itself) the painter is a few stacked rectangles, and the idiom that does most of it is an edge and a face, a rectangle inside a rectangle. Three to six commands per widget, and `cydui_paint_default` still draws the content:

```c
/* ---- the two-rectangle idiom: an edge and a face, three or four commands for a card ---- */
void boxed(cydui_ctx *c, cydui_rect r, int edge_w, cydui_color edge, cydui_color face)
{
    cydui_rect inner = {(int16_t)(r.x + edge_w), (int16_t)(r.y + edge_w), (int16_t)(r.w - 2 * edge_w), (int16_t)(r.h - 2 * edge_w)};

    cydui_draw_rect(c, r, edge, 255);
    cydui_draw_rect(c, inner, face, 255);
}
```

### The root margin is the theme's spacing
The margin of the root layout is `style.spacing`, the same number as the gap between widgets. A screen that goes edge to edge (tabs flush with the top, a status line flush with the bottom) sets `spacing = 0` for the whole theme and asks for a gap where it wants one:

```c
/* ---- the root margin is the theme's spacing ---- */
void edge_to_edge(cydui_theme *t, cydui_ui *ui)
{
    t->style.spacing = 0;      /* tabs flush with the top, a status line flush with the bottom ... */
    cydui_set_spacing(ui, 8);  /* ... and a gap where a container wants one */
}
```

### A target that is wide but short
`style.min_target` is 32 pixels, but a month of six rows in the 192 pixels under a tab bar leaves 22 per row. When the cells cannot be bigger, make the whole grid **one** target and work the cell out from where the finger was (`resp.x`, `resp.y` are the position of the press that this widget owns):

```c
/* ---- a wide but short target: one interaction over the whole grid ---- */
int cell_tapped(cydui_ui *ui, int cols, int rows)
{
    cydui_rect r = cydui_alloc_rect(ui, 320, 22 * rows);
    cydui_response resp = cydui_interact(ui, cydui_id_str(ui, "month"), r, CYDUI_SENSE_CLICK);

    if (!resp.clicked) {
        return -1;
    }
    return ((resp.y - r.y) / (r.h / rows)) * cols + (resp.x - r.x) / (r.w / cols);
}
```

### Watching the touches from outside the library
The backend is a struct of functions, so an application can wrap `poll_event` to learn when the screen was last touched (to dim the backlight after a minute, to start a screensaver) without the library knowing. The event goes on unchanged: the touch that wakes a dimmed screen is also a tap, and nothing is swallowed.

```c
/* ---- observing touches from outside the library ---- */
static cydui_backend real_backend;
static uint32_t last_touch_ms;

static bool poll_noting_touch(void *user, cydui_event *ev)
{
    bool got = real_backend.poll_event(real_backend.user, ev);

    (void)user;
    if (got && ev->type == CYDUI_EVENT_DOWN) {
        last_touch_ms = real_backend.now_ms(real_backend.user);
    }
    return got;
}

void wrap_the_backend(const cydui_backend *be, cydui_backend *to_give_to_the_ui)
{
    real_backend = *be;
    *to_give_to_the_ui = *be;
    to_give_to_the_ui->poll_event = poll_noting_touch; /* the event goes on unchanged: the touch that wakes the screen is also a tap */
}

bool idle_for(uint32_t ms)
{
    return real_backend.now_ms(real_backend.user) - last_touch_ms >= ms; /* unsigned: safe when the clock wraps */
}
```

### Animating once per occurrence
`cydui_animate` moves only when its target **changes**, which is why nothing animates when a list loads or a card scrolls into view. An effect that should happen once each time an event happens (a toast that fades out, a flash when a level is gained) has no target that changes, so salt the id with a counter of your own: every occurrence is a new tween that starts at `from`, and 0 can mean "never", so nothing starts at boot.

```c
/* ---- animating once per occurrence ---- */
static uint16_t toast_seq; /* the application counts its toasts; 0 = none yet */

void show_toast(void)
{
    toast_seq++;
}

int toast_alpha(cydui_ui *ui, bool fading)
{
    int a;

    cydui_push_id_int(ui, toast_seq); /* each toast is a new tween, which starts at `from` */
    a = (int)cydui_animate(ui, cydui_id_str(ui, "fade"), 255, fading ? 0 : 255, 300, CYDUI_EASE_LINEAR);
    cydui_pop_id(ui);
    return a;
}
```

### A loop that sleeps
`cydui_ui_end` returns true while anything is still moving or pending (a tween, a press, a scroll coasting, a queued event, a layout that is settling). When it returns false nothing will change until the next event, so the loop may sleep for as long as the platform allows (a quarter of a second is a good ceiling, to keep the timers of the application alive); an animation keeps the loop awake for exactly as long as it lasts.

```c
/* ---- a loop that sleeps ---- */
void run(cydui_ui *ui, void (*build)(cydui_ui *), void (*wait_for_an_event)(void))
{
    while (cydui_ui_begin(ui, 0)) {
        build(ui);
        if (!cydui_ui_end(ui)) {
            wait_for_an_event(); /* nothing moves and nothing is pending: sleep */
        }
    }
}
```
