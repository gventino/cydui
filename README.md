# cydui

Immediate-mode UI library in C for small embedded displays, targeting the ESP32 "Cheap Yellow Display" (ESP32-2432S028R: ILI9341 320x240 + XPT2046 touch). egui-style API, strip-based rendering (no full framebuffer), swappable themes from clean/modern to retro to cyberpunk.

**Status:** iteration 0 (setup). No UI code yet. See [`docs/`](docs/) for requirements, threat model and the iteration plan.

## Building (PC)

```sh
./ci.sh                      # gcc + clang, ASan/UBSan, ctest (+ hw-probe build if ESP-IDF is installed)
```

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
