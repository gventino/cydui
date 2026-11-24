# 01 — Functional Requirements

Priority: **M** = must (v0.1), **S** = should, **C** = could.
Iteration = where the FR is delivered (see `07-iterations.md`).

## Core and rendering
| ID | Requirement | Pri | It |
|----|-------------|-----|----|
| FR-01 | The library exposes a `cydui_ctx` context initialized with a backend, a theme and caller-provided memory arenas. | M | 1 |
| FR-02 | Each frame, the UI emits a **draw command list** (rect, rounded rect, line, text, bitmap, simple gradient) into a fixed-size buffer. | M | 1 |
| FR-03 | The rasterizer consumes the command list and renders in **horizontal strips** of configurable height, with per-strip clipping. | M | 1 |
| FR-04 | Rounded corners and diagonal lines must be antialiased. | M | 1 |
| FR-05 | The rasterizer supports **dirty regions**: it only redraws what changed between frames (per-region command hash comparison). | S | 5 |
| FR-06 | Color is RGB565 internally; the API accepts RGB888 and converts. Simple src-over alpha blending for rects/bitmaps/text. | M | 1 |
| FR-07 | **Per-strip post-effect** hook (`fn(strip_buf, y0, h, user)`) for theme effects (scanline, glitch, glow). | S | 7 |

## Text and fonts
| ID | Requirement | Pri | It |
|----|-------------|-----|----|
| FR-10 | 4bpp antialiased bitmap fonts generated from TTF by `tools/fontgen.py`, with a configurable glyph subset. | M | 2 |
| FR-11 | UTF-8 text rendering with measurement (`cydui_text_size`) and optional word wrap. | M | 2 |
| FR-12 | Invalid UTF-8 is rejected/replaced with U+FFFD with no out-of-bounds reads. | M | 2 |
| FR-13 | Icon font support (same mechanism, glyphs in the PUA). *Partial:* the generator takes any TTF and code point range; no icon set is shipped yet. | S | 2/4 |

## Immediate mode and layout
| ID | Requirement | Pri | It |
|----|-------------|-----|----|
| FR-20 | Immediate-mode API: `if (cydui_button(ui, "Ok")) { ... }` called every frame. | M | 3 |
| FR-21 | Widgets get a stable ID derived from a hash (label, or `cydui_push_id`). ID collisions are detected in debug builds. | M | 3 |
| FR-22 | Persistent per-widget state (hover, active, scroll, animation value) lives in a fixed-size table indexed by ID; state of widgets not seen for N frames is recycled. | M | 3 |
| FR-23 | Layouts: `row`, `column`, `grid`, `spacer`, `pad`, fixed size / fill / auto. | M | 3 |
| FR-24 | **Single-pass** layout with "one-frame delay" support (egui-style) for auto sizes. | S | 3 |
| FR-25 | Scroll containers with clipping (`cydui_scroll_area`). | M | 4 |

## Widgets
| ID | Requirement | Pri | It |
|----|-------------|-----|----|
| FR-30 | `label`, `button`, `checkbox`, `toggle`. | M | 3 |
| FR-31 | `slider`, `progress`. | M | 4 |
| FR-32 | `tabs`. | M | 4 |
| FR-33 | List with drag scrolling and **kinetic** inertia. | M | 4 |
| FR-34 | `text_input` with cursor and on-screen **virtual keyboard**. | M | 6 |
| FR-35 | `popup`/modal with top layer and input capture. | S | 6 |
| FR-36 | API for custom widgets (the app registers its own drawing on top of the primitives). | S | 4 |

## Input
| ID | Requirement | Pri | It |
|----|-------------|-----|----|
| FR-40 | The backend delivers normalized events: `down`, `move`, `up` with (x,y) in screen pixels. | M | 3 |
| FR-41 | The library derives `tap`, `drag`, `long_press` with configurable thresholds. | M | 3 |
| FR-42 | ESP32 backend: touch calibration (3+ points) persisted in NVS. | M | 5 |
| FR-43 | Touch noise filter (mean/median + pressure debounce). | M | 5 |
| FR-44 | Keyboard focus and navigation in the SDL backend (for testing); on the CYD optional via the BOOT button. | C | 6 |

## Themes and animation
| ID | Requirement | Pri | It |
|----|-------------|-----|----|
| FR-50 | Theme = style table struct (colors, border, radius, padding, fonts, durations) per widget state. | M | 4 |
| FR-51 | A theme can **override the paint function** of each widget type (`paint_button`, `paint_checkbox`, ...). | M | 4 |
| FR-52 | Runtime theme switching without reinitializing the context. | M | 4 |
| FR-53 | Tweens with easing (linear, in/out quad/cubic, back, elastic) bound to widget state. | M | 7 |
| FR-54 | Shipped themes: `clean`, `retro`, `cyberpunk`. | M | 4/7 |
| FR-55 | Animation clock injected by the backend (`now_ms`), for deterministic tests. | M | 7 |

## Backends
| ID | Requirement | Pri | It |
|----|-------------|-----|----|
| FR-60 | `cydui_backend` interface = {`flush_strip`, `poll_input`, `now_ms`, `size`}. The core includes no SDL or IDF headers. | M | 1 |
| FR-61 | SDL2 backend (window, mouse→touch, headless mode for tests with framebuffer hash). | M | 1 |
| FR-62 | ESP32 backend: ILI9341 over SPI + DMA double buffering, XPT2046, PWM backlight. | M | 5 |
| FR-63 | ESP32 backend configurable via Kconfig (pins, SPI host, frequency, inversion, rotation). | S | 5 |

## Packaging
| ID | Requirement | Pri | It |
|----|-------------|-----|----|
| FR-70 | Distributed as an ESP-IDF component (`idf_component.yml`) and as a plain CMake build for PC. | M | 0/1 |
| FR-71 | Examples: `hello`, `widgets`, `theme-gallery`. | M | 3–7 |
