# @NAME@

A cydui project for the ESP32 Cheap Yellow Display.

```sh
source @CYDUI_PATH@/tools/env.sh      # or your own `. $IDF_PATH/export.sh`
idf.py build flash monitor            # add -p /dev/ttyUSB0 if it is not found
```

The first start asks for five touches on crosses (a stylus is better than a finger) and keeps the result in the flash.
Edit `build_screen()` in `main/main.c`. The guide is `@CYDUI_PATH@/docs/12-guide.md`; if the picture or the touch looks
wrong, `@CYDUI_PATH@/docs/13-troubleshooting.md`.
