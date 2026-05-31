#pragma once

#include "driver/uart.h"
#include "driver/gpio.h"

/* ═══════════════════════════════════════════════
 *  UART CONFIG — nhận lệnh từ Jetson Nano
 *  Điền đúng UART port và GPIO bạn dùng
 * ═══════════════════════════════════════════════ */
#define JETSON_UART_PORT    UART_NUM_1       /* đổi sang UART_NUM_1 nếu cần */
// #define JETSON_UART_TX      GPIO_NUM_21      /* TX ESP32 → RX Jetson (ít dùng) */
#define JETSON_UART_RX      GPIO_NUM_8      /* RX ESP32 ← TX Jetson           */
#define JETSON_UART_BAUD    9600

/* ═══════════════════════════════════════════════
 *  PROTOCOL (1 byte/lệnh, đơn giản nhất)
 *   0x46 'F'  forward
 *   0x53 'S'  stop
 *   0x4C 'L'  left
 *   0x52 'R'  right
 *   0x42 'B'  backward
 *
 *  Jetson gửi 1 byte; ESP32 thực hiện ngay.
 *  Nếu muốn thêm framing (header+checksum) hãy mở rộng tại đây.
 * ═══════════════════════════════════════════════ */

/**
 * @brief Khởi tạo UART driver. Gọi một lần trong app_main().
 */
void uart_control_init(void);

/**
 * @brief Task chờ mode UART được kích hoạt rồi xử lý lệnh byte.
 *        Truyền làm tham số cho xTaskCreate().
 */
void uart_control_task(void *arg);
   