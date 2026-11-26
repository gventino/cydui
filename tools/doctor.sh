#!/usr/bin/env bash
# Checks what this computer has for cydui and says how to get what is missing. Changes nothing.
#   tools/doctor.sh
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
bad=0
ok()   { printf '  [ok]   %s\n' "$1"; }
warn() { printf '  [--]   %s\n         %s\n' "$1" "$2"; }
miss() { printf '  [MISS] %s\n         %s\n' "$1" "$2"; bad=1; }
have() { command -v "$1" >/dev/null 2>&1; }

echo "On the computer (the PC build, the tests, the examples with a window):"
for t in gcc cmake ninja; do
    if have "$t"; then ok "$t"; else miss "$t" "install it with your package manager (apt install build-essential cmake ninja-build / pacman -S base-devel cmake ninja)"; fi
done
if have pkg-config && pkg-config --exists sdl2 2>/dev/null; then ok "SDL2 (the examples open a window)"
else warn "SDL2" "only needed for the windows; without it the examples still run with --headless. apt install libsdl2-dev / pacman -S sdl2"; fi

echo "On the board (ESP-IDF 5.5):"
idf="${CYDUI_IDF_PATH:-$HOME/esp/esp-idf}"
if [ -f "$idf/export.sh" ]; then
    ok "ESP-IDF at $idf"
    ver="$(cat "$idf/version.txt" 2>/dev/null || git -C "$idf" describe --tags 2>/dev/null || true)"
    case "$ver" in v5.5*|5.5*) ok "version $ver" ;; "") warn "ESP-IDF version unknown" "the library expects 5.5.x" ;; *) warn "ESP-IDF $ver" "the library is built and tested on 5.5.x" ;; esac
else
    miss "ESP-IDF not found at $idf" "install 5.5 (https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32/get-started/) or set CYDUI_IDF_PATH"
fi
port=""
for p in /dev/ttyUSB* /dev/ttyACM*; do [ -e "$p" ] && port="$p" && break; done
if [ -n "$port" ]; then
    ok "a board on $port"
    if [ -r "$port" ] && [ -w "$port" ]; then ok "you may use it"
    else miss "no permission on $port" "add yourself to the group of the port: sudo usermod -aG dialout \$USER (Debian, Ubuntu) or uucp (Arch), then log in again"; fi
else
    warn "no board found on /dev/ttyUSB* or /dev/ttyACM*" "plug it in with a data cable (some cables only charge)"
fi

echo
if [ "$bad" = 0 ]; then
    echo "Ready. Try the PC first:   cmake -S $here -B $here/build -G Ninja && cmake --build $here/build && $here/build/examples/hello/hello"
    echo "Then the board:            $here/tools/new-project.sh ~/my-gadget"
else
    echo "Fix the [MISS] lines above and run this again."
fi
exit "$bad"
