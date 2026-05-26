#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "max30102.h"
#include "hcsr04.h"

#define TAG "SENDER"

/* ─── GPIO ─── */
#define SDA_PIN     GPIO_NUM_6
#define SCL_PIN     GPIO_NUM_7
#define CAN_TX      GPIO_NUM_4
#define CAN_RX      GPIO_NUM_5
#define TRIG_PIN    GPIO_NUM_2
#define ECHO_PIN    GPIO_NUM_3
#define BTN_PIN     GPIO_NUM_10  // nut chuyen che do

/* ─── CAN Frame ID ─── */
#define CAN_ID_MAX30102  0x001
#define CAN_ID_HCSR04    0x002
#define CAN_ID_MODE      0x003  // 1 byte: 0=JOYSTICK 1=WEB 2=EYE

/* ─── Che do dieu khien ─── */
typedef enum {
    MODE_JOYSTICK = 0,
    MODE_WEB      = 1,
    MODE_EYE      = 2,
    MODE_COUNT    = 3,
} ctrl_mode_t;

static const char *MODE_NAMES[] = { "JOYSTICK", "WEB", "EYE UART" };
static volatile ctrl_mode_t s_mode = MODE_JOYSTICK;

/* ─── Gui CAN ─── */
static void can_send(uint32_t id, uint8_t *data, uint8_t len)
{
    twai_message_t msg = {
        .identifier       = id,
        .data_length_code = len,
    };
    memcpy(msg.data, data, len);

    esp_err_t ret = twai_transmit(&msg, pdMS_TO_TICKS(1000));
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "[CAN TX] ID=0x%03lX OK", id);
    } else {
        ESP_LOGE(TAG, "[CAN TX] ID=0x%03lX loi: %s", id, esp_err_to_name(ret));
        twai_status_info_t status;
        twai_get_status_info(&status);
        if (status.state == TWAI_STATE_BUS_OFF) {
            twai_initiate_recovery();
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

static void send_mode(ctrl_mode_t mode)
{
    uint8_t data = (uint8_t)mode;
    can_send(CAN_ID_MODE, &data, 1);
    ESP_LOGI(TAG, ">>> Chuyen sang che do: %s", MODE_NAMES[mode]);
}

/* ─── Task nut bam ─── */
static void task_button(void *arg)
{
    bool last_state = true; // pull-up nen mac dinh HIGH

    // Gui mode mac dinh luc khoi dong
    vTaskDelay(pdMS_TO_TICKS(1000));
    send_mode(s_mode);

    while (1) {
        bool cur = gpio_get_level(BTN_PIN);

        // Canh xuong (nhan nut)
        if (!cur && last_state) {
            vTaskDelay(pdMS_TO_TICKS(50)); // debounce
            if (!gpio_get_level(BTN_PIN)) {
                s_mode = (ctrl_mode_t)((s_mode + 1) % MODE_COUNT);
                send_mode(s_mode);
            }
        }
        last_state = cur;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* ─── Task cam bien ─── */
static void task_sensor(void *arg)
{
    max30102_t max_dev;
    hcsr04_t   hc_dev;

    // I2C
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = SDA_PIN,
        .scl_io_num        = SCL_PIN,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));
    vTaskDelay(pdMS_TO_TICKS(300));

    bool max_ok = false;
    if (i2c_master_probe(i2c_bus, 0x57, pdMS_TO_TICKS(200)) == ESP_OK) {
        max_ok = (max30102_init(&max_dev, i2c_bus) == ESP_OK);
    }
    ESP_LOGI(TAG, "MAX30102: %s", max_ok ? "OK" : "Khong tim thay");

    hcsr04_init(&hc_dev, TRIG_PIN, ECHO_PIN);
    ESP_LOGI(TAG, "HC-SR04 san sang");

    while (1) {
        /* MAX30102 */
        if (max_ok) {
            uint32_t red = 0, ir = 0;
            esp_err_t ret = max30102_read_fifo(&max_dev, &red, &ir);
            if (ret == ESP_OK) {
                uint8_t data[6] = {
                    (red >> 16) & 0xFF, (red >> 8) & 0xFF, red & 0xFF,
                    (ir  >> 16) & 0xFF, (ir  >> 8) & 0xFF, ir  & 0xFF,
                };
                ESP_LOGI(TAG, "[MAX30102] Red=%lu | IR=%lu", red, ir);
                can_send(CAN_ID_MAX30102, data, 6);
            }
        }

        /* HC-SR04 */
        uint16_t dist_cm = 0;
        if (hcsr04_read_distance(&hc_dev, &dist_cm) == ESP_OK) {
            uint8_t data[2] = { (dist_cm >> 8) & 0xFF, dist_cm & 0xFF };
            ESP_LOGI(TAG, "[HC-SR04] Distance=%d cm", dist_cm);
            can_send(CAN_ID_HCSR04, data, 2);
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Sender khoi dong ===");

    /* Nut bam GPIO10 */
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BTN_PIN),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);
    ESP_LOGI(TAG, "Nut MODE: GPIO%d", BTN_PIN);

    /* CAN */
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NORMAL);
    twai_timing_config_t  t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_ERROR_CHECK(twai_driver_install(&g, &t, &f));
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "CAN Bus san sang!");

    /* Tasks */
    xTaskCreate(task_button, "BUTTON", 2048, NULL, 4, NULL);
    xTaskCreate(task_sensor, "SENSOR", 4096, NULL, 3, NULL);
}
