# cydui

Immediate-mode UI library in C for small embedded displays, targeting the ESP32 "Cheap Yellow Display" (ESP32-2432S028R: ILI9341 320x240 + XPT2046 touch). egui-style API, strip-based rendering (no full framebuffer), swappable themes from clean/modern to retro to cyberpunk.

**Status:** iteration 1 done: drawing core (rect, rounded rect, line, bitmap, gradient with antialiasing), strip rasterizer, in-memory and SDL backends. No text or widgets yet. See [`docs/`](docs/) for requirements, threat model, the iteration plan and the [core design](docs/10-core-design.md).

## Building and running (PC)

```sh
./ci.sh                                   # gcc + clang with ASan/UBSan, goldens, fuzz, static checks (+ hw-probe build)

cmake -S . -B build -G Ninja && cmake --build build
ctest --test-dir build                    # unit tests and golden images
./build/examples/shapes/shapes            # demo scene in an SDL window (needs SDL2)
./build/examples/shapes/shapes --headless --hash --png shapes.png
```

Golden images: the hash of each scene is in `tests/golden/`, and every run writes the PNGs to `build/golden/`.
After an intended rendering change, look at the PNGs and run `CYDUI_UPDATE_GOLDEN=1 ./build/tests/test_golden`.

## ESP-IDF

ESP-IDF v5.5.x is expected in `~/esp/esp-idf`. Load it with:

```sh
source tools/env.sh
```

Hardware probe (finds the real pins/orientation of your board):

```sh
cd examples/hw-probe
idf.py build
idf.py -p /dev/ttyUSB0 flash
```

You need to be in the `uucp` group (Arch) or `dialout` (Debian/Ubuntu) to access the serial port.

## License

MIT
