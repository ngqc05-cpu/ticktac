#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ntp_component.h"
#include "csi_collector.h"
#include "nvs_component.h"

static const char *TAG = "APP_MAIN";

#define WIFI_SSID   "daylassid"
#define WIFI_PASS   "12345678"              //wifi kết nối lúc đầu để đồng bộ thời gian NTP, sau đó sẽ không cần dùng đến nữa

// thay bằng MAC cần lọc           
static const uint8_t TARGET_ASUS_MAC[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x66};          //  d0:cf:13:e3:6f:6d  là mac của esp32c5                 
#define WIFI_CHANNEL_5G  100 // kênh phát   


void app_main(void) {
    ESP_ERROR_CHECK(nvs_component_init());
    if (!ntp_time_sync_and_cleanup(WIFI_SSID, WIFI_PASS)) {
        return;
    }

    csi_collector_init(TARGET_ASUS_MAC,WIFI_CHANNEL_5G);
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // cứ để task main ngủ yên
    }
}