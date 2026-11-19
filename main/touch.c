#include "touch.h"

#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"
#include "sdkconfig.h"

#include "display.h"

static const char *TAG = "touch";

/* The panel is used in landscape (480x320) through swap_xy. Touch
 * controllers report in the panel's native portrait frame (x 0..319,
 * y 0..479), so by default x/y are swapped. Flip these if a tap lands in the
 * wrong place on your unit. */
#define TOUCH_SWAP_XY   1
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  1

static int s_last_x, s_last_y;
static bool s_down;

/* Maps native-frame coordinates to the 480x320 landscape screen. */
static void map_point(int nx, int ny, int *x, int *y)
{
    int lx = TOUCH_SWAP_XY ? ny : nx;
    int ly = TOUCH_SWAP_XY ? nx : ny;
    if (TOUCH_INVERT_X) lx = (LCD_H_RES - 1) - lx;
    if (TOUCH_INVERT_Y) ly = (LCD_V_RES - 1) - ly;
    if (lx < 0) lx = 0;
    if (lx > LCD_H_RES - 1) lx = LCD_H_RES - 1;
    if (ly < 0) ly = 0;
    if (ly > LCD_V_RES - 1) ly = LCD_V_RES - 1;
    *x = lx;
    *y = ly;
}

#if CONFIG_IDF_TARGET_ESP32S3

static i2c_master_dev_handle_t s_ft;

/* FT6336U: register 0x02 = touch count, then x/y of the first point. */
static bool read_point(int *nx, int *ny)
{
    const uint8_t reg = 0x02;
    uint8_t d[5] = {0};
    if (i2c_master_transmit_receive(s_ft, &reg, 1, d, sizeof(d), 20) != ESP_OK) {
        return false;
    }
    if ((d[0] & 0x0F) == 0 || (d[0] & 0x0F) > 2) {
        return false;
    }
    *nx = ((d[1] & 0x0F) << 8) | d[2];
    *ny = ((d[3] & 0x0F) << 8) | d[4];
    return true;
}

static bool touch_hw_init(void)
{
    i2c_master_bus_handle_t bus = display_i2c_bus();
    if (bus == NULL) {
        return false;
    }
    if (i2c_master_probe(bus, PIN_TOUCH_I2C_ADDR, 50) != ESP_OK) {
        ESP_LOGW(TAG, "FT6336 not found at 0x%02x", PIN_TOUCH_I2C_ADDR);
        return false;
    }
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PIN_TOUCH_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    return i2c_master_bus_add_device(bus, &dev_cfg, &s_ft) == ESP_OK;
}

#else /* XPT2046 */

#include "driver/spi_master.h"

/* Raw 12-bit range of the resistive panel (adjust after a tap test). */
#define RAW_X_MIN 240
#define RAW_X_MAX 3800
#define RAW_Y_MIN 240
#define RAW_Y_MAX 3800

static spi_device_handle_t s_xpt;

static int xpt_read(uint8_t cmd)
{
    uint8_t tx[3] = {cmd, 0, 0}, rx[3] = {0};
    spi_transaction_t t = {
        .length = 24,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    if (spi_device_polling_transmit(s_xpt, &t) != ESP_OK) {
        return -1;
    }
    return ((rx[1] << 8) | rx[2]) >> 3;
}

static int scale(int raw, int lo, int hi, int out)
{
    int v = (raw - lo) * out / (hi - lo);
    return v < 0 ? 0 : (v > out - 1 ? out - 1 : v);
}

static bool read_point(int *nx, int *ny)
{
    if (gpio_get_level(PIN_TOUCH_IRQ) != 0) {
        return false;                       /* IRQ is low while pressed */
    }
    int sx = 0, sy = 0, n = 0;
    for (int i = 0; i < 4; i++) {
        const int rx = xpt_read(0xD0);      /* X, differential */
        const int ry = xpt_read(0x90);      /* Y, differential */
        if (rx < 0 || ry < 0) continue;
        sx += rx;
        sy += ry;
        n++;
    }
    if (n == 0 || gpio_get_level(PIN_TOUCH_IRQ) != 0) {
        return false;
    }
    *nx = scale(sx / n, RAW_X_MIN, RAW_X_MAX, LCD_V_RES);   /* native x: 0..319 */
    *ny = scale(sy / n, RAW_Y_MIN, RAW_Y_MAX, LCD_H_RES);   /* native y: 0..479 */
    return true;
}

static bool touch_hw_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << PIN_TOUCH_IRQ,
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&io);
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = PIN_TOUCH_CS,
        .queue_size = 1,
    };
    return spi_bus_add_device(LCD_HOST, &dev, &s_xpt) == ESP_OK;
}

#endif

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    int nx, ny;
    if (read_point(&nx, &ny)) {
        map_point(nx, ny, &s_last_x, &s_last_y);
        if (!s_down) {
            ESP_LOGI(TAG, "touch native (%d,%d) -> screen (%d,%d)", nx, ny, s_last_x, s_last_y);
        }
        s_down = true;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        s_down = false;
        data->state = LV_INDEV_STATE_RELEASED;
    }
    data->point.x = s_last_x;
    data->point.y = s_last_y;
}

bool touch_init(void)
{
    if (!touch_hw_init()) {
        ESP_LOGW(TAG, "No touch controller, touch disabled");
        return false;
    }
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_cb);
    lv_indev_set_display(indev, lv_display_get_default());
    ESP_LOGI(TAG, "Touch ready");
    return true;
}
