#ifndef PROTOCOLO_H
#define PROTOCOLO_H
/** \addtogroup Protocolo Protocolo con la app
 ** @{ */

/** \brief Órdenes de la app y formato en texto del registro, según documentación/cambios_para_la_app.md.
 *
 * Junta lo que escribe la app hasta encontrar un '\n', ejecuta la orden y responde. Para DESCARGAR convierte las líneas
 * binarias del registro (componente registro) a texto, de la última infusión y hasta su FIN si ya terminó. No usa
 * Bluetooth directamente: envía por la función que se le da, así se puede probar sin Bluetooth.
 *
 * Órdenes: HORA=<epoch_s>, PACIENTE=<codigo>, ESTADO, DESCARGAR, OK. Respuestas: ACK,<orden> o ERR,<motivo>.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 07/11/2026 | Document creation                              |
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*==================[typedef]================================================*/
/**
 * @brief Lo que el protocolo necesita de afuera
 */
typedef struct {
    bool (*enviar)(const uint8_t *datos, size_t largo);   /*!< Envía a la app (false si se cortó la conexión) */
    uint8_t (*estado)(void);                              /*!< 1 = en curso, 0 = terminada, 2 = interrumpida por un reinicio */
    uint32_t (*minuto_actual)(void);                      /*!< Minuto actual que se informa (desde el inicio, o desde el reinicio si está interrumpida) */
    void (*descargado)(void);                             /*!< Opcional: la app confirmó la descarga (OK) */
} protocolo_enlace_t;

/*==================[external functions declaration]=========================*/

/** @fn void Protocolo_Init(const protocolo_enlace_t *enlace)
 * @brief Guarda las funciones de enlace (deben seguir existiendo)
 */
void Protocolo_Init(const protocolo_enlace_t *enlace);

/** @fn void Protocolo_Recibir(const uint8_t *datos, size_t largo)
 * @brief Procesa lo que escribió la app: junta los pedazos hasta el '\n' y ejecuta la orden. DESCARGAR bloquea
 * hasta enviar todo el registro.
 */
void Protocolo_Recibir(const uint8_t *datos, size_t largo);

/** @fn void Protocolo_FinDeEntrada(void)
 * @brief Pasó un rato sin recibir datos: si quedó una orden sin el '\n' (por ejemplo escrita a mano en nRF Connect), se
 * ejecuta igual. La app real termina siempre sus órdenes en '\n'.
 */
void Protocolo_FinDeEntrada(void);

/** @fn void Protocolo_Reiniciar(void)
 * @brief Se cortó la conexión: descarta la orden a medio recibir y la confirmación pendiente
 */
void Protocolo_Reiniciar(void);

/** @} */
#endif /* PROTOCOLO_H */
