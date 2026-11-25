#!/usr/bin/env python3
"""Generate a cydui bitmap font (C source) from a TrueType/OpenType file.

Each glyph is rendered by FreeType as 8-bit coverage and quantized to 4 bits (A4): high nibble
first, every row padded to a whole byte. The output is deterministic (no dates, no paths), so
regenerating it must give byte-identical files.

    fontgen.py --ttf Font.ttf --size 16 --name my_font_16 --out fonts/my_font_16 \
               --chars "U+0020-U+007E,U+00A0-U+00FF,U+20AC"

writes fonts/my_font_16.c and fonts/my_font_16.h declaring `const cydui_font my_font_16`.

--chars is a comma-separated list of: U+XXXX, U+XXXX-U+YYYY, or literal characters where X-Y is a
range ("A-Za-z0-9éï"). Code points the font does not have are skipped with a note.

The fallback glyph (stored under U+FFFD) is always a hollow box drawn by this script, never the
font's own replacement glyph, so that control characters and bad bytes look the same in every font.
"""

import argparse
import os
import re
import sys

import freetype

FALLBACK_CP = 0xFFFD
HINTING = {
    "light": freetype.FT_LOAD_TARGET_LIGHT,
    "normal": freetype.FT_LOAD_TARGET_NORMAL,
    "none": freetype.FT_LOAD_NO_HINTING,
    "mono": freetype.FT_LOAD_TARGET_MONO,  # one bit per pixel: crisp, no antialiasing (a bitmap look)
}


def parse_chars(spec):
    """Turns the --chars specification into a sorted list of code points."""
    cps = set()
    for token in spec.split(","):
        token = token.strip()
        if not token:
            continue
        m = re.fullmatch(r"U\+([0-9A-Fa-f]+)(?:-U\+([0-9A-Fa-f]+))?", token)
        if m:
            lo = int(m.group(1), 16)
            hi = int(m.group(2), 16) if m.group(2) else lo
            if hi < lo:
                raise ValueError(f"empty range {token}")
            cps.update(range(lo, hi + 1))
            continue
        i = 0
        while i < len(token):
            if i + 2 < len(token) and token[i + 1] == "-":
                lo, hi = ord(token[i]), ord(token[i + 2])
                if hi < lo:
                    raise ValueError(f"empty range {token[i:i + 3]!r}")
                cps.update(range(lo, hi + 1))
                i += 3
            else:
                cps.add(ord(token[i]))
                i += 1
    for cp in cps:
        if cp > 0x10FFFF or 0xD800 <= cp <= 0xDFFF:
            raise ValueError(f"U+{cp:04X} is not a Unicode scalar value")
    return sorted(cps)


class Glyph:
    def __init__(self, cp, w, h, left, top, advance, rows):
        self.cp, self.w, self.h = cp, w, h
        self.left, self.top, self.advance = left, top, advance
        self.rows = rows  # list of h lists of w values 0..15
        self.offset = 0

    def packed(self):
        out = bytearray()
        for row in self.rows:
            for x in range(0, self.w, 2):
                hi = row[x]
                lo = row[x + 1] if x + 1 < self.w else 0
                out.append((hi << 4) | lo)
        return bytes(out)


def to_nibble(v):
    return (v * 15 + 127) // 255


def render_glyph(face, cp, flags):
    face.load_char(chr(cp), flags | freetype.FT_LOAD_RENDER)  # TARGET_MONO renders a 1-bit bitmap
    slot = face.glyph
    bm = slot.bitmap
    w, h = bm.width, bm.rows
    advance = (slot.advance.x + 32) >> 6
    if not 0 <= advance <= 255:
        raise ValueError(f"U+{cp:04X}: advance {advance} does not fit in a byte")
    if w == 0 or h == 0:
        return Glyph(cp, 0, 0, 0, 0, advance, [])
    if w > 255 or h > 255:
        raise ValueError(f"U+{cp:04X}: bitmap {w}x{h} too large")
    if not (-128 <= slot.bitmap_left <= 127 and -128 <= slot.bitmap_top <= 127):
        raise ValueError(f"U+{cp:04X}: bearing out of range")
    buf = bm.buffer
    if bm.pixel_mode == freetype.FT_PIXEL_MODE_MONO:  # most significant bit first
        rows = [[15 if buf[y * bm.pitch + x // 8] & (0x80 >> (x % 8)) else 0 for x in range(w)] for y in range(h)]
    elif bm.pixel_mode == freetype.FT_PIXEL_MODE_GRAY:
        rows = [[to_nibble(buf[y * bm.pitch + x]) for x in range(w)] for y in range(h)]
    else:
        raise ValueError(f"U+{cp:04X}: unexpected pixel mode {bm.pixel_mode}")
    if not any(any(r) for r in rows):  # a blank bitmap (a space in a 1-bit rendering) is just an advance
        return Glyph(cp, 0, 0, 0, 0, advance, [])
    return Glyph(cp, w, h, slot.bitmap_left, slot.bitmap_top, advance, rows)


def make_box(size):
    """A hollow rectangle standing on the baseline: the glyph for anything unrenderable."""
    h = max(5, round(size * 0.7))
    w = max(4, round(size * 0.5))
    rows = []
    for y in range(h):
        if y in (0, h - 1):
            rows.append([15] * w)
        else:
            rows.append([15] + [0] * (w - 2) + [15])
    return Glyph(FALLBACK_CP, w, h, 1, h, w + 2, rows)


def build_font(ttf, size, cps, hinting):
    face = freetype.Face(ttf)
    face.set_pixel_sizes(0, size)
    flags = HINTING[hinting]
    glyphs, missing = [], []
    for cp in cps:
        if cp == FALLBACK_CP:
            print(f"note: U+FFFD is always the generated box; the font's own glyph is ignored",
                  file=sys.stderr)
            continue
        if face.get_char_index(cp) == 0:
            missing.append(cp)
            continue
        glyphs.append(render_glyph(face, cp, flags))
    glyphs.append(make_box(size))
    glyphs.sort(key=lambda g: g.cp)

    # Lay out the bitmap data, sharing storage between identical bitmaps.
    data = bytearray()
    seen = {}
    for g in glyphs:
        packed = g.packed()
        if not packed:
            g.offset = 0
            continue
        if packed not in seen:
            seen[packed] = len(data)
            data += packed
        g.offset = seen[packed]
    if len(data) > 0xFFFFFFFF or len(glyphs) > 0xFFFF:
        raise ValueError("font too large")

    metrics = face.size
    line_height = (metrics.height + 32) >> 6
    ascent = (metrics.ascender + 32) >> 6
    if not (0 < line_height <= 255 and 0 <= ascent <= 255):
        raise ValueError(f"line metrics out of range: {line_height}, {ascent}")
    fallback = next(i for i, g in enumerate(glyphs) if g.cp == FALLBACK_CP)
    info = f"{face.family_name.decode()} {face.style_name.decode()}"
    return glyphs, bytes(data), line_height, ascent, fallback, missing, info


def comment_lines(text):
    return "".join(f" * {line}\n" if line else " *\n" for line in text.strip("\n").split("\n"))


def write_c(path_c, path_h, name, info, size, hinting, chars_spec, font, notice):
    glyphs, data, line_height, ascent, fallback, _, _ = font
    base = os.path.basename(path_h)
    guard = "CYDUI_FONT_" + re.sub(r"[^A-Za-z0-9]", "_", name).upper() + "_H"

    with open(path_h, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* Generated by tools/fontgen.py. Do not edit.\n")
        if notice:
            f.write(comment_lines(notice))
        f.write(" */\n")
        f.write(f"#ifndef {guard}\n#define {guard}\n\n#include \"cydui/font.h\"\n\n")
        f.write(f"#ifdef __cplusplus\nextern \"C\" {{\n#endif\n\n")
        f.write(f"/* {info}, {size} px, {hinting} hinting, {len(glyphs)} glyphs, "
                f"{len(data)} bytes of bitmap */\n")
        f.write(f"extern const cydui_font {name};\n\n")
        f.write(f"#ifdef __cplusplus\n}}\n#endif\n\n#endif /* {guard} */\n")

    with open(path_c, "w", encoding="utf-8", newline="\n") as f:
        f.write("/* Generated by tools/fontgen.py. Do not edit.\n")
        f.write(f" * Derived from: {info}, {size} px, {hinting} hinting\n")
        f.write(f" * Characters: {chars_spec}\n")
        if notice:
            f.write(" *\n" + comment_lines(notice))
        f.write(" */\n\n")
        f.write(f"#include \"{base}\"\n\n")
        f.write(f"static const uint8_t {name}_bitmap[] = {{\n")
        if data:
            for i in range(0, len(data), 16):
                f.write("    " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",\n")
        else:
            f.write("    0x00, /* no pixels in this font */\n")
        f.write("};\n\n")
        f.write(f"static const cydui_glyph {name}_glyphs[] = {{\n")
        for g in glyphs:
            f.write(f"    {{0x{g.cp:04X}, {g.offset}, {g.w}, {g.h}, {g.left}, {g.top}, "
                    f"{g.advance}}},\n")
        f.write("};\n\n")
        f.write(f"const cydui_font {name} = {{\n")
        f.write(f"    {name}_glyphs, {len(glyphs)},\n")
        f.write(f"    {name}_bitmap, {len(data)},\n")
        f.write(f"    {line_height}, {ascent}, {fallback},\n")
        f.write("};\n")


def preview(font, text):
    """ASCII art of `text` set in the generated font, straight from the quantized data."""
    glyphs, _, line_height, ascent, _, _, _ = font
    by_cp = {g.cp: g for g in glyphs}
    box = by_cp[FALLBACK_CP]
    width = sum(by_cp.get(ord(c), box).advance for c in text) + 2
    canvas = [[0] * width for _ in range(line_height + 2)]
    pen = 1
    for c in text:
        g = by_cp.get(ord(c), box)
        for y, row in enumerate(g.rows):
            for x, v in enumerate(row):
                cy, cx = ascent - g.top + y, pen + g.left + x
                if 0 <= cy < len(canvas) and 0 <= cx < width:
                    canvas[cy][cx] = max(canvas[cy][cx], v)
        pen += g.advance
    ramp = " .:-=+*#%@"
    for row in canvas:
        print("".join(ramp[min(9, v * 10 // 16)] for v in row).rstrip())


def main():
    ap = argparse.ArgumentParser(description="Generate a cydui bitmap font from a TTF/OTF file.")
    ap.add_argument("--ttf", required=True)
    ap.add_argument("--size", type=int, required=True, help="pixel size (em height)")
    ap.add_argument("--name", required=True, help="C identifier of the generated cydui_font")
    ap.add_argument("--chars", required=True, help="code points to include (see --help header)")
    ap.add_argument("--out", help="output path without extension: writes OUT.c and OUT.h")
    ap.add_argument("--hinting", choices=sorted(HINTING), default="light")
    ap.add_argument("--preview", metavar="TEXT", help="print TEXT as ASCII art and exit")
    ap.add_argument("--notice", default="", help="license/attribution text put in the file headers")
    ap.add_argument("--strict", action="store_true", help="fail if the font lacks a requested glyph")
    args = ap.parse_args()

    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", args.name):
        ap.error("--name must be a C identifier")
    if not 4 <= args.size <= 200:
        ap.error("--size must be between 4 and 200")
    if not args.out and not args.preview:
        ap.error("give --out and/or --preview")

    cps = parse_chars(args.chars)
    font = build_font(args.ttf, args.size, cps, args.hinting)
    glyphs, data, line_height, ascent, fallback, missing, info = font

    if missing:
        shown = " ".join(f"U+{c:04X}" for c in missing[:12]) + (" ..." if len(missing) > 12 else "")
        print(f"note: {len(missing)} requested code points are not in the font: {shown}",
              file=sys.stderr)
        if args.strict:
            return 1
    print(f"{args.name}: {len(glyphs)} glyphs, {len(data)} bytes of bitmap, "
          f"line {line_height}, ascent {ascent}", file=sys.stderr)

    if args.preview:
        preview(font, args.preview)
    if args.out:
        write_c(args.out + ".c", args.out + ".h", args.name, info, args.size, args.hinting,
                args.chars, font, args.notice)
    return 0


if __name__ == "__main__":
    sys.exit(main())
