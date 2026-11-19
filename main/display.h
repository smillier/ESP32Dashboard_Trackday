#pragma once

#include "lvgl.h"
#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S3
/* Waveshare ESP32-S3-Touch-LCD-3.5: ST7796 320x480, SPI2. CS is tied low and
 * the panel reset is on the TCA9554 I/O expander (I2C SDA 8 / SCL 7, EXIO1). */
#define PIN_LCD_MOSI    1
#define PIN_LCD_MISO    (-1)
#define PIN_LCD_SCLK    5
#define PIN_LCD_CS      (-1)
#define PIN_LCD_DC      3
#define PIN_LCD_BL      6
#define LCD_INVERT_COLORS   true
#define LCD_HAS_IO_EXPANDER_RESET 1
#define PIN_I2C_SDA     8
#define PIN_I2C_SCL     7
#define PIN_TOUCH_I2C_ADDR 0x38   /* FT6336U */
#else
/* Freenove FNK0103N: 3.5" ST7796 320x480, SPI on HSPI (SPI2). Touch unused. */
#define PIN_LCD_MOSI    13
#define PIN_LCD_MISO    12
#define PIN_LCD_SCLK    14
#define PIN_LCD_CS      15
#define PIN_LCD_DC      2
#define PIN_LCD_BL      27
#define LCD_INVERT_COLORS   false
/* Resistive XPT2046 on the same SPI bus as the display. */
#define PIN_TOUCH_CS    33
#define PIN_TOUCH_IRQ   36
#endif
#define LCD_HOST        SPI2_HOST

#define LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)
#define LCD_H_RES           480     /* landscape */
#define LCD_V_RES           320

/* Flip these if the picture is mirrored on your unit. */
#define LCD_MIRROR_X    false
#define LCD_MIRROR_Y    false

#define LCD_BRIGHTNESS_PCT  100    /* backlight level at boot, 0..100 */

#ifdef __cplusplus
extern "C" {
#endif
#if CONFIG_IDF_TARGET_ESP32S3
#include "driver/i2c_master.h"
/* I2C bus shared by the TCA9554 expander and the FT6336 touch controller. */
i2c_master_bus_handle_t display_i2c_bus(void);
#endif
lv_display_t *display_init(void);
void display_set_brightness(int pct);
#ifdef __cplusplus
}
#endif
