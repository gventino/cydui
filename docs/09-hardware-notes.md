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
- **Usable raw range: ~400 .. ~3600** on both axes (measured with continuous drags; exact edges to be fixed by calibration in It5).
- Pressure `z = z1 + 4095 - z2`: ~1100–2800 while touching with a finger; it varies with pressure and position, so a fixed threshold needs care. The IRQ pin gates reads (low = touched).
- Noise: individual samples jitter by ±10–20 counts when held still, so averaging 4 samples (after the discard) is enough for the probe; the backend still needs a filter and debounce (FR-43).
- A naive linear mapping with the axis swap and the 400..3600 range put the on-screen dot correctly under the finger. The real backend still computes a calibration matrix (FR-42).

## Process notes
- Flashing works without root once the user is in the `uucp` group (`newgrp uucp` or a new login). Auto-reset via RTS worked, no need to hold BOOT.
- The firmware that came on the board was overwritten during this iteration (it was not backed up).
- `ci.sh` builds `hw-probe` too, so the Kconfig/sdkconfig defaults must always build from a clean checkout.
