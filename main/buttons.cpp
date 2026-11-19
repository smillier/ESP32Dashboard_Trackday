#include "buttons.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "config.h"
#include "gopro_control.h"
#include "line_store.h"
#include "racebox_ble.h"
#include "telemetry.h"

namespace {

constexpr char kTag[] = "buttons";

QueueHandle_t g_evt_queue;

void IRAM_ATTR button_isr(void* arg) {
    uint32_t pin = (uint32_t)(uintptr_t)arg;
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(g_evt_queue, &pin, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

// Debounces the pin and blocks until it is released. Returns the press
// duration in ms, or -1 if it was only noise.
int debounce_and_wait_release(gpio_num_t pin) {
    vTaskDelay(pdMS_TO_TICKS(40));
    if (gpio_get_level(pin) != 0) {
        return -1;
    }
    const int64_t start = esp_timer_get_time();
    while (gpio_get_level(pin) == 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return (int)((esp_timer_get_time() - start) / 1000) + 40;
}

void mark_line_pressed(int held_ms) {
    if (held_ms >= kLongPressMs) {
        telemetry::reset_session();
        ESP_LOGI(kTag, "Session reset");
        return;
    }
    telemetry::Line line;
    if (telemetry::mark_line(&line)) {
        line_store::save(line);
        ESP_LOGI(kTag, "Start/finish line marked (heading %.1f)", (double)line.heading_deg);
    } else {
        ESP_LOGW(kTag, "Cannot mark line: need a GPS fix and some speed");
    }
}

void button_task(void*) {
    uint32_t pin;
    for (;;) {
        if (xQueueReceive(g_evt_queue, &pin, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        const int held_ms = debounce_and_wait_release((gpio_num_t)pin);
        if (held_ms < 0) {
            continue;
        }
        switch (pin) {
            case kBtnRaceBoxRec: racebox_ble::toggle_recording(); break;
            case kBtnGoProShutter: gopro_control_toggle_shutter(); break;
            case kBtnGoProMode: gopro_control_cycle_mode(); break;
            case kBtnGoProPower: gopro_control_power_button(); break;
            case kBtnMarkLine: mark_line_pressed(held_ms); break;
            default: break;
        }
    }
}

void configure_button(int pin) {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_INPUT;
    cfg.pull_up_en = GPIO_IS_VALID_OUTPUT_GPIO(pin) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    cfg.intr_type = GPIO_INTR_NEGEDGE;
    ESP_ERROR_CHECK(gpio_config(&cfg));
    ESP_ERROR_CHECK(gpio_isr_handler_add((gpio_num_t)pin, button_isr, (void*)(uintptr_t)pin));
}

} // namespace

extern "C" void buttons_init(void) {
    g_evt_queue = xQueueCreate(8, sizeof(uint32_t));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    configure_button(kBtnRaceBoxRec);
    configure_button(kBtnGoProShutter);
    configure_button(kBtnGoProMode);
    configure_button(kBtnGoProPower);
    configure_button(kBtnMarkLine);
    xTaskCreate(button_task, "buttons", 3072, nullptr, 5, nullptr);
    ESP_LOGI(kTag, "rec=%d shutter=%d mode=%d power=%d mark=%d", kBtnRaceBoxRec, kBtnGoProShutter, kBtnGoProMode,
             kBtnGoProPower, kBtnMarkLine);
}
