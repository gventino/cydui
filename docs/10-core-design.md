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

## Text and fonts (FR-10..13, CT-02, CT-03, CT-04, AB-16)

**Font format** (`include/cydui/font.h`). A `cydui_font` is constant data: a glyph table sorted by code point, 4-bit coverage bitmaps (A4, high nibble first, rows padded to a byte) and the line metrics. Lookup is a binary search that falls back to the box glyph stored under U+FFFD. Advances are whole pixels and there is no kerning (a known limitation, see below).

**Making fonts.** `tools/fontgen.py` renders a TTF with FreeType (`freetype-py==2.5.1`, which bundles FreeType 2.13.2, so output does not depend on the system library) into 8-bit coverage, quantizes it to 4 bits, always generates its own hollow box as the fallback glyph, shares storage between identical bitmaps, and writes `OUT.c`/`OUT.h` deterministically: `tools/gen_fonts.sh --check` regenerates into a temp dir and compares byte for byte. The bundled `cydui_sans_{12,16,24}` use ASCII, Latin-1 and a few symbols (`• … € ← ↑ → ↓`). `--preview TEXT` prints ASCII art of a string set in the generated font, for a quick look without a display.

**Licensing.** The bundled fonts are renderings of Liberation Sans and stay under the SIL OFL 1.1 (`fonts/LICENSE-OFL.txt`), not the MIT license of the code. The OFL reserves the name "Liberation", so the generated fonts are called `cydui_sans_*`; each generated file carries the original copyright notice and the OFL notice.

**UTF-8** (`include/cydui/utf8.h`). `cydui_utf8_decode` is strict: overlong forms, surrogates, values above U+10FFFF and bytes that never occur in UTF-8 are ill-formed, and each ill-formed sequence gives one U+FFFD for its "maximal subpart" (so `E2 82` followed by `g` is one replacement, `F0 80 80 80` is four). It always consumes at least one byte and never reads past `len`.

**Drawing text.** `cydui_draw_text` copies the string into a per-frame text arena (`text_mem` in the config), so callers may pass temporary buffers, and records one `CYDUI_CMD_TEXT`. Without `text_mem`, or when it is full, the text is dropped and `CYDUI_ERR_NO_MEMORY` is raised (CT-01). The rasterizer lays the text out again for every strip and blits only what touches it, so the image does not depend on the strip height. `cydui_text_size` measures with the same layout code, so a box sized from it always fits what is drawn.

**Layout.** One linear pass over the code points. Lines break at `\n`; at the last run of spaces when the text would pass the wrap width; and inside a word wider than the line. Every line consumes at least one byte, so the loop always ends even for a box narrower than one glyph. Leading spaces are kept (an indent); a run of spaces at a wrap point is dropped; trailing spaces do not count towards the width. A final newline does not add an empty line. `\r` is skipped and has no width.

**Limits (CT-02).** At most `CYDUI_MAX_STR` (256) bytes of a string are used. The cut falls on a character boundary: a sequence the limit would split is dropped whole, and `CYDUI_ERR_TEXT_TRUNCATED` is raised (informative, the text is still drawn). For `CYDUI_NTS` strings the search for the NUL stops at the limit, so an unterminated buffer is never read beyond it.

**Control and bidirectional characters (CT-04).** These draw the fallback box even when the font has a glyph for them: C0 and C1 controls (including tab and DEL), U+061C, U+200E/200F, U+202A..202E and U+2066..2069. The set is wider than the two ranges written in CT-04 (it adds the marks and the C0/C1 controls); the bidi part can be compiled out with `CYDUI_FILTER_CONTROL=0`. Boundaries are tested with real glyphs on both sides of every range.

**Malformed fonts (AB-16, CT-09).** The glyph bitmap is read only through `cydui_font_glyph_data`, which checks `offset + size <= bitmap_len` without overflow; the fallback index is checked before use; a font without a usable fallback is rejected (`CYDUI_ERR_BAD_ARG`). `cydui_font_validate` does the full check (order, bounds, fallback, metrics) for startup and tests. Culling uses `CYDUI_TEXT_REACH`, the worst-case distance of a glyph's ink from its line box (`ascent + 383`, from the int8 bearings and 255-row bitmaps), so skipping a line or a command can never change the image whatever the font says. A fuzz-built font with a tall ascent over a short line exposed exactly this once.

**Measured on the ESP32 (Xtensa, `-Os`, 32-bit pointers).**

| Item | Size |
|------|------|
| Whole core (`arena ctx font raster text types utf8`) | 6.8 KB of code (budget NFR-50: 60 KB) |
| `cydui_sans_12` / `_16` / `_24` | 9.3 / 13.1 / 23.4 KB (45.7 KB for the three; R-07) |
| `cydui_cmd` | 36 bytes (a list of 64 commands: 2.3 KB) |
| `cydui_ctx` / `cydui_font` / `cydui_glyph` | 52 / 20 / 16 bytes |

The line rasterizer uses `int64_t`, which the 32-bit core emulates; it is still to be timed on the board (R-02, It5).

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
| `test_golden` | nine scenes (six drawing, three text) pinned by FNV-1a 64 hashes of the framebuffer (`tests/golden/*.hash`); the PNG of every run goes to `build/**/golden/` for looking at |
| `test_utf8` | 45 vectors, every scalar round-trips, every truncation, exhaustive 1-3 byte inputs on exact-size heap buffers |
| `test_font`, `test_fonts_bundled` | lookup and fallback, validation of every kind of malformed font, bounds-checked glyph access; every bundled font validates and the Portuguese subset has ink |
| `test_text` | layout by hand on a monospace font (wrap, long words, newlines, edge cases, termination over many inputs), the length limit and its character boundary, alignment, filter boundaries with real glyphs on both sides, the text arena, malformed fonts, ink far from the line box, strip-height independence |
| `fuzz_cmds` (clang) | random call sequences with extreme parameters, text with arbitrary bytes and fonts built from the input, under ASan/UBSan, asserting strip-height independence (CT-12). Seed inputs in `tests/fuzz_seeds/` are run by `ctest`. |
| `fuzz_utf8` (clang) | the decoder always progresses, stays inside the buffer, only returns scalar values, and re-encodes what it decoded |

Updating a golden: run `CYDUI_UPDATE_GOLDEN=1 ./build/tests/test_golden`, **open the PNGs first**, then commit the changed `.hash` files together with the code change that caused them.

Test-quality note: the unit tests and fuzzers were mutation-checked in both iterations, about 40 deliberate bugs in the rasterizer, the UTF-8 decoder, the font module and the text code, each of which must make a test or the fuzzer fail. Several first attempts survived and each hole was closed by a specific test:
- It1: breaking the AA corner bias (the tests only checked symmetry, hence the area tests), and a fuzz harness that did not find an injected strip bug until its parameter generation was rewritten.
- It2: a filter boundary one code point off (hence real glyphs on both sides of every range), a culling distance that was too small (hence fonts whose ink lands far from the line box, on screen), a string that was not copied (the overwriting buffer must differ visibly), a blank-glyph rule, and again a fuzz harness that found nothing until it got seed inputs and a mild mode for the generated fonts.

## Known limits and follow-ups
- Line rasterization is per pixel over the clipped bounding box with `int64_t` math; fine on the PC, to be measured on the ESP32 in It5 (R-02). A per-row span restriction is the obvious optimization if needed.
- Bitmap data for `RGB565` is read with `memcpy`, so unaligned data is fine; the A4 reader assumes the layout above.
- Text: no kerning, whole-pixel advances, no real bidirectional text or shaping (the bidi marks are drawn as boxes, not applied), no ligatures, no ellipsis. The 12 px size is the most sensitive to the missing kerning.
- FR-13 (icon fonts) is only partly done: `fontgen.py` takes any TTF and any code point range, so an icon font works the same way, but no icon set is shipped yet (It4).
- No dirty regions yet (FR-05, It5), no widgets (It3).
