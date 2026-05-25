idf_component_register(SRCS "csi_collector.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_csi_gain_ctrl esp_wifi ntp_component esp_driver_uart freertos esp_driver_gpio log esp_hw_support)