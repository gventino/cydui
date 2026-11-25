#!/usr/bin/env python3
"""NFR-42: every public function of include/cydui/*.h has a documentation comment.

A prototype counts as documented when a /** ... */ block ends right above it (blank lines allowed), or
when it directly follows a documented prototype with nothing but blank lines or a trailing comment between
(a family such as cydui_focus_set / cydui_focus_clear shares one block). Static inline functions of the
headers are checked the same way. Exits 1 and lists the undocumented ones.

Usage: tools/check_docs.py
"""
import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

PROTO = re.compile(r"^[A-Za-z_][\w \t\*]*?\b(cydui_\w+)\s*\([^;{]*\)\s*(?:;|\{)", re.M)


def check(path):
    text = open(path, encoding="utf-8").read()
    bad = []
    prev_documented_end = -1
    for m in PROTO.finditer(text):
        head = text[: m.start()]
        if re.search(r"^\s*#\s*define", text[text.rfind("\n", 0, m.start()) + 1: m.start()]):
            continue
        before = head.rstrip()
        documented = before.endswith("*/") and before.rfind("/**") > before.rfind("*/", 0, len(before) - 2)
        between = text[prev_documented_end:m.start()] if prev_documented_end >= 0 else None
        if not documented and between is not None and re.fullmatch(r"\s*(/\*[^\n]*\*/)?\s*", between):
            documented = True  # follows a documented prototype directly
        if documented:
            prev_documented_end = m.end()
        else:
            bad.append((m.group(1), text.count("\n", 0, m.start()) + 1))
    return bad


def main():
    failed = 0
    for path in sorted(glob.glob(os.path.join(ROOT, "include", "cydui", "*.h"))):
        for name, line in check(path):
            print(f"{os.path.relpath(path, ROOT)}:{line}: {name} has no documentation comment")
            failed += 1
    if failed:
        print(f"check_docs: {failed} undocumented public function(s)")
        return 1
    print("check_docs: every public function is documented")
    return 0


if __name__ == "__main__":
    sys.exit(main())
