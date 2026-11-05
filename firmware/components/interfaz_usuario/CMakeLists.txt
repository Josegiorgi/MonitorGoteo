file(GLOB_RECURSE SOURCES src/*.c)
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "include"
                       REQUIRES drivers u8g2 display freertos esp_timer
                       PRIV_REQUIRES esp_driver_gpio esp_hw_support)
