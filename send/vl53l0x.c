#include "vl53l0x.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "VL53L0X";

static esp_err_t write_reg(vl53l0x_t *dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev->dev_handle, buf, 2, pdMS_TO_TICKS(100));
}

/* FIX: dung transmit_receive thay vi transmit + receive rieng */
static esp_err_t read_reg(vl53l0x_t *dev, uint8_t reg, uint8_t *data, size_t len) {
    return i2c_master_transmit_receive(dev->dev_handle, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

esp_err_t vl53l0x_init(vl53l0x_t *dev, i2c_master_bus_handle_t bus) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = VL53L0X_ADDR,
        .scl_speed_hz    = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev->dev_handle));

    // Kiem tra Model ID
    uint8_t model_id = 0;
    esp_err_t ret = read_reg(dev, 0xC0, &model_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "VL53L0X khong phan hoi I2C!");
        return ret;
    }
    ESP_LOGI(TAG, "Model ID: 0x%02X (expected 0xEE)", model_id);

    // Unlock registers
    write_reg(dev, 0x88, 0x00);
    write_reg(dev, 0x80, 0x01);
    write_reg(dev, 0xFF, 0x01);
    write_reg(dev, 0x00, 0x00);

    // Luu stop variable
    uint8_t stop_var = 0;
    read_reg(dev, 0x91, &stop_var, 1);
    dev->stop_variable = stop_var;

    // Lock lai
    write_reg(dev, 0x00, 0x01);
    write_reg(dev, 0xFF, 0x00);
    write_reg(dev, 0x80, 0x00);

    ESP_LOGI(TAG, "VL53L0X initialized (stop_var=0x%02X)", stop_var);
    return ESP_OK;
}

esp_err_t vl53l0x_read_distance(vl53l0x_t *dev, uint16_t *distance_mm) {
    // Trigger single-shot
    write_reg(dev, 0x80, 0x01);
    write_reg(dev, 0xFF, 0x01);
    write_reg(dev, 0x00, 0x00);
    write_reg(dev, 0x91, dev->stop_variable);
    write_reg(dev, 0x00, 0x01);
    write_reg(dev, 0xFF, 0x00);
    write_reg(dev, 0x80, 0x00);
    write_reg(dev, 0x00, 0x01);

    // Cho do xong (reg 0x00 = 0x00)
    uint8_t val = 0x01;
    for (int i = 0; i < 100; i++) {
        read_reg(dev, 0x00, &val, 1);
        if (val == 0x00) break;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    if (val != 0x00) return ESP_ERR_TIMEOUT;

    // Cho data ready (reg 0x13 bit[0..2])
    uint8_t status = 0;
    for (int i = 0; i < 100; i++) {
        read_reg(dev, 0x13, &status, 1);
        if (status & 0x07) break;
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    // Doc khoang cach tai reg 0x1E
    uint8_t buf[2];
    esp_err_t ret = read_reg(dev, 0x1E, buf, 2);
    if (ret != ESP_OK) return ret;

    *distance_mm = ((uint16_t)buf[0] << 8) | buf[1];

    // Clear interrupt
    write_reg(dev, 0x0B, 0x01);
    return ESP_OK;
}
