// #include "sensor_monitor.h"
// #include "mode_manager.h"
// #include "joystick_motor.h"
// #include "mqtt_control.h"

// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include <stdio.h>

// static const char *TAG = "SENSOR_MON";

// /* ═══════════════════════════════════════════════
//  *  TRẠNG THÁI NỘI BỘ
//  * ═══════════════════════════════════════════════ */
// static volatile bool s_emergency_stop = false;   /* đang dừng khẩn cấp? */

// /* ═══════════════════════════════════════════════
//  *  XỬ LÝ HC-SR04 — dừng khẩn cấp
//  *
//  *  Logic:
//  *   - dist_cm == 0 : chưa có dữ liệu (sensor node chưa gửi) → bỏ qua
//  *   - dist_cm < OBSTACLE_DIST_CM : vật cản → dừng khẩn cấp, set flag
//  *   - dist_cm >= OBSTACLE_DIST_CM && s_emergency_stop : đường thông → nhả
//  * ═══════════════════════════════════════════════ */
// static void handle_hcsr04(uint16_t dist_cm)
// {
//     if (dist_cm == 0) return;   /* chưa có dữ liệu */

//     if (dist_cm < OBSTACLE_DIST_CM) {
//         if (!s_emergency_stop) {
//             s_emergency_stop = true;
//             motor_set_lr(0, 0);   /* dừng ngay lập tức */

//             ESP_LOGW(TAG, "!!! DUNG KHAN CAP: vat can %d cm (nguong %d cm) !!!",
//                      dist_cm, OBSTACLE_DIST_CM);

//             /* Publish cảnh báo lên MQTT */
//             char payload[80];
//             snprintf(payload, sizeof(payload),
//                      "{\"alert\":\"obstacle\",\"dist_cm\":%d,\"threshold\":%d}",
//                      dist_cm, OBSTACLE_DIST_CM);
//             mqtt_publish_alert(MQTT_ALERT_TOPIC, payload);
//         }
//         /* Đang dừng → gọi lại mỗi poll để chắc chắn motor = 0
//          * (đề phòng task khác vô tình set lại)                 */
//         motor_set_lr(0, 0);

//     } else {
//         /* Đường thông → nhả emergency stop */
//         if (s_emergency_stop) {
//             s_emergency_stop = false;
//             ESP_LOGI(TAG, "Duong thong (%d cm), nha emergency stop", dist_cm);

//             char payload[64];
//             snprintf(payload, sizeof(payload),
//                      "{\"alert\":\"obstacle_clear\",\"dist_cm\":%d}",
//                      dist_cm);
//             mqtt_publish_alert(MQTT_ALERT_TOPIC, payload);
//         }
//     }
// }

// /* ═══════════════════════════════════════════════
//  *  XỬ LÝ MAX30102 — cảnh báo nhịp tim cao
//  *
//  *  Sensor node gửi raw IR; nếu IR > HEART_RATE_HIGH_THRESHOLD
//  *  thì ước tính nhịp tim đang cao → cảnh báo + publish MQTT.
//  *
//  *  Throttle: chỉ cảnh báo 1 lần mỗi 5 giây để tránh spam.
//  * ═══════════════════════════════════════════════ */
// #define HEART_ALERT_INTERVAL_MS  5000

// static void handle_max30102(uint32_t red, uint32_t ir)
// {
//     if (ir == 0) return;   /* chưa có dữ liệu */

//     static TickType_t last_alert_tick = 0;

//     if (ir > HEART_RATE_HIGH_THRESHOLD) {
//         TickType_t now = xTaskGetTickCount();

//         if ((now - last_alert_tick) >= pdMS_TO_TICKS(HEART_ALERT_INTERVAL_MS)) {
//             last_alert_tick = now;

//             ESP_LOGW(TAG, "!!! NHIP TIM CAO: IR=%lu RED=%lu (nguong IR=%lu) !!!",
//                      ir, red, (uint32_t)HEART_RATE_HIGH_THRESHOLD);

//             /* Publish cảnh báo lên MQTT */
//             char payload[128];
//             snprintf(payload, sizeof(payload),
//                      "{\"alert\":\"heart_rate_high\",\"ir\":%lu,\"red\":%lu,\"threshold\":%lu}",
//                      ir, red, (uint32_t)HEART_RATE_HIGH_THRESHOLD);
//             mqtt_publish_alert(MQTT_ALERT_TOPIC, payload);
//         }
//     }
// }

// /* ═══════════════════════════════════════════════
//  *  PUBLIC: init (dự phòng mở rộng sau)
//  * ═══════════════════════════════════════════════ */
// void sensor_monitor_init(void)
// {
//     ESP_LOGI(TAG, "Sensor monitor init: HC-SR04 nguong=%d cm, IR nguong=%lu",
//              OBSTACLE_DIST_CM, (uint32_t)HEART_RATE_HIGH_THRESHOLD);
// }

// /* ═══════════════════════════════════════════════
//  *  PUBLIC: task chính
//  * ═══════════════════════════════════════════════ */
// void sensor_monitor_task(void *arg)
// {
//     ESP_LOGI(TAG, "sensor_monitor_task STARTED");

//     for (;;) {
//         sensor_data_t data = mode_manager_get_sensor();

//         /* 1. Kiểm tra vật cản HC-SR04 */
//         handle_hcsr04(data.dist_cm);

//         /* 2. Kiểm tra nhịp tim MAX30102
//          *    (chỉ chạy khi KHÔNG đang emergency stop để tránh nhiễu log) */
//         if (!s_emergency_stop) {
//             handle_max30102(data.red, data.ir);
//         }

//         vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_MS));
//     }
// }
#include "sensor_monitor.h"
#include "mode_manager.h"
#include "joystick_motor.h"
#include "mqtt_control.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "SENSOR_MON";

/* ═══════════════════════════════════════════════
 *  TRẠNG THÁI NỘI BỘ
 * ═══════════════════════════════════════════════ */
static volatile bool s_emergency_stop = false;   /* đang dừng khẩn cấp? */

/* ═══════════════════════════════════════════════
 *  XỬ LÝ HC-SR04 — dừng khẩn cấp
 *
 *  Logic:
 *   - dist_cm == 0 : chưa có dữ liệu (sensor node chưa gửi) → bỏ qua
 *   - dist_cm < OBSTACLE_DIST_CM : vật cản → dừng khẩn cấp, set flag
 *   - dist_cm >= OBSTACLE_DIST_CM && s_emergency_stop : đường thông → nhả
 * ═══════════════════════════════════════════════ */
static void handle_hcsr04(uint16_t dist_cm)
{
    if (dist_cm == 0) return;   /* chưa có dữ liệu */

    if (dist_cm < OBSTACLE_DIST_CM) {
        if (!s_emergency_stop) {
            s_emergency_stop = true;
            motor_set_lr(0, 0);   /* dừng ngay lập tức */

            ESP_LOGW(TAG, "!!! DUNG KHAN CAP: vat can %d cm (nguong %d cm) !!!",
                     dist_cm, OBSTACLE_DIST_CM);

            /* Publish cảnh báo lên MQTT */
            char payload[80];
            snprintf(payload, sizeof(payload),
                     "{\"alert\":\"obstacle\",\"dist_cm\":%d,\"threshold\":%d}",
                     dist_cm, OBSTACLE_DIST_CM);
            mqtt_publish_alert(MQTT_ALERT_TOPIC, payload);
        }
        /* Đang dừng → gọi lại mỗi poll để chắc chắn motor = 0
         * (đề phòng task khác vô tình set lại)                 */
        motor_set_lr(0, 0);

    } else {
        /* Đường thông → nhả emergency stop */
        if (s_emergency_stop) {
            s_emergency_stop = false;
            ESP_LOGI(TAG, "Duong thong (%d cm), nha emergency stop", dist_cm);

            char payload[64];
            snprintf(payload, sizeof(payload),
                     "{\"alert\":\"obstacle_clear\",\"dist_cm\":%d}",
                     dist_cm);
            mqtt_publish_alert(MQTT_ALERT_TOPIC, payload);
        }
    }
}

/* ═══════════════════════════════════════════════
 *  XỬ LÝ MAX30102 — cảnh báo nhịp tim cao
 *
 *  Sensor node gửi raw IR; nếu IR > HEART_RATE_HIGH_THRESHOLD
 *  thì ước tính nhịp tim đang cao → cảnh báo + publish MQTT.
 *
 *  Throttle: chỉ cảnh báo 1 lần mỗi 5 giây để tránh spam.
 * ═══════════════════════════════════════════════ */
#define HEART_ALERT_INTERVAL_MS  5000

static void handle_max30102(uint32_t red, uint32_t ir)
{
    if (ir == 0) return;   /* chưa có dữ liệu */

    static TickType_t last_alert_tick = 0;

    if (ir > HEART_RATE_HIGH_THRESHOLD) {
        TickType_t now = xTaskGetTickCount();

        if ((now - last_alert_tick) >= pdMS_TO_TICKS(HEART_ALERT_INTERVAL_MS)) {
            last_alert_tick = now;

            ESP_LOGW(TAG, "!!! NHIP TIM CAO: IR=%lu RED=%lu (nguong IR=%lu) !!!",
                     ir, red, (uint32_t)HEART_RATE_HIGH_THRESHOLD);

            /* Publish cảnh báo lên MQTT */
            char payload[128];
            snprintf(payload, sizeof(payload),
                     "{\"alert\":\"heart_rate_high\",\"ir\":%lu,\"red\":%lu,\"threshold\":%lu}",
                     ir, red, (uint32_t)HEART_RATE_HIGH_THRESHOLD);
            mqtt_publish_alert(MQTT_ALERT_TOPIC, payload);
        }
    }
}

/* ═══════════════════════════════════════════════
 *  PUBLIC: init (dự phòng mở rộng sau)
 * ═══════════════════════════════════════════════ */
void sensor_monitor_init(void)
{
    ESP_LOGI(TAG, "Sensor monitor init: HC-SR04 nguong=%d cm, IR nguong=%lu",
             OBSTACLE_DIST_CM, (uint32_t)HEART_RATE_HIGH_THRESHOLD);
}

/* ═══════════════════════════════════════════════
 *  PUBLISH ĐỊNH KỲ — gửi sensor data lên vehicle/sensor mỗi 1s
 *  Web dashboard dùng để hiển thị nhịp tim + khoảng cách thời gian thực
 * ═══════════════════════════════════════════════ */
#define SENSOR_PUBLISH_INTERVAL_MS  1000

static void publish_sensor_data(uint16_t dist_cm, uint32_t ir)
{
    /* Ước tính BPM đơn giản từ IR raw:
     * IR > 100000 → ~100 BPM, scale tuyến tính 50k-150k → 60-120 BPM */
    int bpm = 0;
    if (ir > 0) {
        if (ir >= 150000) bpm = 120;
        else if (ir <= 50000) bpm = 60;
        else bpm = 60 + (int)((ir - 50000) * 60 / 100000);
    }

    /* Battery: dùng giá trị mock 87% (thay bằng ADC thực nếu có) */
    int battery = 87;

    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"heart_rate\":%d,\"distance\":%d,\"battery\":%d}",
             bpm, dist_cm, battery);

    mqtt_publish_alert(MQTT_ALERT_TOPIC, payload);
    ESP_LOGD(TAG, "SENSOR published: %s", payload);
}

/* ═══════════════════════════════════════════════
 *  PUBLIC: task chính
 * ═══════════════════════════════════════════════ */
void sensor_monitor_task(void *arg)
{
    ESP_LOGI(TAG, "sensor_monitor_task STARTED");

    TickType_t last_publish = 0;

    for (;;) {
        sensor_data_t data = mode_manager_get_sensor();

        /* 1. Kiểm tra vật cản HC-SR04 */
        handle_hcsr04(data.dist_cm);

        /* 2. Kiểm tra nhịp tim MAX30102
         *    (chỉ chạy khi KHÔNG đang emergency stop để tránh nhiễu log) */
        if (!s_emergency_stop) {
            handle_max30102(data.red, data.ir);
        }

        /* 3. Publish định kỳ lên vehicle/sensor để web hiển thị */
        TickType_t now = xTaskGetTickCount();
        if ((now - last_publish) >= pdMS_TO_TICKS(SENSOR_PUBLISH_INTERVAL_MS)) {
            last_publish = now;
            if (data.dist_cm > 0 || data.ir > 0) {
                publish_sensor_data(data.dist_cm, data.ir);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_MS));
    }
}