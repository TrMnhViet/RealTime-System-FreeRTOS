#pragma once
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdint.h>

typedef struct {
    gpio_num_t trig_pin;
    gpio_num_t echo_pin;
} hcsr04_t;

esp_err_t hcsr04_init(hcsr04_t *dev, gpio_num_t trig, gpio_num_t echo);
esp_err_t hcsr04_read_distance(hcsr04_t *dev, uint16_t *distance_cm);
