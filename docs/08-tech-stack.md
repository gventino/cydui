# 08 — Tech Stack

Principle: **KISS** — few dependencies, everything replaceable, core with no external dependencies.

> **No hosted CI.** There is no CI service or workflow in this repository. Where the docs say "CI" or "local checks", they mean the checks run on the developer machine: `ctest`, the sanitizer and fuzz builds described in the README, and an untracked `ci.sh` helper that chains them.

## Languages
| Use | Language |
|-----|----------|
| Core, themes, widgets | C99 |
| ESP32 backend | C (ESP-IDF; toolchain GNU11) |
| SDL backend, tests | C99 (+ C++ only for libFuzzer, if needed) |
| Tools (fontgen, test scripts) | Python 3 |

## Firmware / target
| Item | Choice | Notes |
|------|--------|-------|
| SoC | ESP32 (WROOM, dual-core 240 MHz, no PSRAM) | CYD ESP32-2432S028R |
| Framework | **ESP-IDF v5.5.6** (pinned in It0; v6.x exists but is out of scope for now) | Installed in `~/esp/esp-idf` via the IDF `install.sh`. The IDF tooling runs on **Python 3.13** from `uv`, because the system Python (3.14) is too new for IDF 5.5. `source tools/env.sh` loads everything without touching shell rc files. |
| Drivers | `driver/spi_master` + DMA, `driver/ledc` (backlight), `nvs_flash`, FreeRTOS | No `esp_lcd` at first (full control of strips); may be revisited. |
| Display | ILI9341-compatible command set, SPI, RGB565, 320×240. On the author's board the panel is natively 320x240 (ILI9342C-like); see `09-hardware-notes.md`. | Init via our own command sequence; orientation via Kconfig. |
| Touch | XPT2046 (separate SPI on the CYD) | Simple custom driver. |
| Config | Kconfig (pins, SPI, rotation, inversion) | |
| Build | CMake via `idf.py` | Library as a component with `idf_component.yml`. |

## PC development
| Item | Choice |
|------|--------|
| Compiler | gcc ≥ 13 and clang (the local checks run both) |
| Build | CMake ≥ 3.20 + Ninja |
| Window/input | **SDL2** (`libsdl2-dev` / Arch package: `sdl2`) |
| Sanitizers | ASan + UBSan |
| Fuzzing | libFuzzer (clang) |
| Coverage | gcov / lcov |
| Tests | Unity (ships with IDF; vendored in the repo for PC) + `ctest` runner; golden images by hash (FNV-1a/CRC32 of the framebuffer) and PNG diff via Python script |

## Python tools
| Package | Use |
|---------|-----|
| `freetype-py` | Rasterize glyphs from the TTF |
| `pillow` | Output debugging/PNG diff |
| `numpy` (optional) | 4bpp packing |
Installed in a local `venv` (`tools/.venv`), no global packages.

## Runtime dependencies (library)
- **None** beyond minimal libc (`string.h`, `stdint.h`, `stddef.h`, `stdbool.h`; `math.h` avoided in the pixel path).
- SDL backend → SDL2. ESP32 backend → ESP-IDF.

## Deliberate decisions (and what was left out)
| Alternative | Why not (for now) |
|-------------|-------------------|
| LVGL | It's exactly what the user wants to avoid; retained model. |
| TFT_eSPI / LovyanGFX | Ties us to Arduino/a driver layer; we want our own strip rasterizer. |
| FreeType in firmware | Heavy; fonts are pre-generated. |
| Full framebuffer | 150 KB, no PSRAM; incompatible with WiFi+TLS. |
| Dynamic heap in the library | Would violate NFR-11 and hinder robustness proofs. |
| `float` in the pixel path | SDL/ESP32 determinism (R-08); use integer/fixed point. |

## Dependencies for the app (future repo `cyd-todo`)
- ESP-IDF: `esp_wifi`, `esp_http_client` + `esp-tls` (mbedTLS), `cJSON` (or `jsmn` for KISS and memory limits), `nvs_flash`.
- Homelab backend: to be decided (e.g. small HTTP service with SQLite). Out of scope for the library.

## Required installation (to be done in It0, with your approval)
1. `sudo usermod -aG uucp $USER` (and re-login).
2. System packages: `sdl2`, `ninja`, `clang`, `git`, `python`, `cmake`, `ccache`, `dfu-util`/`libusb` (IDF dependencies).
3. `uv python install 3.13`, then ESP-IDF v5.5.6 in `~/esp/esp-idf` + `./install.sh esp32` with that Python first on `PATH` (`tools/env.sh` does this at runtime).
4. Python venv for `tools/`.

## Repo layout
```
cydui/
  docs/  include/cydui/  src/  themes/  backends/{sdl,esp32}/
  tools/fontgen.py  examples/  tests/  CMakeLists.txt  idf_component.yml  LICENSE  README.md
```
