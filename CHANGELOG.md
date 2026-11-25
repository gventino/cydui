# Changelog

Versions are tagged per iteration. Until 1.0 a new minor version may change the API; every change is listed under "API changes" (see `docs/11-api.md`).

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
