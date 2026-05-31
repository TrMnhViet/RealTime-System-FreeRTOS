#pragma once

/* ═══════════════════════════════════════════════
 *  SENSOR MONITOR — chạy độc lập, không phụ thuộc mode
 *
 *  Hai chức năng:
 *   1. HC-SR04 : khoảng cách < OBSTACLE_DIST_CM → dừng khẩn cấp motor,
 *                giữ cho đến khi đường thông rồi mới nhả.
 *   2. MAX30102 : nhịp tim ước tính > HEART_RATE_HIGH_BPM → cảnh báo log
 *                 + publish JSON lên MQTT topic "vehicle/alert".
 *
 *  Ngưỡng có thể điều chỉnh ở đây.
 * ═══════════════════════════════════════════════ */

/* ── Ngưỡng HC-SR04 (cm) ── */
#define OBSTACLE_DIST_CM    70      /* < 30 cm → dừng khẩn cấp */

/* ── Ngưỡng nhịp tim (BPM ước tính từ IR ratio) ──
 *  MAX30102 gửi raw IR; threshold dưới đây là raw IR tương đương ~100 BPM.
 *  Điều chỉnh theo thuật toán tính BPM thực tế của sensor node.        */
#define HEART_RATE_HIGH_THRESHOLD   100000UL   /* raw IR > giá trị này → cao */

/* ── Topic MQTT để gửi cảnh báo ── */
#define MQTT_ALERT_TOPIC    "vehicle/sensor"

/* ── Chu kỳ poll sensor (ms) ── */
#define SENSOR_POLL_MS      100

/**
 * @brief Khởi tạo sensor monitor (không làm gì thêm hiện tại,
 *        để dành cho future expansion).
 *        Gọi trước xTaskCreate(sensor_monitor_task, ...).
 */
void sensor_monitor_init(void);

/**
 * @brief Task giám sát cảm biến — tạo bằng xTaskCreate() trong app_main().
 *        Stack khuyến nghị: 4096 bytes, priority: 8 (cao hơn task motor).
 */
void sensor_monitor_task(void *arg);