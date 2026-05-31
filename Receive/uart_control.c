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
                                UART_PIN_NO_CHANGE,  // TX — không dùng
                              JETSON_UART_RX,       // GPIO 10
                              UART_PIN_NO_CHANGE,
                              UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(JETSON_UART_PORT,
                                         256,   /* rx buf */
                                         0,     /* tx buf (0 = blocking ok) */
                                         0, NULL, 0));

    // Dòng log cũ — in JETSON_UART_TX nhưng không dùng nó nữa
    ESP_LOGI(TAG, "UART%d init: RX=GPIO%d @%d baud",
            JETSON_UART_PORT, JETSON_UART_RX, JETSON_UART_BAUD);
}

/* ═══════════════════════════════════════════════
 *  NỘI BỘ: map byte lệnh → tốc độ (L, R)
 * ═══════════════════════════════════════════════ */
/* ═══════════════════════════════════════════════
 *  NỘI BỘ: map chuỗi lệnh → tốc độ (L, R)
 * ═══════════════════════════════════════════════ */
typedef struct { int left; int right; } lr_speed_t;

static lr_speed_t str_to_speed(const char *cmd)
{
    if      (strcmp(cmd, "Forward") == 0) return (lr_speed_t){ PWM_MAX,      PWM_MAX      };
    else if (strcmp(cmd, "Left")    == 0) return (lr_speed_t){ PWM_TURN_MIN, PWM_MAX      };
    else if (strcmp(cmd, "Right")   == 0) return (lr_speed_t){ PWM_MAX,      PWM_TURN_MIN };
    else                                  return (lr_speed_t){ 0,            0            };
}

/* ═══════════════════════════════════════════════
 *  PUBLIC: task xử lý lệnh UART → motor
 * ═══════════════════════════════════════════════ */
void uart_control_task(void *arg)
{
    char    buf[32];
    int     buf_len = 0;

    for (;;) {
        xEventGroupWaitBits(g_mode_event_group,
                            BIT_MODE_UART,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);

        /* Đọc từng byte, tích lũy vào buf cho đến khi gặp '#' */
        uint8_t byte;
        int len = uart_read_bytes(JETSON_UART_PORT, &byte, 1,
                                  pdMS_TO_TICKS(200));

        if (len < 1) {
            if (mode_manager_get() != MODE_UART) {
                motor_set_lr(0, 0);
                ESP_LOGI(TAG, "Roi MODE_UART, dung motor");
            }
            continue;
        }

        if (mode_manager_get() != MODE_UART) {
            motor_set_lr(0, 0);
            buf_len = 0;   // reset buffer khi đổi mode
            continue;
        }

        if (byte == '#') {
            /* Kết thúc lệnh — xử lý */
            buf[buf_len] = '\0';

            lr_speed_t spd = str_to_speed(buf);
            motor_set_lr(spd.left, spd.right);

            ESP_LOGI(TAG, "UART cmd: \"%s\" | L=%4d R=%4d",
                     buf, spd.left, spd.right);

            buf_len = 0;   // reset cho lệnh tiếp theo
        } else {
            /* Tích lũy ký tự vào buffer, tránh overflow */
            if (buf_len < (int)sizeof(buf) - 1) {
                buf[buf_len++] = (char)byte;
            } else {
                /* Buffer tràn — lệnh rác, reset */
                ESP_LOGW(TAG, "Buffer tran, reset");
                buf_len = 0;
            }
        }
    }
}