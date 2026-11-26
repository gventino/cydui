# 05 — Threat Model (STRIDE)

## 1. Scope
- **In scope:** the `cydui` library (core, backends, tools) and the **usage profile** of a typical firmware (CYD + WiFi + HTTP backend in the homelab). Network/storage controls belong to the app, but the library must not get in the way and should provide safe hooks.
- **Out of scope:** advanced physical security (chip decapping), vulnerabilities in the ESP-IDF/WiFi stack, security of the homelab itself.

## 2. Assets
| ID | Asset | Value |
|----|-------|-------|
| A1 | User data (tasks, calendar) | Privacy |
| A2 | Credentials (WiFi PSK, backend token) | Access to network/service |
| A3 | Firmware integrity | Trust in the device |
| A4 | UI availability | Usefulness |
| A5 | Integrity of what is displayed | Avoids misleading the user |

## 3. Data flow diagram and boundaries
```
 [User]──touch──▶(P1 Touch driver / event parser)
     ▲                          │
     │ pixels                   ▼
 [Display]◀──SPI/DMA──(P4 Rasterizer)◀──(P3 Command list)◀──(P2 UI core: IDs, state, layout, anim)
                                                                     ▲
                                                                     │ model data
 [HTTP Backend]◀──WiFi/HTTP(S)──▶(P5 App network client)──▶(P6 App model)
                                        │
                                  [DS1 NVS/Flash: SSID, token, calibration]
 [Dev host]◀──USB/UART──▶(P7 Bootloader/serial/log)

 Trust boundaries:
  TB1  Physical world ↔ firmware (touch, USB)
  TB2  Firmware ↔ network (WiFi)
  TB3  Firmware ↔ flash (external access via USB)
  TB4  App ↔ library (the app is "trusted", but its data is not)
```

## 4. STRIDE per component

### P1 — Touch driver / event parser (TB1)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| S | "Forged" touch from electrical interference (phantom clicks) | AB-04 | Medium |
| T | Out-of-range or NaN coordinates reach hit-testing | AB-05 | Medium |
| R | n/a (no action log on the device) | — | Low |
| I | n/a | — | — |
| D | Event flood fills the queue and hangs | AB-04 | Medium |
| E | n/a | — | — |

### P2 — UI core (IDs, state, layout, animation) (TB4)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| S | ID collision inherits another widget's state (action on the wrong item) | AB-06 | Medium |
| T | Dev/data corrupts the state table (invalid pointers, multi-task without lock) | AB-15 | Medium |
| R | n/a | — | — |
| I | Residual widget state (typed text) stays in RAM after recycling | — | Low |
| D | Too many items / deep tree / infinite tweens exhaust arenas or CPU | AB-07, AB-08 | **High** |
| E | Memory corruption from arena overflow → code execution | AB-01, AB-15 | Medium (high impact) |

### P3 — Command list
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| T | Command with invalid pointers/dimensions (bitmap, font) | AB-16, AB-17 | Medium |
| D | Command buffer overflow | AB-01, AB-07 | High |
| E | Out-of-bounds read/write in the raster | AB-17 | Medium (high impact) |

### P4 — Rasterizer / text / fonts
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| S | Text misleads the user (bidi, control characters, homoglyphs) | AB-03 | Medium |
| T | Malformed font/theme | AB-16 | Low (local artifacts, build-time) |
| I | Read past the glyph buffer exposes adjacent memory as pixels | AB-02 | Low |
| D | Huge text / quadratic line wrapping | AB-01 | High |
| E | OOB in UTF-8 decode | AB-02 | Medium (high impact) |

### P5 — App network client (TB2)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| S | Fake backend (DNS/ARP spoof) | AB-09 | **High** on a home LAN |
| T | Response tampered in transit (plain HTTP) | AB-10 | **High** |
| R | Backend cannot tell which device acted | — | Low |
| I | Sniffing of tasks/calendar and token | AB-10 | **High** |
| D | Huge response exhausts heap; slow backend blocks UI | AB-07 | Medium |
| E | Vulnerable JSON/HTTP parser | — | Medium |

### P6 — App model
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| T/S | Malicious task content (long text, control characters, bidi) reaches the UI | AB-01..03 | Medium |

### DS1 — NVS / flash (TB3)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| T | Rewriting calibration/config by someone with USB | AB-12 | Medium |
| I | Extracting SSID/token via `esptool read_flash` | AB-11 | **High** with physical access |
| D | NVS corruption prevents the app from booting | — | Low |

### P7 — Serial/log/bootloader (TB1/TB3)
| STRIDE | Threat | Ref | Level |
|--------|--------|-----|-------|
| T/E | Flashing malicious firmware | AB-12 | High with physical access (accept or mitigate with secure boot) |
| I | Logs with sensitive data | AB-13 | Medium |
| R | No audit trail of who flashed firmware | — | Low |

## 4b. Review against the implementation (iteration 8)

The tables above were written before the code. This pass walks the library as it is at 1.0.0-rc.1 and says, for each component, what the code does about each threat and where it is tested. "Fuzz" is `tests/fuzz_*.c` (libFuzzer with ASan/UBSan); the file names are in `06` §1b.

### P1 — Touch driver, event parser, keys (TB1)
| Threat | What the implementation does | Evidence |
|--------|------------------------------|----------|
| Phantom touch (S) | Pressure hysteresis and debounce (`z_on`/`z_off`, a number of ticks in a row), the mean of a burst without its extremes, a bouncing burst is thrown away, a jump is believed only if the next tick agrees. A destructive action needs a modal popup (CT-22). | `backends/esp32/touch_filter.c`, `test_touch_filter`, `fuzz_touch` (valid DOWN/MOVE/UP sequence for any readings and any configuration) |
| Bad coordinates (T) | The pointer is clamped to the screen in the UI; a calibration is refused when it is not finite, degenerate or a target is more than 12 px off, and a stored one needs its magic and CRC-32 (one changed byte is always refused). | `src/ui.c`, `touch_cal.c`, `test_touch_cal`, `fuzz_touch` |
| Flood (D) | 32 events, MOVE merged, oldest dropped, 64 polled and 16 processed per frame; a lost release cannot leave a press stuck. | `test_ui_input`, `fuzz_ui` |
| Typed characters and keys (new, AB-19) | Their own queue of 32 for one frame, oldest dropped, what is left is dropped at the end of the frame (no key is applied later). A character goes through the editing core: controls and bidi marks refused (CT-04), `\n` only in a text area, `cap` and `max_chars` respected, never cut inside a character. | `src/ui.c` (`k_push`), `src/textedit.c`, `test_textedit`, `fuzz_textedit`, `fuzz_ui` ops 56-57 |

### P2 — UI core (TB4)
| Threat | What the implementation does | Evidence |
|--------|------------------------------|----------|
| ID collision (S) | 32-bit FNV-1a with a 16-bit check from another function; `##` suffixes and `push_id` for lists; a same-frame collision is detected in debug builds (`CYDUI_ERR_ID_COLLISION`). The fuzzer treats a collision as an application error: two widgets with one id may fight over their state. | `test_ui_ids`, `fuzz_ui` |
| State table (T, I) | The table holds numbers only (cursor, scroll, deadlines, tween ends), never text: what a field edits is the buffer of the application. A password is drawn as bullets (a test pins that the command holds only bullets). `cydui_ui_init` clears the table it is given (found on the board in It7: a second UI over the same storage inherited tweens). The active widget is never evicted. | `src/ui.c`, `test_anim`, `test_ui_text` |
| Exhaustion (D) | Caps: 512 widgets per frame, 8 nested ids, layout depth, 16 tweens moving at once and 5 s per tween (`CYDUI_ERR_TWEEN_LIMIT`), 320 or so commands by the application's choice. A tween only takes a free or an expired state entry and uses its own salted id, so it can neither replace the state of a scroll area nor fight the look of a widget (both found by the fuzzer in It7). `cydui_ui_end` returns false when nothing moves so the loop sleeps (0.05 % CPU idle on the board). | `test_anim`, `fuzz_ui`, `GALLERY_TWEENS` on the board |
| Focus and popup (new, AB-22) | The focus is one id. It is taken by a press on a field or by an explicit `cydui_focus_set` of the application, never by a widget that is merely called; it is dropped by a press elsewhere, by Enter on a single line, by Escape, and when the field is not called for a frame. A popup captures the touch from everything outside it (through the previous frame's rectangle); a modal one is closed only by the application, a non-modal one also by a press outside or Escape. One popup at a time. | `src/ui.c`, `src/popup.c`, `test_ui_popup`, `test_ui_text`, `fuzz_ui` ops 58-59 |
| Memory corruption (E) | Every arena is of a fixed size given at init and drops what does not fit; no `malloc`, no mutable globals in the core (checked by `ci.sh` with `nm`). | `test_ctx`, `test_arena`, `ci.sh` |

### P3 — Command list, P4 — Rasterizer, text, fonts
| Threat | What the implementation does | Evidence |
|--------|------------------------------|----------|
| Bad command (T) | Geometry is clipped, a bitmap needs `len` for its whole `w x h` (multiplication overflow checked), text is copied into the frame's arena and cut at `CYDUI_MAX_STR` on a character boundary, a font is validated and its glyph access is bounds-checked. | `test_ctx`, `test_font`, `fuzz_cmds` |
| Out of bounds in the raster (E) | Everything is drawn inside the strip and the clip; the fast paths of It7 are compared pixel by pixel with the plain rasterizer on 20 000 random strips in every run. | `test_raster_equiv`, `fuzz_cmds`, `fuzz_ui` (three strip heights, one or two buffers, dirty regions: the picture must be the same) |
| UTF-8 (E, I) | Strict decoder: overlong, surrogates, above U+10FFFF and truncated sequences are one replacement character; it never reads past `len`. | `test_utf8`, `fuzz_utf8` |
| Spoofing with text (S) | Control characters and bidi marks are drawn as a box; homoglyphs are not handled (R-21). | `test_text` |
| Cost of a hostile frame (D, AB-20) | Work is bounded by the commands that fit (`cmd_cap`) times the pixels of the screen. Measured on the PC at `-O2`: 320 full-screen translucent rectangles 23 ms, 320 full-screen rounded rectangles with outlines 118 ms, 320 gradients 104 ms; the board is about 40 times slower, so the worst imaginable frame is a few seconds, not a hang: `flush_strip` feeds the task watchdog for every strip. Widgets draw small rectangles, so only a bug of the application can reach it; there is no time budget inside the rasterizer (a decision, see the residual risks). | `09` (measure), `cydui_esp32.c` |

### P8 — Code the library runs for the application (new, AB-21)
A post-effect (`cydui_post_effect`), a painter (`cydui_paint_fn`), a backend and a custom widget are called by the library with pointers into its memory (a strip, the command list, the UI). They are **trusted**: nothing stops a post-effect from writing past its strip. What the library does is to hand over exact geometry (strip pointer, width, rows in screen coordinates), to require that an effect depend on the absolute row (so the picture does not depend on the strip height), and to test its own effects with ASan (`fuzz_ui` runs a clock-driven effect in some themes and aborts if the geometry is wrong). A theme is validated when it is set (a missing font is refused, a NULL painter means the default), and `cydui_set_theme` keeps the old theme on error.

### Review of the 1.0.0-rc.2 additions
- **`CYDUI_BITMAP_A1` and `cydui_draw_bitmap_a1`** (memory corruption from hostile input): the scale is 1 to 8 and the scaled size must fit an int16 (else `CYDUI_ERR_BAD_ARG`, command dropped); the data must cover `((w + 7) / 8) * h` bytes (else `CYDUI_ERR_BAD_BITMAP`, dropped), so the rasterizer, which reads only inside the visible part of the rectangle, never reads past it. `tests/test_bitmap_a1.c` (every error, the 32768-pixel edge), `test_raster_equiv` (20 000 random strips against the plain reference, A1 included), `fuzz_cmds` (the format and wrong scales and lengths).
- **`cydui_text_fit` and `cydui_text_ellipsize`** (DoS, overflow): both apply the `CYDUI_MAX_STR` limit, take about log2(n) measurements, never cut inside a character, and `cydui_text_ellipsize` never writes past `cap` and always terminates (`tests/test_text_fit.c`: every cap from 1 to 23 with guard bytes, 3000 random strings).
- **`cydui_ui_stats_*`, `CYDUI_ERR_STATE_FULL`** (information, DoS): numbers only, no pointers, no text; the new flag makes a silently lost state visible.
- **`cydui_next_font/color/alpha`, `theme.font_small/font_title`, `theme.user`**: a font that cannot be used is refused by `cydui_theme_validate` (the optional ones too) or ignored by the modifier, so a bad font never reaches the rasterizer; `theme.user` is a pointer the library never dereferences.

### Findings of the review
Fixed during It7 and It8: the state table was not cleared at init; a tween could replace other state or fight the look of a widget; the keyboard label table was writable data (NFR-21 check); one undocumented public function (`cydui_id_eq`); the fuzz harness lost a release when the backend queue was full and demanded a settled screen after an application id collision. Documented, not changed: the cost of a hostile frame above (no time budget in the rasterizer: it would make the picture depend on the speed of the machine, against NFR-30), a password lives in the application's buffer which the library does not wipe, plug-ins are trusted.

## 5. Prioritized threats
1. **High:** DoS from arena/CPU exhaustion (P2/P3/P4), app network without TLS/authentication (P5), credential extraction from flash (DS1).
2. **Medium / high impact:** memory corruption from hostile input (UTF-8, bitmap, arena overflow).
3. **Consciously accepted:** full physical access without secure boot (document it), visual spoofing with homoglyphs (partial mitigation: filter control/bidi).

Controls and residual risks are in `06-controls-and-risks.md`.
