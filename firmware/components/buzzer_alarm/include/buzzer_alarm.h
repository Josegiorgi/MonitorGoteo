#ifndef BUZZER_ALARM_H
#define BUZZER_ALARM_H
/** \addtogroup Buzzer_Alarm Buzzer de alarma
 ** @{ */

/** \brief Patrones sonoros de alarma sobre el buzzer del driver buzzer (PWM).
 *
 * El buzzer va solo (sin LED) en GPIO_10 por defecto.
 *
 * Cada patrón es una secuencia de pasos (frecuencia, duración) que se repite hasta
 * BuzzerAlarm_Stop(). Se reproducen desde una tarea propia, así que BuzzerAlarm_Start() no
 * bloquea. Los patrones se distinguen por ritmo y por tono (no solo por tono), para que se
 * reconozcan sin mirar la pantalla:
 *
 * | Patrón            | Prioridad | Sonido                                         |
 * |:------------------|:---------:|:-----------------------------------------------|
 * | VOLUMEN_COMPLETO  | alta      | sirena rápida de dos tonos agudos, pausa corta |
 * | FLUJO_DETENIDO    | alta      | 3 pitidos rápidos y agudos, pausa larga        |
 * | FIN_SACHET        | alta      | 3 notas ascendentes, pausa larga               |
 * | COLOCACION        | alta      | 2 pitidos cortos de tono medio, pausa          |
 * | BATERIA_CRITICA   | alta      | 2 pitidos largos y graves, pausa               |
 * | DESVIACION_FUERTE | media     | dos tonos alternados (sirena lenta), pausa     |
 * | BATERIA_BAJA      | media     | un pitido largo y grave, pausa larga           |
 *
 * @note Los patrones son los eventos con sonido de la especificación de alarmas, en el mismo
 * orden de prioridad (de mayor a menor) que los eventos del componente alarmas. Los avisos no
 * suenan.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 29/10/2026 | Document creation                              |
 * | 29/10/2026 | Patrones de la especificación de alarmas: se suman VOLUMEN_COMPLETO y BATERIA_CRITICA y se ordenan por prioridad |
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "gpio_mcu.h"

/*==================[macros]=================================================*/
#define BUZZER_ALARM_GPIO   GPIO_10   /*!< Pin del buzzer */

/*==================[typedef]================================================*/
/**
 * @brief Patrones de alarma disponibles
 */
typedef enum {
    BUZZER_ALARM_VOLUMEN_COMPLETO = 0,
    BUZZER_ALARM_FLUJO_DETENIDO,
    BUZZER_ALARM_FIN_SACHET,
    BUZZER_ALARM_COLOCACION,
    BUZZER_ALARM_BATERIA_CRITICA,
    BUZZER_ALARM_DESVIACION_FUERTE,
    BUZZER_ALARM_BATERIA_BAJA,
    BUZZER_ALARM_CANTIDAD,          /*!< Cantidad de patrones (no es un patrón) */
} buzzer_alarm_t;

/*==================[external functions declaration]=========================*/

/** @fn bool BuzzerAlarm_Init(gpio_t pin)
 * @brief Inicia el PWM del buzzer en el pin dado y crea la tarea que reproduce los patrones.
 * @return true si quedó lista
 */
bool BuzzerAlarm_Init(gpio_t pin);

/** @fn void BuzzerAlarm_Start(buzzer_alarm_t patron)
 * @brief Empieza a repetir un patrón (si ya sonaba otro, lo reemplaza). No bloquea.
 */
void BuzzerAlarm_Start(buzzer_alarm_t patron);

/** @fn void BuzzerAlarm_Stop(void)
 * @brief Silencia el buzzer.
 */
void BuzzerAlarm_Stop(void);

/** @fn bool BuzzerAlarm_Sonando(void)
 * @brief true mientras el buzzer está emitiendo un tono (no en las pausas del patrón). Sirve para
 * no medir la batería mientras suena: el buzzer hace caer momentáneamente la tensión.
 */
bool BuzzerAlarm_Sonando(void);

/** @fn const char *BuzzerAlarm_Nombre(buzzer_alarm_t patron)
 * @brief Nombre corto del patrón, para mensajes de depuración
 */
const char *BuzzerAlarm_Nombre(buzzer_alarm_t patron);

/** @} */
#endif /* BUZZER_ALARM_H */
