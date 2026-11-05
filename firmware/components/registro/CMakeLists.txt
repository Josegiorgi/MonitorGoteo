idf_component_register(SRCS "src/registro.c"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash
                       PRIV_REQUIRES esp_timer esp_partition)
