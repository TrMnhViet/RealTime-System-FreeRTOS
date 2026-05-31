// #include "mqtt_control.h"
// #include "mode_manager.h"
// #include "joystick_motor.h"

// #include "esp_log.h"
// #include "esp_wifi.h"
// #include "esp_event.h"
// #include "esp_netif.h"
// #include "nvs_flash.h"
// #include "mqtt_client.h"
// #include "freertos/queue.h"
// #include "freertos/event_groups.h"
// #include <string.h>

// /* Bit báo hiệu WiFi đã có IP */
// #define WIFI_CONNECTED_BIT  BIT0
// static EventGroupHandle_t s_wifi_event_group;

// /* ═══════════════════════════════════════════════
//  *  WiFi credentials — điền vào đây
//  * ═══════════════════════════════════════════════ */
// #define WIFI_SSID   "IMC Lab"
// #define WIFI_PASS   "IMC@2026"

// static const char *TAG = "MQTT_CTRL";

// /* ═══════════════════════════════════════════════
//  *  Queue: chỉ cần truyền 1 ký tự dir ('F','B','L','R','S')
//  *  sau khi đã parse JSON trong callback
//  * ═══════════════════════════════════════════════ */
// #define CMD_QUEUE_LEN   4

// static QueueHandle_t s_cmd_queue = NULL;   /* queue 1 byte dir */

// /* ═══════════════════════════════════════════════
//  *  PARSE JSON TỐI GIẢN — không dùng cJSON
//  *
//  *  Flutter gửi payload dạng:
//  *    {"cmd":"MOVE","dir":"F","ts":1234}
//  *    {"cmd":"STOP","dir":"S","ts":1234}
//  *    {"cmd":"SPEED","spd":80,"ts":1234}
//  *
//  *  Chỉ cần lấy:
//  *    - "cmd" : nếu là "SPEED" → bỏ qua
//  *    - "dir" : 1 ký tự F/B/L/R/S
//  * ═══════════════════════════════════════════════ */
// static char parse_dir(const char *payload, int len)
// {
//     /* Tìm "dir":"X" trong chuỗi payload */
//     const char *key = "\"dir\":\"";
//     int key_len = (int)strlen(key);

//     for (int i = 0; i < len - key_len - 1; i++) {
//         if (strncmp(payload + i, key, key_len) == 0) {
//             return payload[i + key_len];   /* ký tự ngay sau dấu " */
//         }
//     }
//     return 0;   /* không tìm thấy */
// }

// /* ═══════════════════════════════════════════════
//  *  MAP dir → tốc độ (L, R)
//  * ═══════════════════════════════════════════════ */
// typedef struct { int left; int right; } lr_speed_t;

// static lr_speed_t dir_to_speed(char dir)
// {
//     switch (dir) {
//         case 'F': return (lr_speed_t){ PWM_MAX,  PWM_MAX};
//         case 'B': return (lr_speed_t){-PWM_MAX, -PWM_MAX};
//         case 'L': return (lr_speed_t){PWM_TURN_MIN,  PWM_MAX};
//         case 'R': return (lr_speed_t){ PWM_MAX, PWM_TURN_MIN};
//         default:  return (lr_speed_t){0, 0};   /* 'S' hoặc unknown → dừng */
//     }
// }

// /* ═══════════════════════════════════════════════
//  *  MQTT EVENT HANDLER
//  *  Parse JSON ngay trong callback, đẩy 1 byte dir vào queue
//  * ═══════════════════════════════════════════════ */
// static void mqtt_event_handler(void *arg, esp_event_base_t base,
//                                 int32_t event_id, void *event_data)
// {
//     esp_mqtt_event_handle_t event = event_data;

//     if (event_id == MQTT_EVENT_DATA) {
//         /* Kiểm tra đúng topic */
//         if (event->topic_len > 0 &&
//             strncmp(event->topic, MQTT_CMD_TOPIC, event->topic_len) != 0) {
//             return;
//         }

//         /* Bỏ qua SPEED command (không có "dir") */
//         if (strnstr(event->data, "\"SPEED\"", event->data_len) != NULL) {
//             ESP_LOGD(TAG, "SPEED cmd: bo qua (khong dieu khien huong)");
//             return;
//         }

//         char dir = parse_dir(event->data, event->data_len);
//         if (dir == 0) {
//             ESP_LOGW(TAG, "Khong parse duoc dir tu payload");
//             return;
//         }

//         /* Gửi 1 byte vào queue; không block */
//         xQueueSend(s_cmd_queue, &dir, 0);

//     } else if (event_id == MQTT_EVENT_CONNECTED) {
//         ESP_LOGI(TAG, "MQTT connected → subscribe: %s", MQTT_CMD_TOPIC);
//         esp_mqtt_client_subscribe(event->client, MQTT_CMD_TOPIC, 0);

//     } else if (event_id == MQTT_EVENT_DISCONNECTED) {
//         ESP_LOGW(TAG, "MQTT disconnected");
//     }
// }


// /* ═══════════════════════════════════════════════
//  *  WIFI EVENT HANDLER
//  *  Set bit khi có IP → wifi_init() thoát block
//  * ═══════════════════════════════════════════════ */
// static void wifi_event_handler(void *arg, esp_event_base_t base,
//                                 int32_t event_id, void *event_data)
// {
//     if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         ESP_LOGW(TAG, "WiFi mat ket noi, thu lai...");
//         esp_wifi_connect();
//     } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
//         ESP_LOGI(TAG, "WiFi co IP: " IPSTR, IP2STR(&ev->ip_info.ip));
//         xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
//     }
// }

// /* ═══════════════════════════════════════════════
//  *  WIFI INIT — block cho đến khi có IP
//  * ═══════════════════════════════════════════════ */
// static void wifi_init(void)
// {
//     s_wifi_event_group = xEventGroupCreate();

//     ESP_ERROR_CHECK(nvs_flash_init());
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     ESP_ERROR_CHECK(esp_event_handler_instance_register(
//         WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(
//         IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

//     wifi_config_t wifi_cfg = {
//         .sta = {
//             .ssid      = WIFI_SSID,
//             .password  = WIFI_PASS,
//             .threshold.authmode = WIFI_AUTH_WPA2_PSK,
//         },
//     };
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
//     ESP_ERROR_CHECK(esp_wifi_start());
//     ESP_ERROR_CHECK(esp_wifi_connect());

//     ESP_LOGI(TAG, "WiFi connecting: %s — cho IP...", WIFI_SSID);

//     /* BLOCK tại đây đến khi IP_EVENT_STA_GOT_IP */
//     xEventGroupWaitBits(s_wifi_event_group,
//                         WIFI_CONNECTED_BIT,
//                         pdFALSE, pdTRUE, portMAX_DELAY);

//     ESP_LOGI(TAG, "WiFi san sang, bat dau MQTT...");
// }

// /* ═══════════════════════════════════════════════
//  *  PUBLIC: khởi tạo WiFi + MQTT
//  * ═══════════════════════════════════════════════ */
// void mqtt_control_init(void)
// {
//     s_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, sizeof(char));
//     configASSERT(s_cmd_queue);

//     wifi_init();

//     esp_mqtt_client_config_t mqtt_cfg = {
//         .broker.address.uri = MQTT_BROKER_URI,
//         .credentials.client_id = MQTT_CLIENT_ID,
//     };
//     esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
//     esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
//                                    mqtt_event_handler, NULL);
//     esp_mqtt_client_start(client);

//     ESP_LOGI(TAG, "MQTT client started → %s", MQTT_BROKER_URI);
//     ESP_LOGI(TAG, "Topic lệnh: %s", MQTT_CMD_TOPIC);
// }

// /* ═══════════════════════════════════════════════
//  *  PUBLIC: task xử lý dir byte → motor
//  * ═══════════════════════════════════════════════ */
// void mqtt_control_task(void *arg)
// {
//     char dir;

//     for (;;) {
//         /* Block chờ mode MQTT được kích hoạt */
//         xEventGroupWaitBits(g_mode_event_group,
//                             BIT_MODE_MQTT,
//                             pdFALSE,
//                             pdTRUE,
//                             portMAX_DELAY);

//         /* Đọc 1 byte dir từ queue, timeout 200 ms */
//         if (xQueueReceive(s_cmd_queue, &dir, pdMS_TO_TICKS(200)) == pdTRUE) {

//             /* Kiểm tra lại mode (có thể đã chuyển trong lúc chờ) */
//             if (mode_manager_get() != MODE_MQTT) {
//                 motor_set_lr(0, 0);
//                 continue;
//             }

//             lr_speed_t spd = dir_to_speed(dir);
//             motor_set_lr(spd.left, spd.right);

//             static const char *dir_name[] = {
//                 ['F'] = "TIEN", ['B'] = "LUI",
//                 ['L'] = "TRAI", ['R'] = "PHAI", ['S'] = "DUNG"
//             };
//             const char *name = (dir == 'F' || dir == 'B' ||
//                                  dir == 'L' || dir == 'R' || dir == 'S')
//                                 ? dir_name[(int)dir] : "???";

//             ESP_LOGI(TAG, "MQTT cmd: '%c' %-5s | L=%4d R=%4d",
//                      dir, name, spd.left, spd.right);
//         }

//         /* Nếu mode đã chuyển khỏi MQTT → dừng motor */
//         if (mode_manager_get() != MODE_MQTT) {
//             motor_set_lr(0, 0);
//             ESP_LOGI(TAG, "Roi MODE_MQTT, dung motor");
//         }
//     }
// }
#include "mqtt_control.h"
#include "mode_manager.h"
#include "joystick_motor.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <string.h>

/* Bit báo hiệu WiFi đã có IP */
#define WIFI_CONNECTED_BIT  BIT0
static EventGroupHandle_t s_wifi_event_group;

/* ═══════════════════════════════════════════════
 *  WiFi credentials — điền vào đây
 * ═══════════════════════════════════════════════ */
#define WIFI_SSID   "IMC Lab"
#define WIFI_PASS   "IMC@2026"

static const char *TAG = "MQTT_CTRL";

/* ═══════════════════════════════════════════════
 *  Queue: chỉ cần truyền 1 ký tự dir ('F','B','L','R','S')
 *  sau khi đã parse JSON trong callback
 * ═══════════════════════════════════════════════ */
#define CMD_QUEUE_LEN   4

static QueueHandle_t              s_cmd_queue   = NULL;   /* queue 1 byte dir */
static esp_mqtt_client_handle_t   s_mqtt_client = NULL;   /* dùng cho publish_alert */

/* ═══════════════════════════════════════════════
 *  PARSE JSON TỐI GIẢN — không dùng cJSON
 *
 *  Flutter gửi payload dạng:
 *    {"cmd":"MOVE","dir":"F","ts":1234}
 *    {"cmd":"STOP","dir":"S","ts":1234}
 *    {"cmd":"SPEED","spd":80,"ts":1234}
 *
 *  Chỉ cần lấy:
 *    - "cmd" : nếu là "SPEED" → bỏ qua
 *    - "dir" : 1 ký tự F/B/L/R/S
 * ═══════════════════════════════════════════════ */
static char parse_dir(const char *payload, int len)
{
    /* Tìm "dir":"X" trong chuỗi payload */
    const char *key = "\"dir\":\"";
    int key_len = (int)strlen(key);

    for (int i = 0; i < len - key_len - 1; i++) {
        if (strncmp(payload + i, key, key_len) == 0) {
            return payload[i + key_len];   /* ký tự ngay sau dấu " */
        }
    }
    return 0;   /* không tìm thấy */
}

/* ═══════════════════════════════════════════════
 *  MAP dir → tốc độ (L, R)
 * ═══════════════════════════════════════════════ */
typedef struct { int left; int right; } lr_speed_t;

static lr_speed_t dir_to_speed(char dir)
{
    switch (dir) {
        case 'F': return (lr_speed_t){ PWM_MAX,  PWM_MAX};
        case 'B': return (lr_speed_t){-PWM_MAX, -PWM_MAX};
        case 'L': return (lr_speed_t){PWM_TURN_MIN,  PWM_MAX};
        case 'R': return (lr_speed_t){ PWM_MAX, PWM_TURN_MIN};
        default:  return (lr_speed_t){0, 0};   /* 'S' hoặc unknown → dừng */
    }
}

/* ═══════════════════════════════════════════════
 *  MQTT EVENT HANDLER
 *  Parse JSON ngay trong callback, đẩy 1 byte dir vào queue
 * ═══════════════════════════════════════════════ */
static void mqtt_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    if (event_id == MQTT_EVENT_DATA) {
        /* Kiểm tra đúng topic */
        if (event->topic_len > 0 &&
            strncmp(event->topic, MQTT_CMD_TOPIC, event->topic_len) != 0) {
            return;
        }

        /* Bỏ qua SPEED command (không có "dir") */
        if (strnstr(event->data, "\"SPEED\"", event->data_len) != NULL) {
            ESP_LOGD(TAG, "SPEED cmd: bo qua (khong dieu khien huong)");
            return;
        }

        char dir = parse_dir(event->data, event->data_len);
        if (dir == 0) {
            ESP_LOGW(TAG, "Khong parse duoc dir tu payload");
            return;
        }

        /* Gửi 1 byte vào queue; không block */
        xQueueSend(s_cmd_queue, &dir, 0);

    } else if (event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "MQTT connected → subscribe: %s", MQTT_CMD_TOPIC);
        esp_mqtt_client_subscribe(event->client, MQTT_CMD_TOPIC, 0);

    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "MQTT disconnected");
    }
}


/* ═══════════════════════════════════════════════
 *  WIFI EVENT HANDLER
 *  Set bit khi có IP → wifi_init() thoát block
 * ═══════════════════════════════════════════════ */
static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi mat ket noi, thu lai...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi co IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ═══════════════════════════════════════════════
 *  WIFI INIT — block cho đến khi có IP
 * ═══════════════════════════════════════════════ */
static void wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid      = WIFI_SSID,
            .password  = WIFI_PASS,
            // .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .threshold.authmode = WIFI_AUTH_WPA_PSK,

        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "WiFi connecting: %s — cho IP...", WIFI_SSID);

    /* BLOCK tại đây đến khi IP_EVENT_STA_GOT_IP */
    xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "WiFi san sang, bat dau MQTT...");
}

/* ═══════════════════════════════════════════════
 *  PUBLIC: khởi tạo WiFi + MQTT
 * ═══════════════════════════════════════════════ */
void mqtt_control_init(void)
{
    s_cmd_queue = xQueueCreate(CMD_QUEUE_LEN, sizeof(char));
    configASSERT(s_cmd_queue);

    wifi_init();

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .credentials.client_id = MQTT_CLIENT_ID,
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);

    ESP_LOGI(TAG, "MQTT client started → %s", MQTT_BROKER_URI);
    ESP_LOGI(TAG, "Topic lệnh: %s", MQTT_CMD_TOPIC);
}

/* ═══════════════════════════════════════════════
 *  PUBLIC: publish cảnh báo sensor lên MQTT
 *  Gọi được từ sensor_monitor_task (bất kỳ thread nào).
 *  Trả về ngay (QoS 0, non-blocking).
 * ═══════════════════════════════════════════════ */
void mqtt_publish_alert(const char *topic, const char *payload)
{
    if (s_mqtt_client == NULL) {
        ESP_LOGW(TAG, "mqtt_publish_alert: client chua san sang");
        return;
    }
    int msg_id = esp_mqtt_client_publish(s_mqtt_client,
                                         topic, payload,
                                         0,    /* len=0: tự tính strlen */
                                         0,    /* QoS 0 */
                                         0);   /* retain = 0 */
    if (msg_id < 0) {
        ESP_LOGW(TAG, "mqtt_publish_alert: publish that bai (topic=%s)", topic);
    } else {
        ESP_LOGI(TAG, "ALERT published → [%s] %s", topic, payload);
    }
}

/* ═══════════════════════════════════════════════
 *  PUBLIC: task xử lý dir byte → motor
 * ═══════════════════════════════════════════════ */
void mqtt_control_task(void *arg)
{
    char dir;

    for (;;) {
        /* Block chờ mode MQTT được kích hoạt */
        xEventGroupWaitBits(g_mode_event_group,
                            BIT_MODE_MQTT,
                            pdFALSE,
                            pdTRUE,
                            portMAX_DELAY);

        /* Đọc 1 byte dir từ queue, timeout 200 ms */
        if (xQueueReceive(s_cmd_queue, &dir, pdMS_TO_TICKS(200)) == pdTRUE) {

            /* Kiểm tra lại mode (có thể đã chuyển trong lúc chờ) */
            if (mode_manager_get() != MODE_MQTT) {
                motor_set_lr(0, 0);
                continue;
            }

            lr_speed_t spd = dir_to_speed(dir);
            motor_set_lr(spd.left, spd.right);

            static const char *dir_name[] = {
                ['F'] = "TIEN", ['B'] = "LUI",
                ['L'] = "TRAI", ['R'] = "PHAI", ['S'] = "DUNG"
            };
            const char *name = (dir == 'F' || dir == 'B' ||
                                 dir == 'L' || dir == 'R' || dir == 'S')
                                ? dir_name[(int)dir] : "???";

            ESP_LOGI(TAG, "MQTT cmd: '%c' %-5s | L=%4d R=%4d",
                     dir, name, spd.left, spd.right);
        }

        /* Nếu mode đã chuyển khỏi MQTT → dừng motor */
        if (mode_manager_get() != MODE_MQTT) {
            motor_set_lr(0, 0);
            ESP_LOGI(TAG, "Roi MODE_MQTT, dung motor");
        }
    }
}