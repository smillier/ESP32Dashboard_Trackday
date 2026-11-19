idf_component_register(
    SRCS
        "main.cpp"
        "display.c"
        "ui.cpp"
        "telemetry.cpp"
        "line_store.cpp"
        "demo.cpp"
        "buttons.cpp"
        "ble_manager.cpp"
        "peer_pins.c"
        "status_led.cpp"
        "racebox_ble.cpp"
        "ubx_protocol.cpp"
        "gopro_ble.c"
        "gopro_control.c"
        "heart_rate_ble.c"
        "app_state.c"
    INCLUDE_DIRS "."
    REQUIRES bt driver esp_driver_spi esp_driver_gpio esp_driver_ledc esp_driver_i2c esp_lcd esp_timer nvs_flash
)
