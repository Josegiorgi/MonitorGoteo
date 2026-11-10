#ifndef COMUNICACION_H
#define COMUNICACION_H
/** \addtogroup Comunicacion Comunicación con la app
 ** @{ */

/** \brief Comunicación Bluetooth Low Energy con la app Android de la enfermera.
 *
 * Es un componente **separado** del resto del firmware: el main solo lo inicia y lo apaga. La app descarga el
 * registro de la infusión (componente registro) para auditarla; no ve el estado en tiempo real ni cambia nada del
 * equipo. El contrato completo con la app está en documentación/cambios_para_la_app.md.
 *
 * - **BLE siempre anunciando** mientras el equipo está prendido (nombre MonitorGoteo-XX), una sola conexión a la vez, sin
 *   emparejamiento. No envía nada si nadie está conectado.
 * - **Un solo canal** (servicio 0xFFE0, característica 0xFFE1): la app escribe las órdenes y el equipo notifica las
 *   respuestas y el registro, todo en texto UTF-8 y con líneas terminadas en '\n'.
 * - **Órdenes** (de la app): HORA=<epoch_s>, PACIENTE=<codigo> (se valida y no se guarda), ESTADO (ID de la infusión, su estado
 *   1/0/2 y el minuto actual, para calcular la hora de inicio sin descargar), DESCARGAR y OK.
 * - **Respuestas:** ACK,<orden> o ERR,<motivo>. DESCARGAR envía el registro de la última infusión como texto y cierra
 *   con FIN_TX,<lineas>,<minuto_actual>. Con OK la app confirma que lo recibió completo: el equipo anota DESCARGA y marca
 *   el registro como descargado.
 *
 * Archivos: ble_goteo (driver BLE, adaptado de drivers/microcontroller/ble_mcu), protocolo (orden y formato de las
 * líneas) y comunicacion (tarea que une los dos).
 *
 * @note Hace falta Bluetooth activado en el sdkconfig del proyecto (CONFIG_BT_ENABLED, Bluedroid) y los componentes
 * comunicacion y bt en la lista de COMPONENTS.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 07/11/2026 | Document creation                              |
 * | 10/11/2026 | ESTADO con tres estados (1 en curso, 0 terminada, 2 interrumpida por un reinicio); el minuto sigue contando después del fin hasta el OK; REINICIO con su minuto; callback descargado |
 *
 */

#include <stdint.h>
#include <stdbool.h>

/*==================[typedef]================================================*/
/**
 * @brief Lo que el firmware le informa a la comunicación (así el componente no depende del main)
 */
typedef struct {
    uint8_t (*estado)(void);          /*!< Estado de la última infusión: 1 = en curso, 0 = terminada, 2 = interrumpida por un reinicio */
    uint32_t (*minuto_actual)(void);  /*!< Minuto actual (para ESTADO y FIN_TX): desde el inicio de la infusión (sigue contando después del fin hasta el OK)
                                           o, si está interrumpida, desde el reinicio */
    void (*descargado)(void);         /*!< Opcional: se llama cuando la app confirma la descarga (OK); el firmware deja de contar el minuto */
} comunicacion_enlace_t;

/*==================[external functions declaration]=========================*/

/** @fn bool Comunicacion_Init(const char *nombre, const comunicacion_enlace_t *enlace)
 * @brief Inicia el Bluetooth, empieza a anunciarse con el nombre indicado y arranca la tarea de comunicación.
 * @param nombre Nombre del equipo que ve la app (hasta 20 caracteres), por ejemplo "MonitorGoteo-01"
 * @param enlace Funciones con las que consulta el estado de la infusión (deben seguir existiendo)
 * @return true si quedó andando
 */
bool Comunicacion_Init(const char *nombre, const comunicacion_enlace_t *enlace);

/** @fn void Comunicacion_Apagar(void)
 * @brief Apaga el Bluetooth por completo (reposo). Para volver a usarlo hay que reiniciar el equipo.
 */
void Comunicacion_Apagar(void);

/** @fn bool Comunicacion_Conectado(void)
 * @brief true si la app está conectada
 */
bool Comunicacion_Conectado(void);

/** @} */
#endif /* COMUNICACION_H */
