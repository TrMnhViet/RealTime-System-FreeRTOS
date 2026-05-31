// #include "mode_manager.h"
// #include "esp_log.h"
// #include <string.h>

// static const char *TAG = "MODE_MGR";

// /* ═══════════════════════════════════════════════
//  *  STATE
//  * ═══════════════════════════════════════════════ */
// static volatile robot_mode_t s_current_mode = MODE_JOYSTICK;
// // static volatile robot_mode_t s_current_mode = MODE_MQTT;
// EventGroupHandle_t g_mode_event_group = NULL;

// /* ═══════════════════════════════════════════════
//  *  NỘI BỘ: áp dụng mode mới
//  *  — xoá bit mode cũ, set bit mode mới
//  *  — log thay đổi
//  * ═══════════════════════════════════════════════ */
// static void apply_mode(robot_mode_t new_mode)
// {
//     if (new_mode == s_current_mode) return;   // không đổi → bỏ qua

//     static const char *mode_name[] = {
//         [MODE_JOYSTICK] = "JOYSTICK",
//         [MODE_MQTT]     = "MQTT",
//         [MODE_UART]     = "UART",
//     };

//     ESP_LOGI(TAG, "Chuyen mode: %s → %s",
//              mode_name[s_current_mode],
//              mode_name[new_mode]);

//     s_current_mode = new_mode;

//     /* Xoá tất cả bit cũ, set bit tương ứng mode mới */
//     xEventGroupClearBits(g_mode_event_group,
//                          BIT_MODE_JOYSTICK | BIT_MODE_MQTT | BIT_MODE_UART);

//     EventBits_t bit = 0;
//     switch (new_mode) {
//         case MODE_JOYSTICK: bit = BIT_MODE_JOYSTICK; break;
//         case MODE_MQTT:     bit = BIT_MODE_MQTT;     break;
//         case MODE_UART:     bit = BIT_MODE_UART;     break;
//     }
//     xEventGroupSetBits(g_mode_event_group, bit);
// }

// /* ═══════════════════════════════════════════════
//  *  TASK: lắng nghe CAN frame chuyển mode
//  * ═══════════════════════════════════════════════ */
// static void can_mode_listener_task(void *arg)
// {
//     twai_message_t msg;

//     for (;;) {
//         /* Block vô hạn chờ frame CAN */
//         esp_err_t ret = twai_receive(&msg, portMAX_DELAY);
//         if (ret != ESP_OK) {
//             ESP_LOGW(TAG, "twai_receive err: %s", esp_err_to_name(ret));
//             continue;
//         }

//         /* Chỉ xử lý frame có ID đúng và DLC >= 1 */
//         if (msg.identifier != CAN_MODE_MSG_ID || msg.data_length_code < 1) {
//             continue;
//         }

//         uint8_t cmd = msg.data[0];
//         if (cmd >= MODE_JOYSTICK && cmd <= MODE_UART) {
//             apply_mode((robot_mode_t)cmd);
//         } else {
//             ESP_LOGW(TAG, "CAN: lenh mode khong hop le 0x%02X", cmd);
//         }
//     }
// }

// /* ═══════════════════════════════════════════════
//  *  PUBLIC: khởi tạo
//  * ═══════════════════════════════════════════════ */
// void mode_manager_init(void)
// {
//     /* Tạo event group */
//     g_mode_event_group = xEventGroupCreate();
//     configASSERT(g_mode_event_group);

//     /* Set bit mode mặc định (Joystick) */
//     xEventGroupSetBits(g_mode_event_group, BIT_MODE_JOYSTICK);
//         // xEventGroupSetBits(g_mode_event_group, BIT_MODE_MQTT);

//     /* Cấu hình TWAI (CAN) driver */
//     twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
//         CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);

//     twai_timing_config_t  t_config = CAN_BAUDRATE;

//     /* Chấp nhận tất cả frame; lọc theo ID trong task */
//     twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

//     ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
//     ESP_ERROR_CHECK(twai_start());

//     ESP_LOGI(TAG, "TWAI (CAN) started: TX=GPIO%d RX=GPIO%d", CAN_TX_GPIO, CAN_RX_GPIO);

//     /* Khởi tạo task lắng nghe CAN */
//     xTaskCreate(can_mode_listener_task,
//                 "can_mode",
//                 2048,
//                 NULL,
//                 10,        /* Priority cao hơn task motor để xử lý ngay */
//                 NULL);
// }

// /* ═══════════════════════════════════════════════
//  *  PUBLIC: lấy mode hiện tại
//  * ═══════════════════════════════════════════════ */
// robot_mode_t mode_manager_get(void)
// {
//     return s_current_mode;
// }
#include "mode_manager.h"
#include "esp_log.h"

static const char *TAG = "MODE_MGR";

/* ═══════════════════════════════════════════════
 *  STATE
 * ═══════════════════════════════════════════════ */
// static volatile robot_mode_t  s_current_mode = MODE_JOYSTICK;
static volatile robot_mode_t  s_current_mode = MODE_UART;

static volatile sensor_data_t s_sensor       = {0};
EventGroupHandle_t             g_mode_event_group = NULL;

/* ═══════════════════════════════════════════════
 *  NỘI BỘ
 * ═══════════════════════════════════════════════ */
static void apply_mode(robot_mode_t new_mode)
{
    if (new_mode == s_current_mode) return;

    static const char *mode_name[] = {
        [MODE_JOYSTICK] = "JOYSTICK",
        [MODE_MQTT]     = "MQTT",
        [MODE_UART]     = "UART",
    };

    ESP_LOGI(TAG, "Chuyen mode: %s -> %s",
             mode_name[s_current_mode],
             mode_name[new_mode]);

    s_current_mode = new_mode;

    xEventGroupClearBits(g_mode_event_group,
                         BIT_MODE_JOYSTICK | BIT_MODE_MQTT | BIT_MODE_UART);

    EventBits_t bit = 0;
    switch (new_mode) {
        case MODE_JOYSTICK: bit = BIT_MODE_JOYSTICK; break;
        case MODE_MQTT:     bit = BIT_MODE_MQTT;     break;
        case MODE_UART:     bit = BIT_MODE_UART;     break;
    }
    xEventGroupSetBits(g_mode_event_group, bit);
}

static void can_listener_task(void *arg)
{
    ESP_LOGI(TAG, "can_listener_task STARTED");
    twai_message_t msg;

    for (;;) {
        /* đổi portMAX_DELAY → 3000ms, giống receiver test */
        esp_err_t ret = twai_receive(&msg, pdMS_TO_TICKS(3000));

        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "3s khong nhan duoc frame nao");
            continue;
        }
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "twai_receive err: %s", esp_err_to_name(ret));
            continue;
        }

        ESP_LOGI(TAG, "Frame: ID=0x%03lX DLC=%d",
                 msg.identifier, msg.data_length_code);


        switch (msg.identifier) {

            case CAN_MODE_MSG_ID:
                if (msg.data_length_code < 1) break;
                {
                    uint8_t cmd = msg.data[0];
                    if (cmd >= MODE_JOYSTICK && cmd <= MODE_UART) {
                        apply_mode((robot_mode_t)cmd);
                    } else {
                        ESP_LOGW(TAG, "CAN: mode khong hop le 0x%02X", cmd);
                    }
                }
                break;

            case CAN_ID_MAX30102:
                if (msg.data_length_code < 6) break;
                s_sensor.red = ((uint32_t)msg.data[0] << 16)
                             | ((uint32_t)msg.data[1] <<  8)
                             |  (uint32_t)msg.data[2];
                s_sensor.ir  = ((uint32_t)msg.data[3] << 16)
                             | ((uint32_t)msg.data[4] <<  8)
                             |  (uint32_t)msg.data[5];
                ESP_LOGI(TAG, "[CAN] MAX30102 Red=%lu IR=%lu",
                         s_sensor.red, s_sensor.ir);
                break;

            case CAN_ID_HCSR04:
                if (msg.data_length_code < 2) break;
                s_sensor.dist_cm = ((uint16_t)msg.data[0] << 8)
                                 |  (uint16_t)msg.data[1];
                ESP_LOGI(TAG, "[CAN] HC-SR04 %d cm", s_sensor.dist_cm);
                break;

            default:
                ESP_LOGD(TAG, "CAN: frame la ID=0x%03lX (bo qua)",
                         msg.identifier);
                break;
        }
    }
}

/* ═══════════════════════════════════════════════
 *  PUBLIC
 * ═══════════════════════════════════════════════ */
void mode_manager_init(void)
{
    g_mode_event_group = xEventGroupCreate();
    configASSERT(g_mode_event_group);

    // xEventGroupSetBits(g_mode_event_group, BIT_MODE_JOYSTICK);
    xEventGroupSetBits(g_mode_event_group, BIT_MODE_UART);

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_NORMAL);
    twai_timing_config_t  t_config = CAN_BAUDRATE;
    twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(TAG, "TWAI started TX=GPIO%d RX=GPIO%d", CAN_TX_GPIO, CAN_RX_GPIO);

    xTaskCreate(can_listener_task, "can_listener", 4096, NULL, 10, NULL);
}

robot_mode_t mode_manager_get(void)
{
    return s_current_mode;
}

sensor_data_t mode_manager_get_sensor(void)
{
    return s_sensor;
}   