idf_component_register(SRCS "src/bateria.c"
                       INCLUDE_DIRS "include"
                       REQUIRES drivers
                       PRIV_REQUIRES esp_timer)
