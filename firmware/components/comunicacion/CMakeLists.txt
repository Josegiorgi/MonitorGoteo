# Comunicación con la app de la enfermera (Bluetooth Low Energy). Es un componente aparte: todo el BLE
# (driver adaptado, protocolo y tarea) está en esta carpeta. Para sacarlo del firmware alcanza con quitar
# Comunicacion_Init y Comunicacion_Apagar del main y el componente de la lista de COMPONENTS del proyecto.
idf_component_register(SRCS "src/ble_goteo.c" "src/protocolo.c" "src/comunicacion.c"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES registro
                       PRIV_REQUIRES alarmas bt esp_timer)
