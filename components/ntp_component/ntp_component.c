#include "ntp_component.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "led_strip.h"


#define LED_GPIO        27
#define LED_BRIGHTNESS  10 

static const char *TAG = "NTP_SYNC";

// quản lý led 
static led_strip_handle_t s_led_strip = NULL;

static void led_init(void) {
    if (s_led_strip != NULL) return; 
    
    led_strip_config_t strip_cfg = {
        .strip_gpio_num   = LED_GPIO,
        .max_leds         = 1,
        .led_model        = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src       = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,   // 10 MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_led_strip));
    led_strip_clear(s_led_strip);
}

static void led_set_color(uint8_t r, uint8_t g, uint8_t b) {
    if (s_led_strip) {
        led_strip_set_pixel(s_led_strip, 0, r, g, b);
        led_strip_refresh(s_led_strip);
    }
}



#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_GOT_IP_BIT     BIT1
static EventGroupHandle_t s_wifi_eg;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        static int retry = 0;
        if (retry < 10) {
            esp_wifi_connect();
            retry++;
        } else {
            xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT); // hủy block
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT);
    }
}

//đồng bộ
bool ntp_time_sync_and_cleanup(const char* ssid, const char* pass) {
    bool success = false;
    
    // khởi tạo LED và báo trạng thái Vàng (Đang chờ)
    led_init();
    led_set_color(LED_BRIGHTNESS, LED_BRIGHTNESS / 2, 0); // Vàng

    // khởi tạo các dịch vụ nền tảng
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    s_wifi_eg = xEventGroupCreate();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();

    // đăng ký sự kiện (Lưu instance để sau này xóa)
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_cfg = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg  = { .capable = true, .required = false },
        },
    };
    strlcpy((char *)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid));
    strlcpy((char *)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();

    ESP_LOGI(TAG, "đang kết nối Wi-Fi để lấy giờ...");

    // Chờ có IP (timeout 20s)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg, WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(20000));

    if ((bits & (WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT)) == (WIFI_CONNECTED_BIT | WIFI_GOT_IP_BIT)) {
        ESP_LOGI(TAG, "đã có IP , đang đồng bộ NTP...");
        
        // đồng bộ SNTP
        esp_sntp_config_t sntp_cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("time.google.com");
        esp_netif_sntp_init(&sntp_cfg);

        int retry = 0;
        while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_ERR_TIMEOUT && ++retry < 10) {
            ESP_LOGW(TAG, "chờ NTP... (%d/10)", retry);
        }

        if (retry < 10) {
            setenv("TZ", "ICT-7", 1); 
            tzset();
            success = true;
            ESP_LOGI(TAG, "đồng bộ thời gian thành công!");
        }
        esp_netif_sntp_deinit();
    }

    // dọn dẹp
    ESP_LOGI(TAG, "ngắt kết nối và giải phóng Wi-Fi khỏi RAM...");
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // hủy đăng ký Event để tránh rò rỉ bộ nhớ
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    
    esp_netif_destroy_default_wifi(sta_netif);
    vEventGroupDelete(s_wifi_eg);

    // báo cáo trạng thái cuối cùng
    if (success) {
        led_set_color(0, LED_BRIGHTNESS, 0); // Xanh lá
    } else {
        led_set_color(LED_BRIGHTNESS, 0, 0); // Đỏ
    }

    return success;
}
uint64_t ntp_get_current_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}