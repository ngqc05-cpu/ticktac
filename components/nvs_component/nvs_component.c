#include "nvs_component.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "NVS_INIT";

esp_err_t nvs_component_init(void) {
    ESP_LOGI(TAG, "Đang khởi tạo NVS Flash...");
    
    esp_err_t ret = nvs_flash_init();
    
    // Nếu NVS bị lỗi do không còn trang trống hoặc bị thay đổi cấu trúc phân vùng
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS flash lỗi hoặc khác phiên bản. Đang tiến hành xóa và định dạng lại...");
        
        // Xóa sạch NVS
        ESP_ERROR_CHECK(nvs_flash_erase());
        
        // Thử khởi tạo lại
        ret = nvs_flash_init();
    }
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Khởi tạo NVS Flash thành công!");
    } else {
        ESP_LOGE(TAG, "Khởi tạo NVS Flash thất bại với mã lỗi: %s", esp_err_to_name(ret));
    }
    
    return ret;
}