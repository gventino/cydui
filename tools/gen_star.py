#!/usr/bin/env python3
"""Generates the 20x20 star of examples/theme-gallery/rating.c as a 4-bit coverage mask
(CYDUI_BITMAP_A4: two pixels per byte, high nibble first, rows padded to a byte).

Coverage comes from 8x8 supersampling of a regular five-pointed star, so the edges are smooth.
Usage: gen_star.py           prints the C table
"""
import math

SIZE = 20
SS = 8
R_OUT = 9.6
R_IN = R_OUT * 0.382


def star_points():
    pts = []
    for k in range(10):
        r = R_OUT if k % 2 == 0 else R_IN
        a = -math.pi / 2 + k * math.pi / 5
        pts.append((SIZE / 2 + r * math.cos(a), SIZE / 2 + r * math.sin(a)))
    return pts


def inside(poly, x, y):
    c = False
    j = len(poly) - 1
    for i in range(len(poly)):
        xi, yi = poly[i]
        xj, yj = poly[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            c = not c
        j = i
    return c


def main():
    poly = star_points()
    rows = []
    for py in range(SIZE):
        row = []
        for px in range(SIZE):
            n = 0
            for sy in range(SS):
                for sx in range(SS):
                    if inside(poly, px + (sx + 0.5) / SS, py + (sy + 0.5) / SS):
                        n += 1
            row.append(round(15 * n / (SS * SS)))
        rows.append(row)
    print("static const uint8_t star_mask[%d] = {" % (SIZE // 2 * SIZE))
    for row in rows:
        bytes_ = ["0x%02X" % ((row[i] << 4) | row[i + 1]) for i in range(0, SIZE, 2)]
        print("    " + ", ".join(bytes_) + ",")
    print("};")
    print("/* center pixel coverage: %d */" % rows[SIZE // 2][SIZE // 2])


main()
