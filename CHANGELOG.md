# Changelog

Versions are tagged per iteration. Until 1.0 a new minor version may change the API; every change is listed under "API changes" (see `docs/11-api.md`).

## Unreleased

- Every text the user can see is in English: the gallery, the examples, the calibration screen of the ESP32 backend, the tests and the goldens, and the pictures of the README. Accented letters remain only as UTF-8 test data (Café, naïve, Zoë, Ångström, €). The board self-test types "Hi 1".

## 0.7.0 — iteration 7: a faster renderer, animation, post-effects and the `retro` and `cyberpunk` themes

### Added
- `cydui_animate` and the easing curves (`anim.h`), `cydui_now`, `cydui_theme_age`; `CYDUI_MAX_TWEENS`, `CYDUI_ANIM_MAX_MS`, `CYDUI_ERR_TWEEN_LIMIT`. Buttons, check boxes, switches, tabs and the dimming of a popup change their look over `cydui_style.anim_ms` (0 for every theme before).
- `cydui_post_effect` and `cydui_set_post_effect`: a hook per strip, with a key that takes part in the dirty regions; `cydui_theme.post`; `cydui_set_effects_level` (`CYDUI_FX_*`).
- `cydui_paint_look` and `cydui_paint_amount` for painters that want to take part in the transitions.
- The themes `retro` (silver, 3D edges, `cydui_mono_16`, a 1-bit monospaced font made from Liberation Mono: `fontgen.py --hinting mono`) and `cyberpunk` (neon, glow, scanlines, a glitch, an entrance sweep). The gallery has five themes (a tab bar instead of three buttons) and 30 hashes on the PC and on the board, plus 12 more at the times of the effects of cyberpunk; `--at MS` for the headless run.

### Changed
- The rasterizer is 3 to 10 times faster on the board and the output is the same, pixel by pixel (rounded rectangles by runs, spans filled by words, lines near the segment only, a glyph skips its empty bytes): a full frame draws in 11.5 ms instead of 26.
- The shadow of a card or a popup is four layers of alpha 16, not eight of alpha 8: the old one turned green over a dark or dimmed page (a test pins the new one). The goldens `clean_panel`, `clean_card` and `clean_popup` changed.
- `cydui_ui_init` clears the state table it is given. A theme set again with the same pointer is not a new entrance.

### API changes
- `cydui_style` (+2 bytes), `cydui_paint_info` and `cydui_theme` (+1 pointer) grew at their end; `cydui_ui` and `cydui_ctx` are bigger. `CYDUI_ERR_TWEEN_LIMIT` is a new bit.
- `cydui_ui_end` also returns true while a tween moves.
- `tools/gen_fonts.sh` makes one more font (`fonts/cydui_mono_16`); a theme that uses many widgets with a duration needs a state table that holds one more entry for each of them (a tween never replaces other state: with none free the value is the target at once).
- Not done: a pixel font with the check mark, a theme with entrance animations per widget (the entrance of cyberpunk is the sweep of the whole screen), a Kconfig default for the level of effects.

## 0.6.0 — iteration 6: text fields, popups and the keyboard on the screen

### Added
- `cydui_text_input` (one line, `CYDUI_TEXT_PASSWORD`) and `cydui_text_area` (several lines that wrap), `cydui_focus_set/_clear`, `cydui_has_focus`; `CYDUI_TEXT_CHANGED` / `CYDUI_TEXT_CONFIRMED`.
- `cydui_textedit_*` (`cydui/textedit.h`): the editing of a UTF-8 string in a buffer, with no UI.
- `cydui_popup_begin/_end` and `CYDUI_POPUP_MODAL`: a panel above everything that takes the touch while it is shown.
- The keyboard on the screen (letters, numbers, symbols, Latin-1 accents), shown by the library while a field has the focus; `cydui_keyboard_top`, `cydui_keyboard_enable`. A field inside a scroll area is scrolled to stay above it.
- `CYDUI_EVENT_CHAR`, `CYDUI_EVENT_KEY`, `cydui_key` and `cydui_event.key`; the SDL backend types with the keyboard of the computer; `CYDUI_KEY_QUEUE` (32).
- Theme kinds `CYDUI_WIDGET_TEXT_INPUT`, `CYDUI_WIDGET_POPUP`, `CYDUI_WIDGET_KEY`, filled by the default and both `clean` themes.
- The gallery has a page "Text" (18 hashes instead of 15 on the PC and on the board) and the board firmware a self-test that types on the keyboard of the screen.

### API changes
- `sizeof(cydui_event)` goes from 8 to 12 (the new field `key`); code that builds events with `{type, x, y}` still works.
- `cydui_theme` is bigger (three more kinds of widget, 360 bytes); `cydui_ui` too (a queue of keys and the state of the focus, the popup and the keyboard); the storage of an application that uses the keyboard needs about 120 more commands.
- The touch under a popup or under the keyboard does not reach the widgets there (`cydui_interact` returns nothing for them).
- Not done: Tab navigation and the BOOT button (FR-44 in part), long press for accents, the QR code / web page for typing from a phone (the applications).

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
