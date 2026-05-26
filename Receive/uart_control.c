#include "uart_control.h"
#include "mode_manager.h"
#include "joystick_motor.h"

#include "esp_log.h"
#include <string.h>

static const char *TAG = "UART_CTRL";

/* ═══════════════════════════════════════════════
 *  PUBLIC: khởi tạo UART
 * ═══════════════════════════════════════════════ */
void uart_control_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate  = JETSON_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(JETSON_UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(JETSON_UART_PORT,
                                  JETSON_UART_TX,
                                  JETSON_UART_RX,
                                  UART_PIN_NO_CHANGE,
                                  UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(JETSON_UART_PORT,
                                         256,   /* rx buf */
                                         0,     /* tx buf (0 = blocking ok) */
                                         0, NULL, 0));

    ESP_LOGI(TAG, "UART%d init: TX=GPIO%d RX=GPIO%d @%d baud",
             JETSON_UART_PORT, JETSON_UART_TX, JETSON_UART_RX, JETSON_UART_BAUD);
}

/* ═══════════════════════════════════════════════
 *  NỘI BỘ: map byte lệnh → tốc độ (L, R)
 * ═══════════════════════════════════════════════ */
typedef struct { int left; int right; } lr_speed_t;

static lr_speed_t byte_to_speed(uint8_t b)
{
    switch (b) {
        case 'F': return (lr_speed_t){ PWM_MAX,  PWM_MAX};   /* tiến  */
        case 'B': return (lr_speed_t){-PWM_MAX, -PWM_MAX};   /* lùi   */
        case 'L': return (lr_speed_t){PWM_TURN_MIN,  PWM_MAX};   /* trái  */
        case 'R': return (lr_speed_t){ PWM_MAX, PWM_TURN_MIN};   /* phải  */
        default:  return (lr_speed_t){0, 0};                  /* stop  */
    }
}

/* ═══════════════════════════════════════════════
 *  PUBLIC: task xử lý lệnh UART → motor
 * ═══════════════════════════════════════════════ */
void uart_control_task(void *arg)
{
    uint8_t byte;

    for (;;) {
        /* Chờ được kích hoạt MODE_UART */
        xEventGroupWaitBits(g_mode_event_group,
                            BIT_MODE_UART,
                            pdFALSE,       /* không clear bit */
                            pdTRUE,
                            portMAX_DELAY);

        /* Đọc 1 byte, timeout 200 ms */
        int len = uart_read_bytes(JETSON_UART_PORT, &byte, 1,
                                  pdMS_TO_TICKS(200));

        if (len < 1) {
            /* timeout — kiểm tra mode có còn UART không */
            if (mode_manager_get() != MODE_UART) {
                motor_set_lr(0, 0);
                ESP_LOGI(TAG, "Roi MODE_UART, dung motor");
            }
            continue;
        }

        /* Đọc được byte — kiểm tra mode trước khi chạy */
        if (mode_manager_get() != MODE_UART) {
            motor_set_lr(0, 0);
            continue;
        }

        lr_speed_t spd = byte_to_speed(byte);
        motor_set_lr(spd.left, spd.right);

        ESP_LOGI(TAG, "UART cmd: '%c' (0x%02X) | L=%4d R=%4d",
                 (byte >= 0x20 ? byte : '?'), byte,
                 spd.left, spd.right);
    }
}