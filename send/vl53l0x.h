#pragma once
#include "driver/i2c_master.h"
#include "esp_err.h"
#include <stdint.h>

#define VL53L0X_ADDR 0x29

typedef struct {
    i2c_master_dev_handle_t dev_handle;
    uint8_t stop_variable;  // cần lưu lại từ lúc init
} vl53l0x_t;

esp_err_t vl53l0x_init(vl53l0x_t *dev, i2c_master_bus_handle_t bus);
esp_err_t vl53l0x_read_distance(vl53l0x_t *dev, uint16_t *distance_mm);
