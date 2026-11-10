#ifndef BLE_GOTEO_H
#define BLE_GOTEO_H
/** \addtogroup BleGoteo Driver BLE del monitor de goteo
 ** @{ */

/** \brief Driver BLE (servidor GATT) adaptado de drivers/microcontroller/ble_mcu.
 *
 * Mantiene la idea del driver original (un servicio 0xFFE0 con una característica 0xFFE1 que sirve en los dos
 * sentidos, como un módulo HM-10) y corrige lo que no servía para la app: el nombre del equipo es configurable y
 * va en el paquete de anuncio, no pide emparejamiento ni cifrado, el anuncio es lento (~1 s), distingue las
 * escrituras de datos de la activación de notificaciones, controla el largo de lo que envía, respeta la congestión y
 * se puede apagar. No usa tareas propias: los datos recibidos van a una cola.
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
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/*==================[macros]=================================================*/
#define BLE_GOTEO_RX_MAX   244   /*!< Máximo de bytes de una escritura de la app (MTU de 247 menos 3) */
#define BLE_GOTEO_NOMBRE_MAX 20  /*!< Largo máximo del nombre (entra en el paquete de anuncio) */

/*==================[typedef]================================================*/
/**
 * @brief Un pedazo de datos escrito por la app (la cola los entrega en el orden en que llegaron)
 */
typedef struct {
    uint8_t largo;                       /*!< Bytes válidos de datos */
    uint8_t datos[BLE_GOTEO_RX_MAX];
} ble_goteo_rx_t;

/*==================[external functions declaration]=========================*/

/** @fn bool BleGoteo_Init(const char *nombre, QueueHandle_t cola_rx)
 * @brief Inicia el controlador y la pila Bluetooth y empieza a anunciarse.
 * @param nombre Nombre del equipo (hasta BLE_GOTEO_NOMBRE_MAX caracteres)
 * @param cola_rx Cola de elementos ble_goteo_rx_t donde se dejan los datos que escribe la app
 */
bool BleGoteo_Init(const char *nombre, QueueHandle_t cola_rx);

/** @fn bool BleGoteo_Conectado(void)
 * @brief true si hay una app conectada
 */
bool BleGoteo_Conectado(void);

/** @fn bool BleGoteo_Enviar(const uint8_t *datos, size_t largo)
 * @brief Envía datos a la app por notificaciones, en pedazos del tamaño que permita el MTU y con una pausa entre
 * uno y otro. Bloquea hasta terminar. No se debe llamar desde los callbacks de Bluetooth.
 * @return false si no hay una app conectada con las notificaciones activadas, o se cortó la conexión
 */
bool BleGoteo_Enviar(const uint8_t *datos, size_t largo);

/** @fn void BleGoteo_Apagar(void)
 * @brief Corta la conexión, deja de anunciar y apaga la pila y el controlador (reposo)
 */
void BleGoteo_Apagar(void);

/** @} */
#endif /* BLE_GOTEO_H */
