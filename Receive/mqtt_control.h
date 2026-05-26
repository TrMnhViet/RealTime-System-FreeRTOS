#pragma once

/* ═══════════════════════════════════════════════
 *  MQTT CONTROL — Mode 2
 *
 *  Khớp với Flutter app (vehicle_service.dart):
 *    Broker  : broker.hivemq.com:1883 (TCP)
 *              App dùng WSS:8884, ESP32 dùng TCP:1883
 *              cùng broker HiveMQ public → nhận được nhau.
 *
 *    Topic nhận lệnh : "vehicle/command"
 *
 *    Payload JSON (Flutter gửi):
 *      {"cmd":"MOVE","dir":"F","ts":...}
 *      {"cmd":"MOVE","dir":"B","ts":...}
 *      {"cmd":"MOVE","dir":"L","ts":...}
 *      {"cmd":"MOVE","dir":"R","ts":...}
 *      {"cmd":"STOP","dir":"S","ts":...}
 *      {"cmd":"SPEED","spd":80,"ts":...}  ← bỏ qua, không điều khiển hướng
 *
 *    Mapping dir → motor:
 *      "F"  tiến   L=+MAX  R=+MAX
 *      "B"  lùi    L=-MAX  R=-MAX
 *      "L"  trái   L=-MAX  R=+MAX
 *      "R"  phải   L=+MAX  R=-MAX
 *      "S"  dừng   L=0     R=0
 * ═══════════════════════════════════════════════ */

    /* Broker công cộng HiveMQ — TCP port 1883 */
    #define MQTT_BROKER_URI  "mqtt://broker.emqx.io:1883"
    #define MQTT_CMD_TOPIC      "vehicle/command"

    /* Client ID unique tránh đá nhau với Flutter app */
    #define MQTT_CLIENT_ID      "esp32_robot_ctrl"

    void mqtt_control_init(void);
    void mqtt_control_task(void *arg);
    /**
     * @brief Publish một JSON string lên MQTT topic bất kỳ.
     *        Dùng cho sensor_monitor để gửi cảnh báo obstacle / heart rate.
     *        Thread-safe (esp_mqtt_client_publish là re-entrant).
     *        Không block; QoS=0, retain=0.
     *
     * @param topic   Topic đích, ví dụ MQTT_ALERT_TOPIC "vehicle/alert"
     * @param payload JSON string kết thúc bằng '\0'
     */
    void mqtt_publish_alert(const char *topic, const char *payload);