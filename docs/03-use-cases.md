# 03 — Use Cases

## Actors
- **LibDev** — maintains the `cydui` library.
- **AppDev** — writes firmware using the library (e.g. the todo app).
- **User** — person touching the CYD screen.
- **Backend** — HTTP service in the homelab (external actor, only relevant to the app and the threat model).

## Overview
```
AppDev ── UC-01 Create a minimal app
       ── UC-02 Switch theme (build/runtime)
       ── UC-03 Create a new theme
       ── UC-04 Add a font
       ── UC-05 Run on PC (SDL)
       ── UC-06 Flash and run on the CYD
       ── UC-07 Create a custom widget
LibDev ── UC-08 Run tests / goldens
User   ── UC-09 Interact (tap/drag/scroll)
       ── UC-10 Type text
       ── UC-11 Calibrate touch
Backend ─ UC-12 Update data and UI reflects it
```

## Descriptions

### UC-01 Create a minimal app
- **Actor:** AppDev. **Pre:** library installed as a component/CMake target.
- **Flow:** (1) create a backend (`cydui_backend_sdl_new` or `_esp32_new`); (2) `cydui_init(ctx, backend, theme, arenas)`; (3) loop: `cydui_begin_frame` → call widget functions → `cydui_end_frame` (rasterize + flush).
- **Post:** window/screen shows the widgets. **FR:** 01, 20, 60. **NFR:** 40.

### UC-02 Switch theme
- **Flow:** `cydui_set_theme(ctx, &theme_cyberpunk)`; the next frame uses the new theme; animation state is kept or reset depending on a flag.
- **Alternate:** invalid theme (required field null) → returns an error and keeps the current one. **FR:** 50–52.

### UC-03 Create a new theme
- **Flow:** copy `themes/clean.c`; change colors, radii, fonts; optionally provide custom `paint_*` functions using the primitives; build; view in `theme-gallery`.
- **Post:** no core file modified. **NFR:** 43.

### UC-04 Add a font
- **Flow:** `python tools/fontgen.py --ttf X.ttf --size 14 --chars "A-Za-z0-9éï" --out font_x14.c`; include the `.c`; reference it in the theme. **FR:** 10, 13.
- **Alternate:** missing glyph → renders the fallback glyph (□).

### UC-05 Run on PC
- **Flow:** `cmake -B build && cmake --build build && ./build/examples/widgets`. Mouse simulates touch. `--headless --hash` mode prints the framebuffer hash. **FR:** 61. **NFR:** 41.

### UC-06 Flash and run on the CYD
- **Pre:** user in the `uucp` group, ESP-IDF active. **Flow:** `idf.py -p /dev/ttyUSB0 flash monitor`. **Alternate:** port busy / board not in boot mode → esptool error, instructions in the README. **FR:** 62, 63.

### UC-07 Create a custom widget
- **Flow:** AppDev uses `cydui_alloc_rect`, `cydui_interact(id, rect)` and the drawing primitives; returns state. **FR:** 36.

### UC-08 Run tests
- **Actor:** LibDev. `ctest` runs unit tests, goldens and short fuzzing under ASan/UBSan. A golden failure shows a PNG diff. **NFR:** 22, 23, 30, 31.

### UC-09 Interact
- **Flow:** User touches a button → `down` (*pressed* state) → `up` inside the rect → `tap` → the widget function returns `true` in that frame.
- **Alternates:** drags outside before release → click canceled; drag over a list → scroll with inertia; long press → `long_press` event. **FR:** 33, 40, 41.

### UC-10 Type text
- **Flow:** touch a `text_input` → focus → virtual keyboard appears (popup) → touches generate characters → `Enter` confirms.
- **Alternate:** length limit reached → extra characters ignored and signaled visually. **FR:** 34, 35.

### UC-11 Calibrate touch
- **Flow:** first boot or reserved gesture → screens with targets (3–5 points) → compute matrix → save to NVS.
- **Alternates:** inconsistent points (high residual) → repeat; NVS unavailable → use defaults and warn. **FR:** 42, 43.

### UC-12 Update data and UI reflects it (app)
- **Flow:** network task fetches JSON → copies validated data into the app model (with limits) → marks "dirty" → next frame the UI draws the new data.
- **Alternate:** backend unreachable → UI shows "offline" state and the last cached data. **Relevant to:** `06-controls-and-risks.md`.
