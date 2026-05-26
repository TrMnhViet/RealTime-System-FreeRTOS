// #pragma once
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/event_groups.h"
// #include "driver/gpio.h"
// #include "driver/twai.h"

// /* ═══════════════════════════════════════════════
//  *  CAN / TWAI CONFIG
//  *  GPIO4 = TX,  GPIO5 = RX
//  *  Tốc độ bus: 500 kbit/s (đổi nếu cần)
//  * ═══════════════════════════════════════════════ */
// #define CAN_TX_GPIO     GPIO_NUM_4
// #define CAN_RX_GPIO     GPIO_NUM_5
// #define CAN_BAUDRATE    TWAI_TIMING_CONFIG_500KBITS()

// /* CAN message ID dùng để chuyển mode
//  * Node gửi (bảng nút bấm) gửi frame ID=0x100, DLC=1
//  *   data[0] = 0x01  → MODE_JOYSTICK
//  *   data[0] = 0x02  → MODE_MQTT
//  *   data[0] = 0x03  → MODE_UART
//  */
// #define CAN_MODE_MSG_ID     0x100UL

// /* ═══════════════════════════════════════════════
//  *  MODE DEFINITIONS
//  * ═══════════════════════════════════════════════ */
// typedef enum {
//     MODE_JOYSTICK = 1,   /* Nút 1 — điều khiển tay bằng joystick    */
//     MODE_MQTT     = 2,   /* Nút 2 — nhận lệnh từ web qua MQTT/WiFi  */
//     MODE_UART     = 3,   /* Nút 3 — nhận lệnh UART từ Jetson Nano   */
// } robot_mode_t;

// /* ═══════════════════════════════════════════════
//  *  EVENT GROUP BITS — dùng để đánh thức task
//  * ═══════════════════════════════════════════════ */
// #define BIT_MODE_JOYSTICK   (1 << 0)
// #define BIT_MODE_MQTT       (1 << 1)
// #define BIT_MODE_UART       (1 << 2)

// /* ═══════════════════════════════════════════════
//  *  PUBLIC API
//  * ═══════════════════════════════════════════════ */

// /**
//  * @brief Khởi tạo TWAI driver và task lắng nghe CAN.
//  *        Gọi một lần trong app_main().
//  */
// void mode_manager_init(void);

// /**
//  * @brief Trả về mode hiện tại (thread-safe).
//  */
// robot_mode_t mode_manager_get(void);

// /**
//  * @brief Event group để các task mode block chờ kích hoạt.
//  *        Extern để mqtt_control.c / uart_control.c có thể dùng.
//  */
// extern EventGroupHandle_t g_mode_event_group;
#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/twai.h"

/* ═══════════════════════════════════════════════
 *  CAN / TWAI CONFIG
 * ═══════════════════════════════════════════════ */
#define CAN_TX_GPIO         GPIO_NUM_4
#define CAN_RX_GPIO         GPIO_NUM_5
#define CAN_BAUDRATE        TWAI_TIMING_CONFIG_500KBITS()

/* CAN Frame IDs — khớp với Sender */
#define CAN_ID_MAX30102     0x001UL
#define CAN_ID_HCSR04       0x002UL
#define CAN_MODE_MSG_ID     0x003UL

/* ═══════════════════════════════════════════════
 *  MODE DEFINITIONS
 *  Giá trị khớp byte Sender gửi: 0 / 1 / 2
 * ═══════════════════════════════════════════════ */
typedef enum {
    MODE_JOYSTICK = 0,   /* Sender: MODE_JOYSTICK = 0 */
    MODE_MQTT     = 1,   /* Sender: MODE_WEB      = 1 */
    MODE_UART     = 2,   /* Sender: MODE_EYE      = 2 */
} robot_mode_t;

/* ═══════════════════════════════════════════════
 *  EVENT GROUP BITS
 * ═══════════════════════════════════════════════ */
#define BIT_MODE_JOYSTICK   (1 << 0)
#define BIT_MODE_MQTT       (1 << 1)
#define BIT_MODE_UART       (1 << 2)

/* ═══════════════════════════════════════════════
 *  SENSOR DATA
 * ═══════════════════════════════════════════════ */
typedef struct {
    uint32_t red;
    uint32_t ir;
    uint16_t dist_cm;
} sensor_data_t;

/* ═══════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════ */
void          mode_manager_init(void);
robot_mode_t  mode_manager_get(void);
sensor_data_t mode_manager_get_sensor(void);

extern EventGroupHandle_t g_mode_event_group;