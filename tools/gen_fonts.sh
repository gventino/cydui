#!/usr/bin/env bash
# Regenerates the bundled bitmap fonts in fonts/ from Liberation Sans.
#
#   tools/gen_fonts.sh            write fonts/cydui_sans_{12,16,24}.{c,h}
#   tools/gen_fonts.sh --check    regenerate into a temp dir and fail if it differs from fonts/
#
# Needs the venv from tools/requirements.txt (tools/.venv) and the TTF. Override the defaults with
# CYDUI_PYTHON and CYDUI_TTF.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/.."

PY="${CYDUI_PYTHON:-tools/.venv/bin/python}"
TTF="${CYDUI_TTF:-/usr/share/fonts/liberation/LiberationSans-Regular.ttf}"
SIZES="12 16 24"

# ASCII, Latin-1 (Portuguese and most of western Europe), ellipsis, bullet, euro, arrows, check mark.
CHARS="U+0020-U+007E,U+00A0-U+00FF,U+2022,U+2026,U+20AC,U+2190-U+2193,U+2713"

NOTICE="Licensed under the SIL Open Font License 1.1 (fonts/LICENSE-OFL.txt), not the MIT license
of the rest of this repository. Bitmap rendering of Liberation Sans:
Digitized data copyright (c) 2010 Google Corporation with Reserved Font Arimo, Tinos and Cousine.
Copyright (c) 2012 Red Hat, Inc. with Reserved Font Name Liberation."

for tool in "$PY" "$TTF"; do
    [ -e "$tool" ] || { echo "gen_fonts.sh: missing $tool" >&2; exit 2; }
done

mode="write"
[ "${1:-}" = "--check" ] && mode="check"

out="fonts"
if [ "$mode" = "check" ]; then
    out="$(mktemp -d)"
    trap 'rm -rf "$out"' EXIT
fi

for size in $SIZES; do
    "$PY" tools/fontgen.py --ttf "$TTF" --size "$size" --name "cydui_sans_$size" \
        --chars "$CHARS" --notice "$NOTICE" --out "$out/cydui_sans_$size"
done

if [ "$mode" = "check" ]; then
    for size in $SIZES; do
        for ext in c h; do
            cmp -s "$out/cydui_sans_$size.$ext" "fonts/cydui_sans_$size.$ext" || {
                echo "gen_fonts.sh: fonts/cydui_sans_$size.$ext differs from the generator output" >&2
                exit 1
            }
        done
    done
    echo "gen_fonts.sh: bundled fonts match the generator"
fi
