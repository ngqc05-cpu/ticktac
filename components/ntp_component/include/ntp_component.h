#ifndef NTP_COMPONENT_H
#define NTP_COMPONENT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Khởi tạo đồng bộ thời gian NTP qua Wi-Fi rồi dọn dẹp RAM.
 * hàm này sẽ:
 * bật LED Vàng
 * kết nối mạng
 * đồng bộ thời gian qua SNTP (chuẩn múi giờ ICT-7).
 * xóa hoàn toàn Wi-Fi khỏi RAM (hủy đăng ký event, deinit).
 * bật LED Xanh (nếu thành công) hoặc LED Đỏ (nếu thất bại).
 * 
 * @param ssid Tên mạng Wi-Fi
 * @param pass Mật khẩu Wi-Fi
 * @return true nếu đồng bộ thành công, false nếu lỗi.
 */
bool ntp_time_sync_and_cleanup(const char* ssid, const char* pass);

/**
 * @brief Lấy thời gian thực (Real-time) tính bằng mili-giây.
 * Hhàm này dùng để đóng dấu thời gian (timestamp) cho các gói CSI.
 */
uint64_t ntp_get_current_time_us(void);

#endif // NTP_COMPONENT_H