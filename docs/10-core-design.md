# 10 — Core Design

How the core of `cydui` works as of iteration 1. Requirements are in `01`/`02`; this document records the decisions and the numeric conventions that the tests pin down.

## Layers
```
 app code ──▶ cydui_draw_*()  ──▶ command list (fixed array) ──▶ cydui_raster_strip() ──▶ backend->flush_strip()
              (ctx.c)              (cmd.h)                        (raster.c)                (mem, sdl, esp32)
```
The core (`src/`, `include/`) is C99 and depends on nothing but `stdint.h`, `stddef.h`, `stdbool.h` and `string.h`. Backends are separate libraries: `backends/mem` (framebuffer in RAM, hash, PNG), `backends/sdl` (window and mouse); `backends/esp32` arrives in It5.

## Memory (FR-01, NFR-11, NFR-12, CT-01)
- The library never calls `malloc`; the local check script checks that `libcydui.a` has no reference to it, and no writable global or static data (NFR-21).
- `cydui_init` receives two blocks from the caller: memory for the command list and memory for the render strip. The command list takes as many whole `cydui_cmd` as fit; the strip height is `strip_mem_size / (width * 2)`, capped at the screen height. Too little memory returns `CYDUI_ERR_NO_MEMORY` instead of failing later.
- `cydui_arena` is the bump allocator used for that carving. It aligns the address (not the offset), rejects zero sizes and non-power-of-two alignments, and cannot wrap on huge sizes.
- Overflow of the command list drops the command and sets `CYDUI_ERR_CMD_OVERFLOW`; the frame still renders. Commands that would draw nothing (empty, fully clipped, alpha 0) are dropped *before* the capacity check, so they cost nothing.

## Commands (FR-02)
`RECT`, `RRECT` (filled, or an outline drawn inside the rectangle), `LINE`, `BITMAP` and `GRADIENT`. Every command stores `alpha` (0..255), its geometry and the clip rectangle in effect when it was emitted (the clip is already intersected with the screen). The list is rasterized in order (painter's algorithm).

- **Bitmap formats:** `RGB565` (2 bytes per pixel, native order, row-major) and `A4` (4-bit coverage, high nibble first, rows padded to a whole byte; tinted with a color). `A4` is what bitmap fonts will use in It2.
- **CT-10:** `cydui_draw_bitmap` takes the buffer length and drops the command with `CYDUI_ERR_BAD_BITMAP` if it is smaller than `w * h * bpp` (computed in `size_t`; the largest legal bitmap, 32767 x 32767, is rejected cleanly instead of wrapping). The caller must keep the data alive until `cydui_end_frame`.
- **Limits:** radius and line thickness are `uint8_t` (0..255). Radius is clamped to half the smaller side. An outline thicker than the shape degrades to a filled shape.

## Rasterization (FR-03, FR-04, FR-06, NFR-30)
- **Strips.** `cydui_end_frame` renders the screen in full-width strips and hands each to `backend->flush_strip`. A strip is first filled with the frame's clear color, then every command that touches it is drawn. **The image does not depend on the strip height** — this is tested with heights from 1 to 240 (unit tests, goldens) and fuzzed.
- **Integers only.** No `float`/`double` anywhere in the core, so the output is bit-identical across gcc, clang, `-O0`/`-O2` and (by design) the ESP32. The goldens are pinned by gcc and verified on clang and on an optimized build.
- **Color.** `cydui_color` is RGB565 in native byte order; byte swapping for a panel is the backend's job. `CYDUI_RGB(r,g,b)` packs 8-bit channels.
- **Blending.** Source-over using the spread-bits trick (`0x07E0F81F`): 5-bit alpha granularity (`(alpha + 4) >> 3`), exact at alpha 0 and 255. Antialiasing coverage in 1/16 units is combined with the command alpha and applied through the same blend; a coverage near 0 is skipped and near 1 is written directly.
- **Antialiasing.** Signed distance in 1/16 pixel with an integer square root. Pixel centers sit at `(x*16 + 8)`, so integer-aligned straight edges are crisp and only curves need AA. Coverage is `clamp(8 - d, 0, 16)` (half a pixel on each side of the edge).
  - *Rounded rect:* only the corner zones compute a distance; body rows are solid spans. An outline is the outer coverage minus the coverage of the rectangle shrunk by the thickness (radius reduced accordingly).
  - *Line:* distance from the pixel to the segment, with round caps; endpoints are pixel centers, so a 1 px horizontal line covers exactly one row. Thickness is the stroke width in pixels. Uses `int64_t` for the projection and cross product; pixels beyond `thickness/2 + 1/2` px are rejected before any division.
  - *Verified against geometry:* the tests sum the coverage of a rounded rect, a circle and a thick line and compare with the exact area (`w*h - (4-π)r²`, `πr²`, `L*t + πt²/4`) within ~0.3%, so the AA is correct and not only symmetric.
- **Gradient.** Two colors, per-channel integer interpolation with truncation, exact at both ends; a one-pixel-long gradient is the first color.

## Backends (FR-60, FR-61)
```c
typedef struct {
    void *user; int16_t width, height;
    void (*flush_strip)(user, const cydui_rect *area, const cydui_color *pixels); /* required */
    bool (*poll_event)(user, cydui_event *ev);                                    /* may be NULL */
    uint32_t (*now_ms)(user);                                                     /* may be NULL */
    void (*frame_end)(user);                                                      /* may be NULL */
} cydui_backend;
```
- `flush_strip` receives `area->w * area->h` pixels with stride `area->w`. Today it is always a full-width strip; the rectangle form is what lets the ESP32 backend send dirty regions in It5.
- **Deviations from the requirements text:**
  - `frame_end` was added to FR-60: the SDL backend needs it to present the frame.
  - FR-61's "headless mode with framebuffer hash" is the separate `mem` backend instead of a mode of the SDL backend. It has no dependencies, so every test and local check run uses it; the SDL backend is only the window.

## Testing
| Layer | What it checks |
|-------|----------------|
| `test_arena`, `test_color`, `test_ctx` | arena limits, color math (all 65536 colors round-trip, blend endpoints), init validation, command overflow (CT-01), bitmap length (CT-10) |
| `test_raster` | exact pixels, clipping, symmetry, AA area vs. geometry, strip-height independence |
| `test_golden` | six scenes pinned by FNV-1a 64 hashes of the framebuffer (`tests/golden/*.hash`); the PNG of every run goes to `build/**/golden/` for looking at |
| `fuzz_cmds` (clang) | random call sequences with extreme parameters under ASan/UBSan, asserting strip-height independence (CT-12) |

Updating a golden: run `CYDUI_UPDATE_GOLDEN=1 ./build/tests/test_golden`, **open the PNGs first**, then commit the changed `.hash` files together with the code change that caused them.

Test-quality note: the unit tests were mutation-checked. Breaking the AA corner bias, the line distance rounding or the bitmap strip offset each makes a test fail. The first attempt at the corner bias survived (tests only checked symmetry), which is why the area tests exist. The fuzz harness was strengthened the same way after it failed to find an injected strip bug.

## Known limits and follow-ups
- Line rasterization is per pixel over the clipped bounding box with `int64_t` math; fine on the PC, to be measured on the ESP32 in It5 (R-02). A per-row span restriction is the obvious optimization if needed.
- Bitmap data for `RGB565` is read with `memcpy`, so unaligned data is fine; the A4 reader assumes the layout above.
- No dirty regions yet (FR-05, It5), no text (It2), no widgets (It3).
