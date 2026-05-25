#ifndef CSI_COLLECTOR_H
#define CSI_COLLECTOR_H

#include <stdint.h>
#include <stdbool.h>

#define CSI_SUBCARRIERS  64
#define CSI_DATA_LEN     (CSI_SUBCARRIERS * 2) // 128 bytes (ảo và thực)

/**
 * @brief Cấu trúc gói tin nhị phân gửi về máy tính
 * Sử dụng __attribute__((packed)) để đảm bảo không có byte rỗng giữa các trường
 */
// Cấu trúc gói tin 155 Bytes xuất qua UART
typedef struct __attribute__((packed)) {
   uint16_t magic_bytes;     // Offset 0: 2 Bytes (Nhận diện đầu frame)
    uint8_t  packet_length;   // Offset 2: 1 Byte  (Tổng kích thước)
    uint8_t  monitor_mac[6];  // Offset 3: 6 Bytes (MAC thiết bị)
    uint32_t sequence_num;        // Offset 9: 4 Bytes (Số thứ tự)
    uint64_t timestamp_us;    // Offset 13: 8 Bytes (Thời gian Micro-seconds)
    int8_t   rssi;            // Offset 21: 1 Byte  (Cường độ tín hiệu)
    uint8_t  channel;         // Offset 22: 1 Byte  (Kênh Wi-Fi)
    uint8_t  agc_gain;        // Offset 23: 1 Byte  (Độ lợi AGC - Không dấu)
    int8_t   fft_gain;        // Offset 24: 1 Byte  (Độ lợi FFT - CÓ DẤU) <--- ĐÃ SỬA
    int8_t   noise_floor;     // Offset 25: 1 Byte  (Mức nhiễu nền)
    int8_t   csi_data[128];   // Offset 26: 128 Bytes (64 cặp I/Q)
    uint8_t  xor_checksum;    // Offset 160: 1 Byte (XOR Checksum)
} csi_binary_packet_t;

/**
 * @brief Khởi tạo bộ thu CSI
 * @param filter_mac MAC của router Asus (để chỉ lọc lấy CSI từ nguồn này)
 */
void csi_collector_init(const uint8_t *filter_mac , uint8_t channel);

#endif // CSI_COLLECTOR_H