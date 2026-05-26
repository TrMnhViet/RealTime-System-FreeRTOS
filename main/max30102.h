#pragma once
#include "driver/i2c_master.h"

#define MAX30102_ADDR       0x57
#define MAX30102_REG_INTR1  0x00
#define MAX30102_REG_FIFO_WR 0x04
#define MAX30102_REG_FIFO_RD 0x05
#define MAX30102_REG_OVF    0x06
#define MAX30102_REG_FIFO   0x07
#define MAX30102_REG_FIFO_CFG 0x08
#define MAX30102_REG_MODE   0x09
#define MAX30102_REG_SPO2   0x0A
#define MAX30102_REG_LED1   0x0C
#define MAX30102_REG_LED2   0x0D
#define MAX30102_REG_PART_ID 0xFF

typedef struct {
    i2c_master_dev_handle_t dev_handle;
} max30102_t;

esp_err_t max30102_init(max30102_t *dev, i2c_master_bus_handle_t bus);
esp_err_t max30102_read_fifo(max30102_t *dev, uint32_t *red, uint32_t *ir);