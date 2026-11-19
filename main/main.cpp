#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app_state.h"
#include "ble_manager.h"
#include "buttons.h"
#include "config.h"
#include "demo.h"
#include "display.h"
#include "fonts.h"
#include "settings.h"
#include "touch.h"
#include "heart_rate_ble.h"
#include "line_store.h"
#include "racebox_ble.h"
#include "status_led.h"
#include "telemetry.h"
#include "ui.h"
#include "version.h"

namespace {

constexpr char kTag[] = "main";

void init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(kTag, "%s v%s starting", FIRMWARE_NAME, FIRMWARE_VERSION);

    init_nvs();
    app_state_init();

    telemetry::Line line;
    if (line_store::load(&line)) {
        telemetry::set_line(line);
        ESP_LOGI(kTag, "Start/finish line restored (heading %.1f)", (double)line.heading_deg);
    }

    settings::load(kDemoTelemetry);
    display_init();
    display_set_brightness(settings::get().brightness);
    fonts::init();
    touch_init();
    ui_create();
    status_led::init(kRgbLedPin, kRgbLedCount);
    buttons_init();
    if (settings::get().demo) {
        demo_set_enabled(true);
    }
    ble_manager_init();

    int64_t last_led_ms = 0;
    for (;;) {
        const uint32_t wait = lv_timer_handler();

        const int64_t now_ms = lv_tick_get();
        if (now_ms - last_led_ms >= 20) {
            last_led_ms = now_ms;
            status_led::set_racebox_state(racebox_ble::state());
            status_led::set_gopro_state(app_state_get_conn(), app_state_get_recording());
            status_led::set_heart_rate_connected(heart_rate_ble_is_connected());
            status_led::tick();
        }

        vTaskDelay(pdMS_TO_TICKS(wait < 5 ? 5 : (wait > 20 ? 20 : wait)));
    }
}
