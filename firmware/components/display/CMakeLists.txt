idf_component_register(SRCS "src/display.c"
                       INCLUDE_DIRS "include"
                       REQUIRES u8g2
                       PRIV_REQUIRES drivers freertos esp_rom)
