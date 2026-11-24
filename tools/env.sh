#!/usr/bin/env bash
# Source this file to load the ESP-IDF environment for cydui.
#   source tools/env.sh
# ESP-IDF 5.5 does not support the system Python (3.14), so we put a uv-managed
# Python 3.13 first on PATH. Nothing is written to your shell rc files.

CYDUI_IDF_PATH="${CYDUI_IDF_PATH:-$HOME/esp/esp-idf}"
CYDUI_IDF_PYTHON_DIR="${CYDUI_IDF_PYTHON_DIR:-$HOME/.local/share/uv/python/cpython-3.13-linux-x86_64-gnu/bin}"

if [ ! -f "$CYDUI_IDF_PATH/export.sh" ]; then
    echo "env.sh: ESP-IDF not found at $CYDUI_IDF_PATH" >&2
    return 1 2>/dev/null || exit 1
fi

export PATH="$CYDUI_IDF_PYTHON_DIR:$PATH"
# shellcheck disable=SC1091
. "$CYDUI_IDF_PATH/export.sh" >/dev/null
