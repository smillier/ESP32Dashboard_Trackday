#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Registers the touch panel as an LVGL pointer. Call after display_init().
 * Waveshare ESP32-S3: FT6336U capacitive controller on the I2C bus.
 * Freenove ESP32: XPT2046 resistive controller on the display SPI bus.
 * Returns false (and the dash simply has no touch) if the controller does
 * not answer. */
bool touch_init(void);

#ifdef __cplusplus
}
#endif
