#ifndef DROP_DETECTION_H
#define DROP_DETECTION_H
/** \addtogroup Drop_Detection Detección de gotas
 ** @{ */

/** \brief Detección de gotas por correlación cruzada sobre la señal filtrada del sensor óptico.
 *
 * Cada canal tiene su detector (componente xcorr_detector de middleware) con la plantilla de la gota
 * de ese canal (drop_template.h, copia de la de projects/pruebabpw34) y su umbral. Una gota se
 * avisa UNA sola vez aunque la vean los dos canales: tras avisar se ignoran ambos canales durante
 * XCORR_REFRACTORY_MS. Es la misma lógica de MODE_DETECTOR de pruebabpw34, que no se modificó.
 *
 * El cálculo de volumen y caudal (Goteo_*) está en modules/sensado y el de las alarmas en el
 * componente alarmas: este componente solo avisa cuándo cayó una gota.
 *
 * @note La plantilla y los umbrales valen para el muestreo, el filtro y el antialias de
 * optical_sensor (ver el comentario de drop_template.h).
 *
 * @note La función de aviso se ejecuta en la tarea de muestreo del sensor: tiene que ser corta y
 * no bloquear (por ejemplo, poner la hora de la gota en una cola).
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 29/10/2026 | Document creation                              |
 *
 */

#include <stdint.h>
#include <stdbool.h>

/*==================[typedef]================================================*/
/**
 * @brief Función de aviso de gota
 * @param ahora_us Hora de la gota, en microsegundos (esp_timer_get_time())
 */
typedef void (*drop_cb_t)(int64_t ahora_us);

/*==================[external functions declaration]=========================*/

/** @fn bool DropDetection_Init(drop_cb_t cb)
 * @brief Arma los detectores e inicia el sensor óptico (OpticalSensor_Init). Con el haz libre
 * al llamarla.
 * @return true si quedó detectando
 */
bool DropDetection_Init(drop_cb_t cb);

/** @fn float DropDetection_UltimoPico(int *canal)
 * @brief Pico de la correlación de la última gota detectada, en mV (para depuración: comparado con
 * el umbral de ese canal, 160 mV el canal 1 y 140 mV el 2, indica si fue una gota clara o un
 * disparo por ruido).
 * @param canal Si no es NULL, recibe el canal que la detectó (1 o 2)
 */
float DropDetection_UltimoPico(int *canal);

/** @} */
#endif /* DROP_DETECTION_H */
