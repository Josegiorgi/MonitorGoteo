idf_component_register(SRCS "src/drop_detection.c"
                       INCLUDE_DIRS "include"
                       REQUIRES optical_sensor
                       PRIV_REQUIRES middleware esp_timer)
