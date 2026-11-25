# 06 — Controls and Risks

## 1. Controls
Type: **P** preventive, **D** detective, **C** corrective/safe degradation. Owner: **Lib** or **App** (responsibility of the firmware using the library).

| ID | Control | Type | Owner | Mitigates | Test/Evidence |
|----|---------|------|-------|-----------|---------------|
| CT-01 | All arenas (commands, state, text) have a fixed size given at init; overflow **drops** the item, sets `ctx->err` and never writes out of bounds. | P/C | Lib | AB-01, 07, 15; STRIDE D/E in P2/P3 | Unit test with 1-item arenas; call-sequence fuzzing. |
| CT-02 | Configurable hard limits: max string length per widget (`CYDUI_MAX_STR`, default 256 B), layout depth, widgets per frame. Longer strings are truncated at a codepoint boundary. | P | Lib | AB-01, 07 | Unit; fuzz. |
| CT-03 | **Strict** UTF-8 decoder: rejects overlong, surrogates, > U+10FFFF; truncated sequence → U+FFFD; never reads past `len`. | P | Lib | AB-02 | UTF-8 test vector suite + fuzz. |
| CT-04 | Filter of control characters and bidi formatting (U+202A–202E, U+2066–2069) by default at render (can be disabled); rendered as `□`. | P | Lib | AB-03 | Unit. |
| CT-05 | Fixed-size input event queue, with `move` coalescing and oldest-drop when full; max rate per frame. | P/C | Lib | AB-04 | Flood test. |
| CT-06 | Coordinate validation (clamp to screen size) and calibration matrix checks (finite, determinant ≠ 0, residual < threshold) before saving/using. | P | Lib | AB-05 | Unit. |
| CT-07 | IDs: 32-bit hash + mandatory `push_id` in lists (documented); in debug builds, same-frame collision detection with logging. The table also stores a secondary check hash to reduce cross-wiring. | P/D | Lib | AB-06 | Unit with forced collision. |
| CT-08 | Animations: fixed max active tweens; max duration; recycling of state not seen for N frames. The app loop sleeps when nothing is dirty/animating. | P | Lib | AB-08 | Tween count test; idle CPU measurement. |
| CT-09 | Validation at registration: themes/fonts with null required fields are rejected in `set_theme`/`init`. Optional paint functions fall back to defaults. | P | Lib | AB-16 | Unit. |
| CT-10 | Bitmap: `w*h*bpp` checked against the provided `len` before rasterizing; multiplication overflow checked. | P | Lib | AB-17 | Unit + fuzz. |
| CT-11 | Threading contract: the library API is **not thread-safe**; call it only from the UI task. The app passes data through a queue/copy. Debug assert if called from another task. | P | Lib | AB-15 | Doc + assert. |
| CT-12 | SDL backend and tests built with `-fsanitize=address,undefined`; fuzzing (libFuzzer) of: UTF-8, command parser, event sequences, `text_input`. | D | Lib | all memory-related | Local checks. |
| CT-13 | ESP32 build with `-fstack-protector-strong`, `-Wformat-security`, `CONFIG_COMPILER_STACK_CHECK_MODE_STRONG`, heap poisoning in debug. | P/D | Lib/App | AB-01, 15 | Config in `sdkconfig.defaults`. |
| CT-14 | ESP32 backend Kconfig validates pins at compile time: rejects flash pins (6–11) and input-only pins (34–39) used as outputs. Strapping pins (0, 2, 5, 12, 15) are accepted for the classic CYD board profile and produce a warning for any other profile, since the CYD wires its display to GPIO 2, 12 and 15. | P | Lib | AB-18 | `_Static_assert` in `hw-probe`; same check in the It5 backend. |
| CT-15 | Task watchdog registered in the UI loop; frame time budget (aborts the UI pass if exceeded). | D/C | Lib | AB-07, 08 | Load test. |
| CT-16 | Network (app): HTTPS with certificate validation/pinning of the homelab CA **or** HMAC token + TLS-PSK; never plain HTTP with a token. | P | App | AB-09, 10; STRIDE S/T/I in P5 | Test with MITM proxy. |
| CT-17 | Network (app): response size limits (e.g. 16 KB), timeouts, JSON parser with depth limit, schema validation and copy into the model with limits (CT-02). | P | App | AB-07; STRIDE D/E in P5/P6 | Response fuzzing. |
| CT-18 | Secrets (app): stored in NVS with **NVS encryption** and **flash encryption** (release mode) when the user accepts losing easy reflash; never in source/git; `sdkconfig` with secrets in `.gitignore`. | P | App | AB-11; STRIDE I in DS1 | Release checklist. |
| CT-19 | Secure Boot v2 (optional, release) to prevent unsigned firmware. | P | App | AB-12 | Release checklist; documented as optional. |
| CT-20 | Logs: default level `WARN` in release; library log macros never print user string content; never tokens. | P | Lib/App | AB-13 | Review grep. |
| CT-21 | Screen: configurable backlight timeout and a "privacy mode" (hide content) in the app. | P | App | AB-14 | Functional. |
| CT-22 | Destructive UI actions in the app require confirmation (popup) or long press. | P | App | AB-04 | App flow. |

## 1b. Implementation status

| Control | State | Where |
|---------|-------|-------|
| CT-01 | done for the command list, the text arena and bitmaps | `src/ctx.c`, `tests/test_ctx.c`, `tests/test_text.c` |
| CT-02 | done: text (`CYDUI_MAX_STR`, cut on a character boundary), the string of a text field (the `cap` of its buffer, an optional number of characters, never cut inside a character, `\n` only in a text area), the lines of a text area (16 rows shown, any length of text), layout depth (8), id depth (8), widgets per frame (512, also for widgets of the application through `cydui_widget_begin`), list rows (`CYDUI_LIST_MAX_PX`), tabs (8), each raising its flag and degrading safely | `src/text.c`, `src/layout.c`, `src/widgets.c`, `src/scroll.c` |
| CT-03 | done | `src/utf8.c`, `tests/test_utf8.c`, `tests/fuzz_utf8.c` |
| CT-04 | done, with a wider set than written (adds C0/C1 controls, U+061C, U+200E/200F); always on for controls, `CYDUI_FILTER_CONTROL` for the bidi marks | `src/text.c` |
| CT-05 | done: queue of 32 events with MOVE merging and oldest-first dropping, 64 polled and 16 processed per frame; a flood and overflowing bursts leave the pointer state right | `src/ui.c`, `tests/test_ui_input.c`, `tests/fuzz_ui.c` |
| CT-06 | done: coordinates are clamped to the screen; the calibration is refused when the matrix is degenerate or a target is further than 12 px from where it falls, and a stored one is checked by magic and CRC32 | `src/ui.c`, `backends/esp32/touch_cal.c`, `tests/test_touch_cal.c` |
| CT-07 | done: id with a separate 16-bit check, `##` suffixes and `push_id`, same-frame collision detection in debug builds, the active widget never evicted | `src/ui.c`, `tests/test_ui_ids.c` |
| CT-09 | done: fonts (`cydui_font_validate`, bounds-checked glyph access, unusable font rejected) and themes (`cydui_theme_validate`; `cydui_set_theme` refuses an invalid one and keeps the current; a NULL painter means the default, kinds and states outside the table mean the plain one) | `src/font.c`, `src/theme.c`, `tests/test_theme.c` |
| CT-10 | done | `src/ctx.c` |
| CT-11 | done: not thread-safe by contract, debug check through `thread_id` (the ESP32 backend supplies the task handle in It5) | `src/ui.c`, `tests/test_ui_ids.c` |
| CT-12 | done for commands, text, the UTF-8 decoder and the whole UI layer (events, themes built from the input, scroll areas and lists, alignment, cards, the custom widget); text fields and text areas on buffers of any size (or none, with no NUL), typed characters of any code point, keys that do not exist, popups left open and the keyboard on the screen (It6, also `fuzz_textedit` for the editing core). The undefined-behavior sanitizer is fatal (`-fno-sanitize-recover=undefined`). | `tests/fuzz_*.c` |
| CT-22 | the library gives what the application needs: `cydui_popup_begin(..., CYDUI_POPUP_MODAL)` is a confirmation that nothing under it can bypass (the touch is taken from the rest of the screen and Escape does not close it) | `src/popup.c`, `tests/test_ui_popup.c` |
| CT-13 | done: strong stack protector, comprehensive heap poisoning and `-Wformat-security` in the gallery firmware, checked by the local `ci.sh` | `examples/theme-gallery/idf/sdkconfig.defaults` |
| CT-14 | done: the Kconfig rejects pins that are not usable (`pins.h`, tested) | `backends/esp32/pins.h`, `tests/test_pins.c` |
| CT-15 | done: the task watchdog is subscribed and fed every frame; the frame budget was proved on the board | `backends/esp32/cydui_esp32.c` |
| CT-08 | It7 | `tests/test_anim.c`: the cap of 16 tweens, the cut of the duration, a clock that wraps or goes back, no tween replaces other state, states recycled (3000 frames, no growth); on the board 1500 frames over all the pages of `cyberpunk` keep 14 state entries and 2 tweens at most, and the loop settles (`GALLERY_TWEENS`); idle 0.05 % CPU, no strip sent |
| CT-16..22 | owned by the app | |

## 2. Threat → control traceability
| Abuse | Controls |
|-------|----------|
| AB-01 | CT-01, 02, 12, 13 |
| AB-02 | CT-03, 12 |
| AB-03 | CT-04 |
| AB-04 | CT-05, 22 |
| AB-05 | CT-06 |
| AB-06 | CT-07 |
| AB-07 | CT-01, 02, 15, 17 |
| AB-08 | CT-08, 15 |
| AB-09 | CT-16 |
| AB-10 | CT-16 |
| AB-11 | CT-18 |
| AB-12 | CT-19 |
| AB-13 | CT-20 |
| AB-14 | CT-21 |
| AB-15 | CT-01, 11, 12, 13 |
| AB-16 | CT-09 |
| AB-17 | CT-10, 12 |
| AB-18 | CT-14 |

## 3. Risk register
Probability (P) and impact (I): 1–3. Score = P×I. Residual after controls.

### Technical risks
| ID | Risk | P | I | Score | Response | Residual |
|----|------|---|---|-------|----------|----------|
| R-01 | Insufficient RAM (no PSRAM) with WiFi+TLS+UI. | 2 | 3 | 6 | Strip rendering, fixed arenas, measure early in It5; smaller strips (H=10) if needed. | Low |
| R-02 | FPS < 30 in the cyberpunk theme with post-effects. | 3 | 2 | 6 | SPI at 40–80 MHz, dirty regions, cheap effects (LUT), per-strip budget, downgrade effects via Kconfig. **Measured in It5:** drawing a full frame costs ~26 ms on the board (the SPI at 40 MHz another 34 ms, overlapped), a full redraw is 24 / 30 fps; with dirty regions an animation is 83 / 104 fps. **After the optimization of It7:** drawing a full frame costs 11.5 ms and a full redraw is bound by the SPI (34.5 ms at 40 MHz, 26.8 fps); `cyberpunk` draws in 17.5 ms (the scanlines cost 1.3 ms) and a full redraw is 25 fps at 40 MHz and 35 fps at 80 MHz; an animation with dirty regions 89 fps (40 MHz) and 124 fps (80 MHz). The effects can be turned down with `cydui_set_effects_level`. | Medium |
| R-03 | Panel/touch clones with a different native orientation, color inversion, RGB/BGR order, or touch noise. **Materialized in It0:** the panel is natively 320x240 (ILI9342C-like), the controller ID cannot be read, and the touch axes are swapped. | 3 | 2 | 6 | Kconfig for raw `MADCTL`, native orientation, inversion; touch axis swap and calibration in the backend; findings in `09-hardware-notes.md`; re-run `hw-probe` on any new board. | Medium |
| R-04 | Noisy/imprecise resistive touch. | 3 | 2 | 6 | 5-point calibration, median, debounce, widgets with targets ≥ 32 px. | Low |
| R-05 | Immediate-mode layout without retention becomes costly or unstable (auto size). | 2 | 2 | 4 | Single-pass layout + one-frame delay; limit auto-size to simple cases. **Done:** positions are final when placed, only `fill` uses last frame's numbers and asks for another frame; 40 widgets cost 9.6 µs on a PC. Real cost on the board (It5): 0.7 to 1.5 ms. | Low |
| R-06 | Tearing/flicker from partial flush with DMA. | 2 | 2 | 4 | Double buffering, flush by whole strips; simple synchronization. | Low |
| R-07 | Generated fonts weigh on flash (many sizes/glyphs). | 2 | 2 | 4 | Per-app subset, 4bpp, shared identical bitmaps; NFR-50 budget. **Measured:** the three bundled fonts (ASCII + Latin-1 + symbols) take 45.7 KB, the whole core 6.8 KB. | Low |
| R-08 | Determinism gaps between SDL and ESP32 (floating point, operation order). | 2 | 2 | 4 | Integer/fixed-point raster; no `float` in the pixel path. **Tested in It7:** the easings, the effects and the themes with a clock give the same 42 hashes on the PC and on the board (the gallery takes them on a made-up clock). | Low |
| R-09 | ESP-IDF version drift breaking the backend. | 2 | 2 | 4 | Pin version in `08-tech-stack.md`; build check. | Low |

### Project risks
| ID | Risk | P | I | Score | Response |
|----|------|---|---|-------|----------|
| R-10 | **Scope explosion** (themes, widgets, effects) before the core is done. | 3 | 3 | 9 | KISS; per-iteration acceptance criteria; extra themes only in It7; freeze core API in It4 (done: `docs/11-api.md`, `tests/test_api_surface.c`). |
| R-11 | API too egui-like but unsuited to C (closures, builders). | 2 | 2 | 4 | Early `hello`/`widgets` prototype (It3), review ergonomics before expanding. **State:** prototype done (`hello` is 22 lines) and reviewed with the user in the It3 milestone: the API stays as it is. The API is frozen at the end of It4. |
| R-12 | Single hardware unit (no variants) hides bugs. | 2 | 1 | 2 | Accept; keep the SDL backend as reference. |
| R-13 | Fuzz/sanitizers consume time and become "later". | 2 | 2 | 4 | Fuzz UTF-8 and commands in It2/It3, not only It8. |

### Residual security risks (explicitly accepted)
| ID | Risk | Rationale |
|----|------|-----------|
| R-20 | An attacker with physical access and **no** secure boot/flash encryption can extract secrets and reflash. | Home device; CT-18/19 optional because of the reflash cost. Document in the README. |
| R-21 | Visual spoofing via Unicode homoglyphs (e.g. Cyrillic). | CT-04 only covers control/bidi; a limited font subset already reduces the surface. |
| R-22 | Vulnerabilities in the ESP-IDF WiFi/TLS stack. | Out of scope; keep ESP-IDF updated. |
