#!/usr/bin/env python3
"""Writes the seed inputs of tests/fuzz_ui.c for what came with the themes, the scroll areas, the lists,
the alignment, the cards and the custom widget. Each seed is a small program in the byte language the
harness reads (see do_op in tests/fuzz_ui.c); the fuzzer mutates from there.

Usage: make_fuzz_seeds.py [out_dir]     (default: tests/fuzz_seeds)
"""
import os
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "tests", "fuzz_seeds")

# ---- the language ----
LABEL, BUTTON, CHECK, TOGGLE = 0, 1, 2, 3
ROW, END_ROW, COL, END_COL = 5, 6, 7, 8
SPACER, NEXT_W, NEXT_H, NEXT_FILL, NEXT_STRETCH, NEXT_DIS = 14, 16, 17, 18, 19, 20
ALLOC = 24
EVENT, CLOCK, FRAME = 25, 26, 27
SLIDER, PROGRESS, TABS, RATING = 35, 36, 37, 38
SCROLL_BEGIN, SCROLL_END, SCROLL_TO = 39, 40, 41
LIST_BEGIN, LIST_ITEM, LIST_ITEM_END, LIST_END = 42, 43, 44, 45
CROSS, MAIN, CARD, CARD_END = 46, 47, 48, 49
THEME, TICK = 50, 51

DOWN, UP, MOVE = 0, 1, 2


def header(state_entries=12, cmds=90, clock=True, big_state=True):
    """state table size byte, collision set (none: one byte), command list size, flags."""
    flags = (0 if big_state else 1) | (0 if clock else 4)
    return bytes([state_entries % 13, 0, cmds - 4, flags])


def small(value, rng):
    """The byte that the harness reads as `value` from small(r, rng)."""
    b = (value + rng // 4) % rng
    assert b != 0xFF and (b % rng - rng // 4) == value, (value, rng)
    return b


def ext(kind, *extra):
    """ext32: kind 0..7 = 0, 1, -1, INT32_MIN, INT32_MAX, 4 bytes follow, small, a byte."""
    return bytes([kind]) + bytes(extra)


def touch(kind, x, y):
    return bytes([EVENT, kind, small(x, 200), small(y, 150)])


def tick(ms7):
    return bytes([CLOCK, ms7])


def frame():
    return bytes([FRAME])


def theme(which, seed=0, big=0):
    return bytes([THEME, which]) + seed.to_bytes(4, "little") + bytes([big])


def scroll_area(n_buttons, view_h=100, label=0):
    b = bytes([SCROLL_BEGIN, label, small(view_h, 150)])
    for i in range(n_buttons):
        b += bytes([BUTTON, 1 + i % 6])
    return b + bytes([SCROLL_END])


def write(name, data):
    path = os.path.join(OUT, name)
    with open(path, "wb") as f:
        f.write(data)
    print("%-28s %4d bytes" % (name, len(data)))


def main():
    # ---- a drag over a scroll area with buttons in it, then a release that coasts ----
    area = scroll_area(10)
    s = header()
    s += area + touch(DOWN, 40, 60) + frame()
    s += area + touch(MOVE, 40, 40) + tick(3) + frame()
    s += area + touch(MOVE, 40, 5) + tick(3) + frame()
    s += area + touch(MOVE, 40, -10) + tick(2) + frame()
    s += area + touch(UP, 40, -10) + tick(2) + frame()
    for _ in range(4):
        s += area + tick(2) + frame()
    write("ui_scroll_drag.bin", s)

    # ---- lists: a huge count, an empty one, a negative item height, items built, jumps ----
    def lst(count_kind, item_h, view_h, outputs=1, items=3, label=0, count_extra=()):
        b = bytes([LIST_BEGIN, label]) + ext(count_kind, *count_extra) + bytes([small(item_h, 80), small(view_h, 150), outputs])
        for i in range(items):
            b += bytes([LIST_ITEM, 1, i, LABEL, 1, LIST_ITEM_END])
        return b + bytes([LIST_END])

    s = header()
    s += lst(4, 30, 100) + bytes([SCROLL_TO, 0]) + ext(4) + frame()      # INT32_MAX rows, then jumped to the end
    s += lst(4, 30, 100) + touch(DOWN, 50, 60) + frame()
    s += lst(4, 30, 100) + touch(MOVE, 50, 20) + tick(4) + frame()
    s += lst(0, 30, 100) + frame()                                        # no rows
    s += lst(1, -10, 100) + frame()                                       # a negative item height
    s += lst(4, 0, 100) + frame()                                         # a zero one
    s += lst(5, 30, 100, count_extra=(255, 255, 255, 127)) + frame()
    s += lst(4, 59, -30) + frame()                                        # a window of negative height
    s += lst(4, 30, 100) + bytes([SCROLL_TO, 0]) + ext(3) + frame()       # INT32_MIN
    for index_kind in (4, 3):                                             # an item with an index at the very edge
        s += bytes([LIST_BEGIN, 0]) + ext(4) + bytes([small(30, 80), small(100, 150), 1])
        s += bytes([LIST_ITEM, 0]) + ext(index_kind) + bytes([LABEL, 1, LIST_ITEM_END, LIST_END]) + frame()
    write("ui_list_extremes.bin", s)

    # ---- alignment and cards: rows centered next to a button, groups at the end, nested cards ----
    card = bytes([NEXT_W, small(80, 120), CARD, CROSS, 1, LABEL, 1])
    card += bytes([ROW, CROSS, 1, LABEL, 2, BUTTON, 3, END_ROW])
    card += bytes([CHECK, 4, 0, NEXT_STRETCH, ROW, MAIN, 2, BUTTON, 1, BUTTON, 2, END_ROW])
    card += bytes([CARD, SPACER, small(10, 60), CARD_END, CARD_END])
    s = header()
    for _ in range(6):
        s += bytes([MAIN, 1, CROSS, 1]) + card + frame()
    s += bytes([CROSS, 77, MAIN, 255]) + card + frame()                   # places that do not exist
    s += bytes([COL, CROSS, 2, ROW, MAIN, 1, BUTTON, 1, END_ROW, END_COL]) + card + frame()
    write("ui_align_cards.bin", s)

    # ---- themes: good ones, ones built from numbers, bad ones, between frames of widgets ----
    body = bytes([CARD, LABEL, 1, BUTTON, 2, TOGGLE, 3, 0, SLIDER, 4, 0, ext(7, 50)[0], 0, 0, 100 & 255]) + bytes([CARD_END])
    body = bytes([CARD, LABEL, 1, BUTTON, 2, CARD_END, PROGRESS]) + ext(6, 50) + ext(4) + bytes([TABS, 3, 6, 5, 1, 0, 0])
    s = header()
    for w, seed, big in ((1, 0, 0), (2, 0, 0), (5, 0x1234, 0), (5, 0xDEADBEEF, 1), (3, 0, 0), (4, 0, 0), (0, 0, 0),
                         (5, 0x0BADF00D, 0), (1, 0, 0)):
        s += theme(w, seed, big) + body + frame()
    write("ui_theme_switch.bin", s)

    # ---- the smallest command list, with shadows, cards and a list: it overflows all the time ----
    s = header(cmds=4)
    s += theme(1) + bytes([CARD, LABEL, 1, BUTTON, 2, CARD, BUTTON, 3, CARD_END, CARD_END]) + frame()
    s += theme(5, 0x7777, 0) + bytes([CARD, BUTTON, 1, BUTTON, 2, CARD_END]) + frame()
    s += scroll_area(4) + frame()
    s += header()[:0] + bytes([CARD, CARD, CARD, LABEL, 1, CARD_END, CARD_END, CARD_END]) + frame()
    write("ui_tiny_cmds.bin", s)

    # ---- the custom widget: taps on its stars, values at the edges of the type ----
    def rate(label, with_ptr=1, which=0, set_kind=None):
        b = bytes([RATING, label, with_ptr, which])
        if set_kind is not None:
            b += bytes([1]) + ext(set_kind)
        else:
            b += bytes([0])
        return b

    s = header(clock=False)
    s += rate(0) + touch(DOWN, 20, 20) + frame()
    s += rate(0) + touch(UP, 20, 20) + frame()
    s += rate(0, set_kind=3) + frame()      # INT32_MIN
    s += rate(0, set_kind=4) + frame()      # INT32_MAX
    s += rate(1, with_ptr=0) + frame()      # no variable
    s += bytes([NEXT_DIS]) + rate(2) + touch(DOWN, 20, 60) + frame()
    s += rate(6) + rate(6) + frame()        # the same label twice
    write("ui_custom_widget.bin", s)

    # ---- slider, progress bar and tabs with every number at the edge of its type ----
    def slider(lo, hi, ptr=1, which=0):
        return bytes([SLIDER, 0, ptr, which]) + ext(lo) + ext(hi)

    s = header()
    s += slider(3, 4) + touch(DOWN, 60, 20) + frame()                    # INT32_MIN .. INT32_MAX, pressed
    s += slider(3, 4) + touch(MOVE, 100, 25) + frame()
    s += slider(3, 4) + touch(UP, 100, 25) + frame()
    s += slider(4, 3) + frame()                                          # reversed
    s += slider(1, 1) + frame()                                          # empty
    s += slider(2, 0, ptr=0) + frame()                                   # no variable
    s += bytes([PROGRESS, 1]) + ext(3) + ext(4) + bytes([PROGRESS, 0]) + ext(4) + ext(4) + frame()
    s += bytes([TABS, 1, 30, 1, 0, 0, 1]) + ext(3) + bytes([TABS, 1, 0, 1, 0, 0, 0]) + frame()   # count 25, then -5
    s += bytes([TABS, 0, 5, 0, 0, 0, 0]) + frame()                       # no selection pointer
    write("ui_value_widgets.bin", s)


main()
