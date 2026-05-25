idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES ntp_component csi_collector nvs_component freertos log)