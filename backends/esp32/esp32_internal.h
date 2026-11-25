#ifndef CYDUI_ESP32_INTERNAL_H
#define CYDUI_ESP32_INTERNAL_H

/* Between cydui_esp32.c (the display) and esp32_touch.c. Not part of the API. */

#include "cydui_esp32.h"

/** Starts the touch screen: its bus, the pen interrupt, the sampling task and the saved calibration. */
void cydui_esp32_touch_start(cydui_esp32 *d);

/** The poll_event of the backend: the events of the touch task, already in pixels. */
bool cydui_esp32_poll_event(void *user, cydui_event *ev);

#endif /* CYDUI_ESP32_INTERNAL_H */
