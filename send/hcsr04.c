#include "hcsr04.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG         "HCSR04"
#define TIMEOUT_US  30000   // 30ms timeout (~5m max range)

esp_err_t hcsr04_init(hcsr04_t *dev, gpio_num_t trig, gpio_num_t echo)
{
    dev->trig_pin = trig;
    dev->echo_pin = echo;

    // Cau hinh TRIG: output
    gpio_config_t trig_cfg = {
        .pin_bit_mask = (1ULL << trig),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&trig_cfg);
    gpio_set_level(trig, 0);

    // Cau hinh ECHO: input
    gpio_config_t echo_cfg = {
        .pin_bit_mask = (1ULL << echo),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&echo_cfg);

    ESP_LOGI(TAG, "HC-SR04 init xong (TRIG=GPIO%d, ECHO=GPIO%d)", trig, echo);
    return ESP_OK;
}

esp_err_t hcsr04_read_distance(hcsr04_t *dev, uint16_t *distance_cm)
{
    // 1. Gui xung TRIG 10us
    gpio_set_level(dev->trig_pin, 0);
    esp_rom_delay_us(2);
    gpio_set_level(dev->trig_pin, 1);
    esp_rom_delay_us(10);
    gpio_set_level(dev->trig_pin, 0);

    // 2. Cho ECHO len HIGH
    int64_t t_start = esp_timer_get_time();
    while (gpio_get_level(dev->echo_pin) == 0) {
        if ((esp_timer_get_time() - t_start) > TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout cho ECHO len HIGH");
            return ESP_ERR_TIMEOUT;
        }
    }

    // 3. Do thoi gian ECHO o muc HIGH
    int64_t t_echo_start = esp_timer_get_time();
    while (gpio_get_level(dev->echo_pin) == 1) {
        if ((esp_timer_get_time() - t_echo_start) > TIMEOUT_US) {
            ESP_LOGW(TAG, "Timeout ECHO qua dai (ngoai tam do)");
            return ESP_ERR_TIMEOUT;
        }
    }
    int64_t t_echo_end = esp_timer_get_time();

    // 4. Tinh khoang cach
    // Khoang cach (cm) = thoi_gian_us * toc_do_am_thanh / 2
    // = duration_us * 0.034 / 2 = duration_us / 58
    int64_t duration_us = t_echo_end - t_echo_start;
    *distance_cm = (uint16_t)(duration_us / 58);

    return ESP_OK;
}
