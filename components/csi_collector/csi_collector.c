#include "csi_collector.h"
#include "ntp_component.h"
#include "esp_csi_gain_ctrl.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "driver/uart.h"       
#include "driver/gpio.h" 
#include "freertos/FreeRTOS.h" 
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "CSI_COLLECTOR";
static uint8_t s_filter_mac[6];
static uint8_t s_self_mac[6];
static csi_binary_packet_t s_packet;


#define UART_PORT_NUM      UART_NUM_1 
#define UART_BAUD_RATE     460800// tốc độ truyền 
#define UART_TX_PIN        (6)        // Nối vào chân DI của module RS485
#define UART_RX_PIN        (1)        // Nối vào chân RO của module RS485 

#define BLINK_GPIO         (8)

static QueueHandle_t s_csi_queue = NULL;

// Task chuyên dụng để tống dữ liệu nhị phân ra phần cứng
static void uart_tx_task(void *arg) {
    csi_binary_packet_t tx_pkt;
    while (1) {
        // Chờ nhận hàng từ Queue
        if (xQueueReceive(s_csi_queue, &tx_pkt, portMAX_DELAY)) {
            // Đẩy thẳng vào bộ đệm của UART Driver (Đã chuyển sang UART1)
            uart_write_bytes(UART_PORT_NUM, &tx_pkt, sizeof(csi_binary_packet_t));
        }
    }
}    

static void wifi_csi_cb(void *ctx, wifi_csi_info_t *info) {
    if (!info || !info->buf) return;
    if (memcmp(info->mac, s_filter_mac, 6) != 0) return; 

    s_packet.timestamp_us = ntp_get_current_time_us(); 
    memcpy(s_packet.monitor_mac, s_self_mac, 6);
    s_packet.rssi         = info->rx_ctrl.rssi;
    s_packet.channel      = info->rx_ctrl.channel;
    s_packet.noise_floor  = info->rx_ctrl.noise_floor;
    s_packet.sequence_num = info->rx_seq;

    static int s_pkt_count = 0;
    uint8_t current_agc = 0;
    int8_t current_fft = 0;
    float compensate_gain = 1.0f; 

    esp_csi_gain_ctrl_get_rx_gain(&info->rx_ctrl, &current_agc, &current_fft);

    s_packet.agc_gain = current_agc;
    s_packet.fft_gain = (uint8_t)current_fft; 

    if (s_pkt_count < 100) {
        esp_csi_gain_ctrl_record_rx_gain(current_agc, current_fft);
    } 
    else if (s_pkt_count == 100) {
        uint8_t base_agc; int8_t base_fft;
        esp_csi_gain_ctrl_get_rx_gain_baseline(&base_agc, &base_fft);
        ESP_LOGI(TAG, "Đã chốt Baseline! AGC_Base: %d, FFT_Base: %d", base_agc, base_fft);
    } 
    else {
        esp_csi_gain_ctrl_get_gain_compensation(&compensate_gain, current_agc, current_fft);
    }

    memcpy(s_packet.csi_data, info->buf, 128);
    s_pkt_count++;

    if (s_pkt_count % 10 == 0) {
        static uint8_t s_led_state = 0;
        s_led_state = !s_led_state; // Đảo từ 0 sang 1, hoặc từ 1 về 0
        gpio_set_level(BLINK_GPIO, s_led_state);
    }

    uint8_t calculated_checksum = 0;
    uint8_t *ptr = (uint8_t *)&s_packet;
    for (int i = 0; i < (sizeof(csi_binary_packet_t) - 1); i++) {
        calculated_checksum ^= ptr[i];
    }
    s_packet.xor_checksum = calculated_checksum;
    
    xQueueSend(s_csi_queue, &s_packet, 0);
}

void csi_collector_init(const uint8_t *filter_mac, uint8_t channel) {
    // 1. Cấu hình UART Driver
    uart_config_t uart_config = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, 1024, 2048, 0, NULL, 0)); 

    //khởi tạo Queue & Task
    s_csi_queue = xQueueCreate(40, sizeof(csi_binary_packet_t)); 
    xTaskCreate(uart_tx_task, "uart_tx_task", 2048, NULL, 10, NULL); 
    
    s_packet.magic_bytes = 0x55AA;
    s_packet.packet_length = sizeof(csi_binary_packet_t);

    memcpy(s_filter_mac, filter_mac, 6);
    esp_read_mac(s_self_mac, ESP_MAC_WIFI_STA);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));          
    ESP_ERROR_CHECK(esp_wifi_start());       
    ESP_ERROR_CHECK(esp_wifi_set_band_mode(WIFI_BAND_MODE_5G_ONLY));                               
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

    wifi_csi_config_t csi_config;
    memset(&csi_config,0,sizeof(csi_config));
    csi_config.enable                   = true;
    csi_config.acquire_csi_legacy       = false;
    csi_config.acquire_csi_force_lltf   = false;
    csi_config.acquire_csi_ht20         = true;
    csi_config.acquire_csi_ht40         = false;
    csi_config.acquire_csi_vht          = true;
    csi_config.acquire_csi_su           = true;
    csi_config.acquire_csi_mu           = true;
    csi_config.acquire_csi_dcm          = false;
    csi_config.acquire_csi_beamformed   = false;
    csi_config.acquire_csi_he_stbc_mode = 0;
    csi_config.val_scale_cfg            = 0;
    csi_config.lltf_bit_mode            = 0;
    csi_config.dump_ack_en              = false;
    csi_config.reserved                 = false;

    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));\
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));

    wifi_promiscuous_filter_t promis_filter = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA
                 | WIFI_PROMIS_FILTER_MASK_MGMT};
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&promis_filter));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_cb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    ESP_LOGI(TAG, "CSI Collector (5GHz) hoạt động trên kênh %d", channel);
}