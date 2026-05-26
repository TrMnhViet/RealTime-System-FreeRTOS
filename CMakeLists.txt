idf_component_register(
    SRCS
        "main.c"
        "joystick_motor.c"
        "mode_manager.c"
        "mqtt_control.c"
        "uart_control.c"
        "sensor_monitor.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_adc
        esp_wifi
        mqtt
        nvs_flash
        esp_netif
        esp_event
)