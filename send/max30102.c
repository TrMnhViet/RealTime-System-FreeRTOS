#include "max30102.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAX30102";

static esp_err_t write_reg(max30102_t *dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(dev->dev_handle, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t read_reg(max30102_t *dev, uint8_t reg, uint8_t *out, size_t len) {
    return i2c_master_transmit_receive(dev->dev_handle, &reg, 1, out, len, pdMS_TO_TICKS(100));
}

esp_err_t max30102_init(max30102_t *dev, i2c_master_bus_handle_t bus) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MAX30102_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev->dev_handle));

    // Reset
    write_reg(dev, MAX30102_REG_MODE, 0x40);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Kiem tra Part ID
    uint8_t part_id = 0;
    esp_err_t ret = read_reg(dev, MAX30102_REG_PART_ID, &part_id, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Doc Part ID that bai");
        return ret;
    }
    ESP_LOGI(TAG, "Part ID: 0x%02X (expected 0x15)", part_id);

    // FIFO: 4 samples average, FIFO rollover on
    write_reg(dev, MAX30102_REG_FIFO_CFG, 0x4F);
    // Mode: SpO2
    write_reg(dev, MAX30102_REG_MODE, 0x03);
    // SpO2: ADC range 4096, 100sps, 411us pulse
    write_reg(dev, MAX30102_REG_SPO2, 0x27);
    // LED amplitude ~7mA
    write_reg(dev, MAX30102_REG_LED1, 0x24);
    write_reg(dev, MAX30102_REG_LED2, 0x24);

    // Reset FIFO pointers
    write_reg(dev, MAX30102_REG_FIFO_WR, 0x00);
    write_reg(dev, MAX30102_REG_OVF,     0x00);
    write_reg(dev, MAX30102_REG_FIFO_RD, 0x00);

    ESP_LOGI(TAG, "MAX30102 initialized");
    return ESP_OK;
}

/* Dem so sample co san trong FIFO */
static int fifo_available(max30102_t *dev) {
    uint8_t wr = 0, rd = 0;
    read_reg(dev, MAX30102_REG_FIFO_WR, &wr, 1);
    read_reg(dev, MAX30102_REG_FIFO_RD, &rd, 1);
    wr &= 0x1F;
    rd &= 0x1F;
    if (wr >= rd) return (wr - rd);
    return (32 - rd + wr);
}

esp_err_t max30102_read_fifo(max30102_t *dev, uint32_t *red, uint32_t *ir) {
    /* Chi doc khi co du lieu */
    int avail = fifo_available(dev);
    if (avail <= 0) {
        return ESP_ERR_NOT_FOUND; // FIFO trong, thu lai sau
    }

    uint8_t buf[6];
    esp_err_t ret = read_reg(dev, MAX30102_REG_FIFO, buf, 6);
    if (ret != ESP_OK) return ret;

    *red = ((uint32_t)(buf[0] & 0x03) << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    *ir  = ((uint32_t)(buf[3] & 0x03) << 16) | ((uint32_t)buf[4] << 8) | buf[5];
    return ESP_OK;
}
