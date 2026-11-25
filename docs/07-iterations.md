# 07 — Planned Iterations

Rule: an iteration only closes when its **acceptance criteria** pass. Each iteration lists the FR/NFR/CT it covers. No time estimates; size is "one focused work session" and may be split.

## It0 — Setup and "hello" on the CYD
- **Deliverables:** ESP-IDF installed and version pinned; `uucp` configured; `cydui` repo with CMake, `idf_component.yml`, `.gitignore`, `LICENSE` (MIT), README; local checks (an untracked `ci.sh` helper: host builds + tests); minimal firmware that fills the screen with colors and prints raw touch readings to the console.
- **Mandatory findings on the board:** real pins (SPI, backlight, touch), color inversion, RGB/BGR order, rotation, max stable SPI frequency.
- **Acceptance:** `idf.py flash monitor` works without sudo; screen shows red/green/blue in the right order; log shows raw x/y on touch; results recorded in `docs/09-hardware-notes.md`.
- **Status: done.** Findings in `docs/09-hardware-notes.md`. Deviation from the plan: the panel ID cannot be read back on this board, so the controller is selected by Kconfig instead of detected.
- **Covers:** FR-70, NFR-70/71, R-03, CT-14 (pins).

## It1 — Core + rasterizer + SDL backend
- **Deliverables:** basic types, arenas, command list, strip rasterizer with clipping (rect, antialiased rounded rect, antialiased line, bitmap, gradient), RGB565 blending; `cydui_backend` interface; SDL backend (window + headless/hash); golden-image harness.
- **Acceptance:** example draws shapes in the SDL window; goldens pass under ASan/UBSan; core compiles with `-std=c99 -pedantic -Werror`; no `malloc` after init.
- **Covers:** FR-01..04, 06, 60, 61; NFR-11, 12, 20, 21, 22, 30; CT-01, 10, 12.
- **Status: done.** Design and the numeric conventions are in `docs/10-core-design.md`. Deviations: `frame_end` added to the backend interface; the headless/hash mode lives in the separate `mem` backend instead of the SDL one.

## It2 — Text and fonts
- **Deliverables:** `tools/fontgen.py` (TTF→C 4bpp, subset); text rendering with measurement, alignment and optional wrapping; strict UTF-8 decoder; glyph fallback; UTF-8 fuzzing.
- **Acceptance:** Latin text with accents rendered at sizes 12/16/24 in SDL; invalid UTF-8 vectors cause no OOB; text golden passes.
- **Covers:** FR-10..13; CT-03, 04; R-07, R-13.
- **Status: done.** Details in `docs/10-core-design.md`. Deviations from the plan above:
  - the bundled fonts are named `cydui_sans_*`, not after Liberation Sans, because the OFL reserves that name for unmodified fonts; they stay under the OFL, separately from the MIT code;
  - the Unicode filter is wider than CT-04 (adds C0/C1 controls, U+061C, U+200E/200F);
  - FR-13 is partial: the mechanism works, no icon set is shipped;
  - the fuzz harness needed seed inputs (`tests/fuzz_seeds/`) to find bugs injected in the text code.

## It3 — Immediate mode, layout, input, basic widgets
- **Deliverables:** `begin_frame/end_frame`; IDs; state table; row/column/grid/spacer layout; tap/drag/long-press events; `label`, `button`, `checkbox`, `toggle`; `hello` example (≤ 25 lines).
- **Acceptance:** `widgets` example in SDL with mouse; headless test with synthetic events yields the expected hash; ID collision detected in debug; input queue survives flood.
- **Covers:** FR-20..24, 30, 40, 41; NFR-40; CT-05, 07, 11; R-05, R-11.
- **Milestone:** API ergonomics review with the user before moving on.
- **Status: done; API reviewed with the user.** The review kept the API as it is and settled two points: vertical centering in rows, group alignment and content-sized container backgrounds wait for It4 (they share one mechanism, knowing a container's final size before placing its children), and ids stay independent of the container hierarchy (lists use `##` or `push_id`). Details in `docs/10-core-design.md`. Deviations and decisions:
  - containers are `begin`/`end` pairs plus `CYDUI_ROW/COLUMN/GRID/PADDING` scope macros, and sizes are one-shot `cydui_next_*` modifiers (the two choices made with the user);
  - the gap goes before each child, so `cydui_set_spacing` is immediate;
  - FR-24 is implemented as last frame's `fill` numbers plus a request for another frame, no re-measuring pass;
  - the state table is searched linearly instead of hashed (a few dozen entries);
  - containers do not scope ids; lists use `##` or `push_id`;
  - cross-axis alignment is top-left only, and groups cannot be centered yet (It4 with cards and themes);
  - extra controls beyond the plan: CT-02 for layout depth, id depth and widgets per frame; the `CYDUI_STORAGE`/`CYDUI_CONFIG` macros that let `hello` fit in 22 lines.

## It4 — Themes v1 (clean) + composite widgets
- **Deliverables:** theme struct, states, per-widget paint override, runtime `set_theme`; `clean` theme (shadows, antialiased corners); `slider`, `progress`, `tabs`, `scroll_area`, list with kinetic scrolling; custom widgets.
- **Acceptance:** `theme-gallery` shows all widgets/states in `clean`; per widget×state goldens; runtime theme switch without leaks.
- **Covers:** FR-25, 31..33, 36, 50..52, 54 (partial); NFR-31, 43; CT-09.
- **Milestone:** **freeze the core API** (stable v0.x) — R-10.
- **Status: done; the API list was confirmed by the user and tagged `v0.4.0`.** The API is in `docs/11-api.md` and frozen by `tests/test_api_surface.c`; the design, the measurements and the lessons are in `docs/10-core-design.md`. Decisions and deviations from the plan above:
  - one iteration, built in eight steps (themes without changing a pixel; `clean`/`clean_dark` and the gallery; slider, progress and tabs; scroll with inertia and lists; alignment and cards; the custom-widget API proved by a rating widget; extended fuzz; the freeze);
  - `clean` has a light and a dark variant (`clean`, `clean_dark`), both one function over a palette; `retro` and `cyberpunk` remain for It7 (FR-54 is partial);
  - the scroll container is `cydui_scroll_begin/end` plus a virtual list (`cydui_list_*`), not a `cydui_scroll_area`; the inertia feel is tuned on the board in It5 (the `CYDUI_SCROLL_*` defaults are not frozen);
  - the three pending points of the It3 API review are done with one mechanism: vertical centering and group alignment (`cydui_set_cross_align/main_align`) and cards whose background is exactly the size of the content (`cydui_begin_card`);
  - writing a widget of one's own needed five more public pieces (found by building `rating` with the public headers alone): `cydui_widget_begin`, `cydui_label_len`, `cydui_draw_text_in`, `cydui_state_of` and the pointer position in `cydui_response`;
  - the 4-bit mask format (`CYDUI_BITMAP_A4`) is what the star uses, not an 8-bit one;
  - defects found on the way and fixed: positions clamped to zero in the layout (visible only in scrolled areas), a wrong golden that I had accepted, and a sanitizer configuration that let undefined behavior pass (now fatal);
  - a shadow is eight translucent rectangles and makes a card page 4 times as slow to render on the PC as a page of controls: to be judged on the board (R-02).
  - **versions:** a tag per iteration from now on (`v0.4.0`, `v0.5.0`, ...). The `v0.1.0` of It8 becomes a release candidate and `v1.0.0` is for after `cyd-todo` has been written on this API.

## It5 — ESP32 backend (ILI9341 + XPT2046)
- **Deliverables:** ESP32 backend with SPI+DMA double buffering, PWM backlight, filtered touch, NVS calibration, Kconfig, dirty regions; `theme-gallery` app on the board.
- **Acceptance:** board shows the gallery with the same hash/appearance as SDL (modulo panel); FPS and heap measured and recorded; ≥ 30 FPS on simple animation; accurate touch after calibration; NFR-10 met.
- **Covers:** FR-05, 42, 43, 62, 63; NFR-01..04, 10, 13, 60, 61; CT-06, 13, 14, 15; R-01, R-02, R-04, R-06.
- **Result:** done, numbers in `09-hardware-notes.md`. Same hash as the PC in 15 of 15 pages; animation above 30 fps with dirty regions; touch 4 px and p95 24 ms with the stylus. **Deviations:** (1) the speed of drawing is only measured, its optimization moves to It7 (a full redraw is 24 fps); (2) 80 MHz, rotation 180 and the touch with a bare finger were not checked on the hardware; (3) the hardware-disconnected test of NFR-60 was not run.

## It6 — Advanced text and popups
- **Deliverables:** `text_input` with cursor, virtual keyboard (alpha/num/symbols layouts), popup/modal; size limits.
- **Acceptance:** type and confirm text on the board; character limit respected; keyboard golden.
- **Covers:** FR-34, 35, 44; CT-02.
- **Result:** done, numbers in `09-hardware-notes.md`. Text field of one line (password mode) and text area, the editing core, popups, the keyboard on the screen (letters, numbers, symbols, accents), the keyboard of the computer in the SDL backend, the page "Texto" in the gallery; all 18 pages of the gallery have the same hash on the board as on the PC, and a self-test on the board types through the keys of the screen. **Deviations:** (1) the QR code and the web page for typing from a phone are left to the applications (Wi-Fi is not in the library; decided with the user); (2) FR-44 is done only for typing and editing keys: no Tab navigation and no BOOT button; (3) accents are a page of the keyboard, there is no long press; (4) the typing with real fingers on the board was not tried by the user: the self-test injects the touches (the layout was checked as pictures and the accuracy of the touch is the one of It5).

## It7 — Animation and retro/cyberpunk themes
- **Deliverables:** tweens and easings; injected clock; per-strip post-effect hook; `retro` theme (bitmap fonts, 3D borders, limited palette) and `cyberpunk` theme (glow, scanlines, glitch, entrance animations).
- **Acceptance:** `theme-gallery` switches among 3 themes at runtime in SDL and on the board; ≥ 30 FPS on cyberpunk (or documented configurable degradation); no tween growth.
- **Covers:** FR-07, 53, 54, 55; NFR-01; CT-08; R-02, R-08.

## It8 — Hardening and release candidate
- **Deliverables:** extended fuzzing (commands, events, text_input), STRIDE review against the implementation, the remaining API guide, finished examples, README with security limits, `CHANGELOG`, the release-candidate tag (`v1.0.0` only after `cyd-todo`).
- **Acceptance:** clean local checks (sanitizers + short fuzz); `06` checklist with all "Lib"-owned CT-xx ✔; README describes residual risks R-20..22.
- **Covers:** NFR-22, 23, 42, 50; all library CTs.

## Post-0.1 (separate repo)
- `cyd-todo`: todo/calendar app + lightweight backend in the homelab, applying CT-16..22 (TLS, response limits, secrets, privacy).

## Summary map
```
It0 setup/hw ─▶ It1 core+SDL ─▶ It2 text ─▶ It3 immediate/widgets ─▶ It4 themes v1 [API freeze]
                                                                           │
          It8 hardening ◀─ It7 animation/themes ◀─ It6 advanced text ◀─ It5 ESP32
```
