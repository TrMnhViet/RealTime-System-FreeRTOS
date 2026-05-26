// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"

// #include "joystick_motor.h"
// #include "mode_manager.h"
// #include "mqtt_control.h"
// #include "uart_control.h"

// static const char *TAG = "MAIN";

// void app_main(void)
// {
//     ESP_LOGI(TAG, "=== Robot Multi-Mode Boot ===");

//     /* 1. Khởi tạo phần cứng dùng chung */
//     adc_init();
//     pwm_init();

//     /* 2. Khởi tạo mode manager (TWAI/CAN + event group) */
//     mode_manager_init();

//     /* 3. Khởi tạo MQTT (WiFi + broker) */
//     mqtt_control_init();

//     /* 4. Khởi tạo UART (Jetson Nano) */
//     // uart_control_init();

//     ESP_LOGI(TAG, "Tat ca peripheral da san sang. Khoi tao task...");

//     /* 5. Tạo 3 task mode — chạy song song, tự block khi không active */
//     xTaskCreate(joystick_task,      "task_joystick", 4096, NULL, 5, NULL);
//     xTaskCreate(mqtt_control_task,  "task_mqtt",     4096, NULL, 5, NULL);
//     xTaskCreate(uart_control_task,  "task_uart",     2048, NULL, 5, NULL);

//     ESP_LOGI(TAG, "He thong khoi dong thanh cong.");
//     ESP_LOGI(TAG, "Mode mac dinh: JOYSTICK (doi CAN frame ID=0x100 de chuyen)");
// }
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "joystick_motor.h"
#include "mode_manager.h"
#include "mqtt_control.h"
#include "uart_control.h"
#include "sensor_monitor.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Robot Multi-Mode Boot ===");

    /* 1. Khởi tạo phần cứng dùng chung */
    adc_init();
    pwm_init();

    /* 2. Khởi tạo mode manager (TWAI/CAN + event group) */
    mode_manager_init();

    /* 3. Khởi tạo MQTT (WiFi + broker) */
    mqtt_control_init();

    /* 4. Khởi tạo UART (Jetson Nano) */
    // uart_control_init();

    /* 5. Khởi tạo sensor monitor (HC-SR04 + MAX30102) */
    sensor_monitor_init();

    ESP_LOGI(TAG, "Tat ca peripheral da san sang. Khoi tao task...");

    /* 6. Tạo các task — chạy song song, tự block khi không active */
    xTaskCreate(joystick_task,          "task_joystick",  4096, NULL, 5, NULL);
    xTaskCreate(mqtt_control_task,      "task_mqtt",      4096, NULL, 5, NULL);
    xTaskCreate(uart_control_task,      "task_uart",      2048, NULL, 5, NULL);

    /* Priority 8 — cao hơn task motor để dừng khẩn cấp kịp thời */
    xTaskCreate(sensor_monitor_task,    "task_sensor_mon",4096, NULL, 8, NULL);

    ESP_LOGI(TAG, "He thong khoi dong thanh cong.");
    ESP_LOGI(TAG, "Mode mac dinh: JOYSTICK ");
}