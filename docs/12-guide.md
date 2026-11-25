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

`cydui_theme_derive_table` builds the table from the palette (pressed = darker, disabled = faded); after it you can set single entries. `cydui_theme_validate` (run by `cydui_ui_init` and `cydui_set_theme`) refuses a theme without a usable font and keeps the one in use.

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
