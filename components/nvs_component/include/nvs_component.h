#ifndef NVS_COMPONENT_H
#define NVS_COMPONENT_H

#include "esp_err.h"

/**
 * @brief Khởi tạo Non-Volatile Storage (NVS) Flash
 * Hàm này bắt buộc phải được gọi trước khi khởi tạo các dịch vụ như WiFi, Bluetooth.
 * @return ESP_OK nếu khởi tạo thành công.
 */
esp_err_t nvs_component_init(void);

#endif // NVS_COMPONENT_H