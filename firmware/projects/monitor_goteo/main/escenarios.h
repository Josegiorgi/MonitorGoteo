#ifndef ESCENARIOS_H
#define ESCENARIOS_H
/** \addtogroup Escenarios Interruptores de compilación
 ** @{ */

/** \brief Los dos interruptores con los que se compila el firmware: Bluetooth y escenario de prueba.
 *
 * Para la versión final: USAR_BLE = 1 y ESCENARIO = ESCENARIO_NORMAL. Los escenarios de prueba precargan la
 * configuración de la infusión y acortan los tiempos para probar las alarmas sin esperar minutos (el resto de los
 * tiempos de prueba se pasan en monitor_goteo.c, con Alarmas_SetTiempos y UI_SetTiempos).
 *
 * Los #warning que avisan que quedó compilado fuera de la versión final están en monitor_goteo.c, para que salgan una
 * sola vez.
 */

/*==================[macros and definitions]=================================*/
/* Bluetooth con la app: 1 = el firmware final. 0 = firmware SIN Bluetooth (como antes del BLE), para probar el resto
 * del equipo. VOLVER A 1 para la versión final. */
#define USAR_BLE                     1

/* Escenario: qué configuración arranca precargada y si los tiempos de alarma se acortan.
 * Cambiar esta línea para probar y VOLVER A ESCENARIO_NORMAL para la versión de verdad. */
#define ESCENARIO_NORMAL             0
#define ESCENARIO_OCLUSION_FIN       1
#define ESCENARIO_DESVIO_COLOCACION  2
#define ESCENARIO                    ESCENARIO_OCLUSION_FIN   /*!< por defecto, la versión final es ESCENARIO_NORMAL */

#if ESCENARIO == ESCENARIO_OCLUSION_FIN
#define NOMBRE_ESCENARIO      "OCLUSION_FIN"
#define CONFIG_FACTOR         10    /*!< gtt/mL: con 50 mL son solo 500 gotas */
#define CONFIG_VOLUMEN_ML     50
#define CONFIG_DURACION_MIN   10    /*!< objetivo 50 gtt/min */
#elif ESCENARIO == ESCENARIO_DESVIO_COLOCACION
#define NOMBRE_ESCENARIO      "DESVIO_COLOCACION"
#define CONFIG_FACTOR         20
#define CONFIG_VOLUMEN_ML     500
#define CONFIG_DURACION_MIN   (8 * 60)   /*!< objetivo 20,8 gtt/min: con un goteo de ~56 gtt/min hay desvío alto */
#else
#define CONFIG_FACTOR         20
#define CONFIG_VOLUMEN_ML     500
#define CONFIG_DURACION_MIN   (8 * 60)
#endif

/* Tiempos que cambian con el escenario */
#if ESCENARIO == ESCENARIO_NORMAL
#define AJUSTE_RECORDATORIO_S 600     /*!< Segundos en el Ajuste de goteo sin confirmar para empezar a recordarlo */
#define AJUSTE_BEEP_CADA_S    60      /*!< Cada cuánto suena el beep del recordatorio */
#define GUARDAR_CADA_S        60      /*!< Cada cuánto se escribe una muestra en el registro (contador, caudal y volumen): de ahí sale el último minuto guardado si el equipo se reinicia */
#else
#define AJUSTE_RECORDATORIO_S 30
#define AJUSTE_BEEP_CADA_S    10
#define GUARDAR_CADA_S        15
#endif

/** @} */
#endif /* ESCENARIOS_H */
