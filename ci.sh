#!/usr/bin/env bash
# Local CI.
#   1. gcc and clang builds with ASan/UBSan and -Werror, ctest (goldens must match on both)
#   2. a 10 s libFuzzer run on the clang build
#   3. an optimized gcc build: goldens must also match with -O2
#   4. static checks on the core library: no malloc, no mutable globals (NFR-11, NFR-21)
#   5. the ESP-IDF hw-probe build, when ESP-IDF is installed
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")"

for tool in gcc clang cmake ninja nm; do
    command -v "$tool" >/dev/null || { echo "ci.sh: $tool not found" >&2; exit 1; }
done

for cc in gcc clang; do
    dir="build/ci-$cc"
    echo "==> $cc build with ASan/UBSan"
    cmake -S . -B "$dir" -G Ninja -DCMAKE_C_COMPILER="$cc" -DCYDUI_SANITIZE=ON >/dev/null
    cmake --build "$dir"
    ctest --test-dir "$dir" --output-on-failure

    if [ -x "$dir/tests/fuzz_cmds" ]; then
        echo "==> 10 s of fuzzing ($cc)"
        mkdir -p "$dir/corpus"
        "$dir/tests/fuzz_cmds" "$dir/corpus" -max_total_time=10 -max_len=1200 >/dev/null
    fi
done

echo "==> gcc -O2 build (goldens must not depend on optimization)"
cmake -S . -B build/ci-o2 -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build/ci-o2
ctest --test-dir build/ci-o2 --output-on-failure

echo "==> static checks on libcydui.a"
lib=build/ci-o2/libcydui.a
if nm -u "$lib" | grep -E '\b(malloc|calloc|realloc|free|aligned_alloc|posix_memalign)\b'; then
    echo "ci.sh: the core references the heap (NFR-11)" >&2
    exit 1
fi
# b/B/d/D = writable data; static locals show up here too.
if nm --defined-only "$lib" | awk '$2 ~ /^[bBdDcC]$/ { print; bad = 1 } END { exit !bad }'; then
    echo "ci.sh: the core has mutable global state (NFR-21)" >&2
    exit 1
fi

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
