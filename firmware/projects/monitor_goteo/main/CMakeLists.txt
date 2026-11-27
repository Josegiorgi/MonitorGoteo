idf_component_register(SRCS "monitor_goteo.c" "estado.c" "autotest.c" "infusion.c" "gestion_alarmas.c" "descarga.c"
                            "pendiente.c" "modo_tecnico.c" "reposo.c"
                    INCLUDE_DIRS ""
                    REQUIRES interfaz_usuario sensado alarmas buzzer_alarm drop_detection inclinacion bateria registro optical_sensor comunicacion)
