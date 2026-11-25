# Changelog

Versions are tagged per iteration. Until 1.0 a new minor version may change the API; every change is listed under "API changes" (see `docs/11-api.md`).

## 0.5.0 — iteration 5: the ESP32 backend

### Added
- The ESP32 backend (`backends/esp32/`): SPI + DMA with two strip buffers, PWM backlight, XPT2046 on a task of its own with a filter, a 5-point calibration kept in NVS, Kconfig for controller, pins, clock and rotation, the task watchdog, timeouts and the touch-to-picture latency (`cydui_esp32_*`, see `cydui_esp32.h`; this is not part of the frozen core API).
- `examples/theme-gallery` runs on the board (Touch and Animation pages, a guided acceptance test), with the hash of every page printed to compare with the PC (15 of 15 equal), and measurements of speed, memory and idle CPU in `docs/09-hardware-notes.md`.
- `cydui_config.strip_buffers` (2 = two strip buffers used in turn, so a backend can send a strip while the next one is drawn; the contract is in `backend.h`), `cydui_config.dirty_mem` and `cydui_invalidate` (dirty regions: a strip whose commands did not change is neither drawn nor sent), `cydui_ui_config.frame_budget_ms` and `CYDUI_ERR_FRAME_BUDGET` (a frame that takes too long skips the widgets that are left).
- Version macros are no longer frozen by value in `tests/test_api_surface.c` (only that they exist).

## 0.4.0 — iteration 4: themes, composite widgets, scrolling; the API is frozen

### API changes
- `cydui_ui_config.style` is now `cydui_ui_config.theme` (a `const cydui_theme *`); `cydui_style` moved to `theme.h`, `CYDUI_CONFIG(storage, backend, &theme)`. `cydui_theme_default(&t, font)` builds the old dark look.
- `cydui_state` grew from 24 to 32 bytes (`c` and `d` are new); the storage arrays computed with `sizeof(cydui_state)` follow by themselves.
- The cursor and origin of containers are signed (they may be negative inside a scrolled area).
- Fields of `struct cydui_ui`, `cydui_region` and `cydui_input` are declared private.
- The public API is now frozen by `tests/test_api_surface.c`.

### Added
- Themes: `cydui_theme` (palette, table of looks per widget kind, state and selection, optional painters), `cydui_set_theme`, `cydui_theme_validate`, `cydui_paint`, `cydui_wstyle_of`, soft shadows; the `clean` and `clean_dark` themes in `themes/`.
- Widgets: `cydui_slider`, `cydui_progress`, `cydui_tabs`.
- Scrolling: `cydui_scroll_begin/end`, `cydui_scroll_to`, virtual lists (`cydui_list_begin`, `cydui_begin/end_list_item`, `cydui_list_end`), kinetic scrolling with a scroll bar painted by the theme.
- Layout: `cydui_set_cross_align`, `cydui_set_main_align`, cards (`cydui_begin_card`/`cydui_end_card`).
- Writing widgets: `cydui_widget_begin`, `cydui_label_len`, `cydui_draw_text_in`, `cydui_state_of`, `CYDUI_CAPTURE`, and the pointer position (`x`, `y`) in `cydui_response`; `examples/theme-gallery/rating.c` as the example.
- `CYDUI_VERSION_*` and `cydui_version()`.
- `examples/theme-gallery`: all widgets in each theme with a run-time switch, a list of 200 tasks, a custom widget.

### Tunable defaults (not frozen)
- `CYDUI_SCROLL_FLING_MIN` 50, `CYDUI_SCROLL_MIN_V` 10, `CYDUI_SCROLL_FRICTION_MS` 325, `CYDUI_SCROLL_MAX_V` 6000; the feel is tuned on the board in iteration 5.

### Fixed
- Layout positions were clamped to 0, so everything after the first child above the top of a scrolled area landed in the wrong place.
- The sanitizer builds let undefined behavior print and continue; it now stops the program.

## 0.3.0 and earlier
Iterations 0 to 3: see `docs/07-iterations.md`. They were not tagged.
