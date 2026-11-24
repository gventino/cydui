#!/usr/bin/env bash
# Local CI: host build with gcc and clang (ASan/UBSan, -Werror), ctest, and the
# ESP-IDF hw-probe build when ESP-IDF is available.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

for cc in gcc clang; do
    if ! command -v "$cc" >/dev/null; then
        echo "ci.sh: $cc not found" >&2
        exit 1
    fi
    dir="build/ci-$cc"
    echo "==> host build with $cc"
    cmake -S . -B "$dir" -G Ninja -DCMAKE_C_COMPILER="$cc" -DCYDUI_SANITIZE=ON >/dev/null
    cmake --build "$dir"
    ctest --test-dir "$dir" --output-on-failure
done

if [ -d examples/hw-probe ] && [ -f "${CYDUI_IDF_PATH:-$HOME/esp/esp-idf}/export.sh" ]; then
    echo "==> ESP-IDF build of hw-probe"
    (
        # shellcheck disable=SC1091
        source tools/env.sh
        cd examples/hw-probe
        idf.py build
    )
else
    echo "==> skipping hw-probe build (ESP-IDF or example not present)"
fi

echo "ci.sh: all good"
