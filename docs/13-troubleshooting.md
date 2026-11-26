# 13 — Troubleshooting on the CYD

Short answers, from the options of `backends/esp32/Kconfig` (change them with `idf.py menuconfig`, menu *cydui ESP32 backend*, or in `sdkconfig.defaults`). Run `tools/doctor.sh` first if nothing builds or flashes.

| What you see | Try |
|---|---|
| The colors are inverted (a dark theme looks like a light one) | `CONFIG_CYDUI_ESP32_LCD_INVERT=y` (off by default: the author's board needs none, other variants of this board do) |
| Red and blue are swapped | `CONFIG_CYDUI_ESP32_LCD_BGR=y` |
| The picture is upside down | `CONFIG_CYDUI_ESP32_LCD_ROTATION_180=y` (portrait is not supported) |
| The screen stays black | Is the backlight pin right? The classic CYD uses GPIO 21 (`CYDUI_ESP32_LCD_BL`); some variants use 27. The library switches the backlight on only after `cydui_esp32_set_backlight()`, so call it after the first frame |
| Garbage or noise on the screen | The SPI clock: `CONFIG_CYDUI_ESP32_LCD_SPI_MHZ=40` is the safe default; 80 worked on the author's board and may not on yours |
| A touch lands in the wrong place, mirrored, or on the other axis | Calibrate again (the *Calibrate* button of the gallery, or `cydui_esp32_calibration_erase()` and reboot): five touches on crosses, a stylus is better than a finger. If it is still mirrored: `CONFIG_CYDUI_ESP32_TOUCH_FLIP_X` or `_FLIP_Y` |
| The calibration is refused ("asks again") | It is refused when a target ends up farther than `CONFIG_CYDUI_ESP32_CAL_MAX_RESIDUAL` pixels from where the fit puts it: touch the center of each cross, without sliding |
| There is no touch at all, but the screen works | `cydui_esp32_touch_status()` says why (the display and the touch start apart); check the touch pins in the menu |
| `idf.py flash` says permission denied | Add yourself to the group of the port (`dialout` on Debian and Ubuntu, `uucp` on Arch) and log in again; a data cable, not a charge-only one |
| `ESP-IDF not found` | `source tools/env.sh` expects `~/esp/esp-idf`; set `CYDUI_IDF_PATH` if it is elsewhere. The library is built on ESP-IDF 5.5.x |
| Things get dropped (a button missing, text missing) | Ask `cydui_ui_errors(&ui)`: `CYDUI_ERR_CMD_OVERFLOW` (more commands per frame: raise the 4th number of `CYDUI_STORAGE`), `CYDUI_ERR_NO_MEMORY` or the text memory, `CYDUI_ERR_STATE_FULL` (more state entries). `cydui_ui_stats_get()` tells how much the busiest screen really uses |
| The screen flickers or the touch is slow | Give `dirty_mem` to the UI so that only the strips that changed are sent (the starter does), and keep animations to what changes; at 40 MHz sending a whole screen takes about 34 ms whatever the drawing speed |
| `Two widgets act together` | The same label twice in one scope: use `"Label##2"` or `cydui_push_id_int` in loops |

Still stuck? Open an issue with the board's name, the `sdkconfig` lines about `CYDUI_ESP32_*` and what the monitor prints.
