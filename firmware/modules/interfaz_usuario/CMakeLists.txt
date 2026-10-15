file(GLOB_RECURSE SOURCES src/*.c)
idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS "include"
                       REQUIRES drivers u8g2 u8g2_hal freertos esp_timer)
