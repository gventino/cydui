# 02 — Non-Functional Requirements

All must be **measurable**; the "Verification" column says how.

## Performance
| ID | Requirement | Verification |
|----|-------------|--------------|
| NFR-01 | ≥ 30 FPS with a full-screen animated UI (cyberpunk theme) on the CYD at 40 MHz SPI; ≥ 60 effective FPS on a static UI using dirty regions (only spends time when something changes). | FPS counter in the `theme-gallery` example, serial log. |
| NFR-02 | Touch→visual feedback latency < 50 ms (p95). | Input event timestamp vs. flush of the affected strip. |
| NFR-03 | UI pass (command generation) for a ~40-widget screen < 3 ms on ESP32 @240 MHz. | Micro-benchmark with `esp_timer`. |
| NFR-04 | Idle CPU use when nothing changes: loop sleeps (no redraw, no busy polling). | `vTaskGetRunTimeStats` measurement. |

## Memory
| ID | Requirement | Verification |
|----|-------------|--------------|
| NFR-10 | Library heap ≤ 40 KB (widget state + command list) + DMA strips (2 × 320 × H × 2 B; H=20 → 25.6 KB). | `heap_caps_get_free_size` before/after `cydui_init`. |
| NFR-11 | **Zero `malloc` after `cydui_init`.** All memory comes from caller-provided arenas. | Test that replaces `malloc` with abort after init (SDL) + review. |
| NFR-12 | Overflow of any arena degrades safely (drops the command/widget, sets an error flag, never corrupts memory). | Unit tests with tiny arenas; fuzzing. |
| NFR-13 | Enough heap left for the app: WiFi + TLS + JSON ≥ 80 KB free in the todo example. | Measurement in the app. |

## Portability and code quality
| ID | Requirement | Verification |
|----|-------------|--------------|
| NFR-20 | Core in **C99**, with no dependency on ESP-IDF, SDL or libc beyond `string.h`, `stdint.h`, `stddef.h`, `stdbool.h`, `math.h` (optional). | The local checks build the core with `-std=c99 -pedantic -Wall -Wextra -Werror` on gcc and clang. |
| NFR-21 | No mutable globals in the core (everything lives in `cydui_ctx`); allows multiple instances in tests. | Review + `nm` showing no core `.data/.bss` symbols. |
| NFR-22 | No ASan/UBSan findings in the test suite and SDL backend fuzzing. | Local checks. |
| NFR-23 | Core unit test coverage ≥ 80% of lines (gcov). | Local checks. |

## Determinism and testability
| ID | Requirement | Verification |
|----|-------------|--------------|
| NFR-30 | Same events + same injected clock → bit-identical framebuffer across runs and between SDL and ESP32 (shared rasterizer). | Golden hashes. |
| NFR-31 | Each theme has a golden-image test per widget and state. | Local checks. Met for `clean` and `clean_dark` in It4 (one golden per kind, light and dark in the same frame). |

## Developer usability
| ID | Requirement | Verification |
|----|-------------|--------------|
| NFR-40 | A "hello" app in ≤ 25 lines of C. | Example. |
| NFR-41 | Edit→see cycle on PC < 5 s (incremental build + SDL window). | Timing. |
| NFR-42 | Every public function documented (Doxygen-style comment), with an example for widgets. | Local check (script). |
| NFR-43 | Creating a new theme: copy a base theme and change the table; no core changes. | `retro` and `cyberpunk` are built this way (a table, plus painters for the 3D edges and the glow, and a post-effect for the scanlines): no change in the core. `clean` and `clean_dark` are one function over a palette. |

## Status after It5 (measured on the board; details in `09-hardware-notes.md`)
NFR-01 (It7): the renderer draws a full frame in 11.5 ms (it was 26), so a full redraw is bound by the SPI: 26 fps at 40 MHz (34.5 ms to send) and, for `cyberpunk` (17.5 ms of drawing with its glow and scanlines), 25 fps at 40 MHz and 35 fps at 80 MHz; a full-screen redraw at 30 fps or more needs 80 MHz. What an animation changes is a few strips: 89 fps (`cyberpunk`) and 99 fps (`clean`) at 40 MHz, 124 / 162 at 80 MHz. The requirement is met for animation with dirty regions at 40 MHz, and for a full-screen one only at 80 MHz (the 40 MHz limit is the SPI, not the library). NFR-02: p95 24 ms with the stylus. NFR-03: 0.7 to 1.5 ms. NFR-04: 0.12 % CPU idle, no strip sent. NFR-10: 37 KB static. NFR-13: 305 KB of free heap before Wi-Fi (not measured with Wi-Fi). NFR-50: as measured in It4. NFR-60 and 61: by timeouts, error returns and review; the hardware-disconnected test of NFR-60 was not done.

## Footprint
| ID | Requirement | Verification |
|----|-------------|--------------|
| NFR-50 | Flash for core + widgets ≤ 60 KB; each theme ≤ 10 KB excluding fonts; fonts sized by subset. | `idf.py size-components`. Measured on Xtensa `-Os` at the end of It4: the library is 23.6 KB, both `clean` themes 1.3 KB (a table of 1 KB is built in RAM per theme). |

## Operational robustness
| ID | Requirement | Verification |
|----|-------------|--------------|
| NFR-60 | Touch/display failure does not hang the firmware: init returns an error, the app decides. | Test with hardware disconnected. |
| NFR-61 | No `while(1)` without a yield in backends; task watchdog fed by the main loop. | Review. |

## Compatibility
| ID | Requirement | Verification |
|----|-------------|--------------|
| NFR-70 | Primary target: ESP32-2432S028R (ILI9341 + XPT2046). Variants (ST7789, color inversion, touch on a separate bus) via Kconfig. | Test on the user's board; other variants best-effort. |
| NFR-71 | ESP-IDF **v5.5.x** (pinned to v5.5.6 in `08-tech-stack.md`). | Local build. |
| NFR-72 | Permissive license (MIT) to ease distribution. | `LICENSE` file. |
