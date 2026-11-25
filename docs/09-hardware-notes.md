# 09 — Hardware Notes

Findings from iteration 0 (`examples/hw-probe`) on the author's board. Treat them as the reference for the ESP32 backend (It5). Other CYD batches may differ; the probe is meant to be re-run on any new board.

## Board
| Item | Value |
|------|-------|
| Module | ESP32-2432S028R ("classic" CYD), single-USB, CH340 (`1a86:7523`) at `/dev/ttyUSB0` |
| SoC | ESP32 rev v3.1, dual core, 240 MHz, no PSRAM |
| Flash | 4 MB, DIO, 40 MHz |
| Free heap at boot (no WiFi, probe running) | ~293 KB |

## Pins (confirmed working with the defaults)
| Function | GPIO | Notes |
|----------|------|-------|
| LCD MOSI / MISO / SCLK | 13 / 12 / 14 | SPI2 (HSPI) |
| LCD CS / DC | 15 / 2 | GPIO 2 and 15 are strapping pins; works fine as outputs after boot |
| LCD backlight | 21 | active high, LEDC PWM works |
| LCD reset | not wired | software reset (0x01) + 150 ms wait |
| Touch CLK / MOSI / MISO | 25 / 32 / 39 | SPI3 (VSPI), separate bus, DMA not needed |
| Touch CS / IRQ | 33 / 36 | IRQ is active low while touched |

Because GPIO 2, 12 and 15 are strapping pins, CT-14 validates against the board profile instead of rejecting them (see `06-controls-and-risks.md`).

## Display panel
- **The panel behaves as natively 320x240 (ILI9342C-like), not as an ILI9341 (natively 240x320).** On an ILI9341 `MV=1` gives landscape; here `MV=1` gives portrait.
- **Working configuration:** `MADCTL = 0x00` → 320x240 landscape, no mirroring, RGB order (BGR bit **off**), no inversion (no `INVON`). 16 bpp (`0x3A = 0x55`).
- **Symptoms of the wrong configuration** (useful when debugging other boards):
  - ILI9341-style landscape (`MADCTL=0x28`): only ~240 of 320 columns written (left 25% kept the old image), axes swapped, content wrapped.
  - ILI9341-style portrait (`MADCTL=0x48`): image mirrored horizontally, 320 rows wrapped at 240, R/B swapped.
- **ID read-back is unreliable** (0x04 and 0xD3 return different garbage on every boot). Do not auto-detect the controller from the ID; use Kconfig.
- Init sequence used: the common ILI9341 register set (power, VCOM, frame rate, gamma, entry mode) followed by `MADCTL`, sleep out (120 ms), display on. It worked unchanged on this panel.

## SPI speed (full-screen 320x240 RGB565 fill, polling transfers, 6400-byte chunks)
| Requested | Real SCLK | 10 fills | FPS | Visual result |
|-----------|-----------|----------|-----|----------------|
| 20 MHz | 20 MHz | 620 ms | 16.1 | clean |
| 40 MHz | 40 MHz | 313 ms | 31.9 | clean |
| 60 MHz | 40 MHz (80 MHz / 2) | 313 ms | 31.9 | clean |
| 80 MHz | 80 MHz | 159 ms | 62.6 | clean (no noise or artifacts seen) |

- The ESP32 SPI clock divides 80 MHz by integers, so 60 MHz silently runs at 40 MHz.
- 80 MHz is stable on this board for write-only traffic. Keep it configurable: other boards or longer wires may not manage it. Default for the backend: 40 MHz, with 80 MHz as an opt-in Kconfig.
- Theoretical ceiling for a full-screen redraw: ~31 fps at 40 MHz, ~62 fps at 80 MHz. This is why dirty regions (FR-05) matter for NFR-01 at 40 MHz.

## Touch (XPT2046)
- Commands: `0xD0` reads X, `0x90` reads Y, `0xB0`/`0xC0` read Z1/Z2. SPI at 2 MHz, mode 0, 24-bit transactions (12-bit result in bits 14..3 of the last two bytes).
- **The first conversion after switching channel must be discarded.** Without that, X stayed stuck near 3300 regardless of where the screen was touched.
- **Axes are swapped relative to the screen.** Raw Y = horizontal position, raw X = vertical position. Both increase to the right / downward (no inversion).
- **Usable raw range: ~400 .. ~3600** on both axes (measured with continuous drags); the backend's calibration (below) replaces the default map of these numbers.
- Pressure `z = z1 + 4095 - z2`: ~1100–2800 while touching with a finger; it varies with pressure and position, so a fixed threshold needs care. The IRQ pin gates reads (low = touched).
- Noise: individual samples jitter by ±10–20 counts when held still, so averaging 4 samples (after the discard) is enough for the probe; the backend still needs a filter and debounce (FR-43).
- A naive linear mapping with the axis swap and the 400..3600 range put the on-screen dot correctly under the finger. The real backend still computes a calibration matrix (FR-42).

## Results of iteration 5 (the ESP32 backend, `examples/theme-gallery/idf`)

Measured on this board, 240 MHz, `-O2`, ESP-IDF 5.5.6, strips of 20 rows in two DMA buffers (25.6 KB). The firmware prints the numbers (`GALLERY_*` lines); the gallery is built with `CONFIG_GALLERY_*` options in its `Kconfig.projbuild`.

**Pixels.** The hash of what is sent to the panel, for each of 3 themes x 5 pages, equals the PC's `theme-gallery --headless --hash` in all 15 cases, at 40 and at 80 MHz: the renderer is bit for bit the same on Xtensa.

**Speed** (NFR-01):

| Scene | 40 MHz | 80 MHz |
|---|---|---|
| Full redraw, 12 strips | 24.4 fps | 29.9 fps |
| Animation, dirty regions (3 of 12 strips per frame) | 82.7 fps | 104 fps |
| Animation, every strip | 25.2 fps | 36.7 fps |
| Drawing alone / sending alone, per full frame | 26.1 ms / 34.5 ms | 26.1 ms / 19.1 ms |

Sending is the limit at 40 MHz and drawing at 80 MHz. The animation is above 30 fps at both clocks only because of the dirty regions. 80 MHz was checked by hash and by speed, not by eye (see below).

**Cost of each command** on one 320 x 20 strip (the data for the optimization of It7, R-02): opaque rectangle 94 ns/pixel, with alpha 232, filled rounded rectangle 460, rounded outline 803, gradient 125, A4 bitmap 590; a 2 px diagonal line 3.4 ms per strip; 25 characters of 16 px text 0.63 ms per strip; an empty strip (the clear) 0.19 ms. A full redraw costs ~26 ms in drawing (about 75 cycles per pixel), against 0.28 ms for the same frame on a PC: this was the first thing to optimize in It7.

**After the optimization of the renderer (It7, step 1).** The same probe, per 320 x 20 strip: the clear 27 us (was 190), opaque rectangle 10 ns/pixel (was 94), with alpha 110 (was 232), filled rounded rectangle 42 (was 460), rounded outline 56 (was 803), gradient 11 (was 125), A4 bitmap 142 (was 590), a 2 px diagonal line 1.19 ms (was 3.4), 25 characters of 16 px text 0.36 ms (was 0.63). A full redraw of the gallery costs 11.5 ms of drawing (was ~26) against 34.5 ms of SPI at 40 MHz, so it is now bound by the SPI: 26.8 fps (a full redraw was 24.4); the animation that sends every strip draws in 6.1 ms; the animation with dirty regions 102.8 fps. The 18 hashes of the gallery are the same as the PC's and as before. How: a rounded rectangle is cut per row into runs of one coverage (only the pixels of the arc are computed, and of those only the ones within half a pixel of it need the square root), runs are filled by words or blended once per distinct background, a line only visits the columns near the segment, a text glyph skips empty bytes. Equivalence with the previous rasterizer was checked pixel by pixel on 400000 random strips of every command (a run outside the repository), besides the goldens.

**UI pass** (NFR-03): 0.67 to 1.5 ms for the gallery pages (13 to ~40 widgets), limit 3 ms.

**Idle** (NFR-04): 10 s with nothing to draw: 0 strips sent, the application task 0.12 % of its core and the touch task 0.01 %, the rest idle. `cydui_esp32_wait` sleeps on the touch queue.

**Memory** (NFR-10, NFR-13): the UI storage (commands, text, states) is 11.1 KB, the strips 25.6 KB, the dirty hashes 96 B, `cydui_ui` 1.3 KB and the backend 0.7 KB, all static: 37 KB in all, under the 40 KB + strips budget of NFR-10. The free internal heap after the backend started is 305.6 KB (largest block 110 KB), and does not move while running. Wi-Fi and TLS were not started, so the "80 KB left for the application" of NFR-13 is a margin of 3.8x, not a measurement of the todo example.

**Flash** (NFR-50): the library is 23.6 KB and both `clean` themes 1.3 KB (`-Os`, Xtensa); the whole gallery firmware, with three fonts and the IDF, is 0x4f4d0 bytes (326 KB, 69 % of the 1 MB partition free).

**Frame budget** (CT-15): a frame with a 1 ms budget that takes 3 ms before its widgets are called builds none of them and raises `CYDUI_ERR_FRAME_BUDGET`; with 20 ms it builds all 13 and raises nothing.

**Touch, calibration and latency** (FR-42, FR-43, NFR-02, CT-06). The calibration is made on screen with five targets and kept in the flash. Tried with the stylus that comes with the board (it is a lot better than a finger on a resistive panel):
- the four targets of the touch page: 7 px of worst error in a first try, 4 px after calibrating again (limit 6 px). The number includes the aim of the person on a 20 px circle, so it is an upper bound of the error of the calibration;
- latency from a press entering the queue to the end of the frame that shows it: p95 24 ms (limit 50 ms); the debounce adds about 11 ms before the press is queued (one more sampling period).

These two lines were read from the screen of the board; the serial capture was not running at that moment, so there is no log of them.

**Text input and the keyboard (iteration 6).** All 18 hashes (3 themes x 6 pages, the new one is "Texto") equal the PC's. A self-test of the firmware injects touches under the real one and types "Ola 1" through the keys of the keyboard on the screen (Shift, the page of numbers, the space, Ok): passes, 45 frames of which the average took 15.3 ms and the worst 74 ms (the frame that opens the keyboard: about 120 commands, the dimming and a dozen strips), 2.4 strips sent per frame. The keyboard needs about 120 commands, so the storage of the UI of the gallery is 17.8 KB (320 commands, 1536 bytes of text, 48 states) and `cydui_ui` is 1.9 KB (it was 1.3 KB: a queue of 32 keys), `cydui_theme` 1.4 KB (three more kinds of widget); the free heap is 295.8 KB (it was 305.6 KB). The `cydui` component (core, widgets, text fields, popup, keyboard and the ESP32 backend) is 50.4 KB of flash at `-O2`, the firmware 0x51e60 bytes. The speed numbers of It5 did not change (a full redraw 24 fps, the animation with dirty regions 84 fps, idle CPU 0.09 %).
Nobody typed on the real screen yet: the layout was checked as pictures and by the self-test, and the keys are 30 x 32 px, which with the 4 px of error of the touch with the stylus (more with a finger) is to be tried by the user.

**Not verified on the hardware.** The 80 MHz clock (only by hash and speed, nobody looked at the screen), rotation 180 (the MADCTL value and the touch flip follow the Kconfig in the code; the picture was not seen) and the touch with a bare finger. Both are Kconfig options and the default stays at 40 MHz and rotation 0; the board is a single unit (R-12).

**Robustness** (NFR-60, NFR-61): a transfer to the panel that does not end in a second is given up with an error log, the calibration waits at most 5 s for a lift, a missing touch controller reads as "not touched" (pull-up on the interrupt pin) and `cydui_esp32_init` returns an error the application can decide on; every loop of the backend sleeps or feeds the task watchdog. The hardened flags of CT-13 (`STACK_CHECK_MODE_STRONG`, `HEAP_POISONING_COMPREHENSIVE`, `-Wformat-security`) are checked from the `sdkconfig` and the compile commands by the local `ci.sh`.

## Process notes
- Flashing works without root once the user is in the `uucp` group (`newgrp uucp` or a new login). Auto-reset via RTS worked, no need to hold BOOT.
- The firmware that came on the board was overwritten during this iteration (it was not backed up).
- A new Kconfig symbol needs `idf.py reconfigure`; a new option of `sdkconfig.defaults` needs the `sdkconfig` to be recreated. The serial port is read by `tools/serial_capture.py` (it writes line by line).
- The local check script (untracked `ci.sh`) builds `hw-probe` and the gallery for the board too, so the Kconfig/sdkconfig defaults must always build from a clean checkout.
