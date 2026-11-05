#ifndef BATERIA_H
#define BATERIA_H
/** \addtogroup Bateria Batería
 ** @{ */

/** \brief Carga de la batería, medida con un divisor resistivo y un canal del ADC.
 *
 * La batería (Li-ion o Li-Po de 1 celda, 4.2 V a plena carga) no puede ir directo al ADC, así que
 * entra por un divisor de dos resistencias: el ADC lee solo la parte de la tensión que cae en
 * BATERIA_R_ABAJO_OHM, y acá se la multiplica por (R_ARRIBA + R_ABAJO) / R_ABAJO para volver a la
 * tensión real. Con 10 kΩ + 10 kΩ el ADC ve la mitad: 2.1 V con la batería llena, dentro del rango
 * donde la calibración del ESP32-C3 es precisa (hasta ~2.5 V con 12 dB de atenuación).
 *
 * Se toma una lectura cada BATERIA_PERIODO_MUESTRA_MS y se promedian las últimas BATERIA_MUESTRAS:
 * una lectura aislada del ADC tiene ruido, y la batería se mueve despacio. Mientras suena el
 * buzzer no se mide (omitir = true), porque sus picos de consumo hacen caer la tensión un instante.
 *
 * El porcentaje sale de una curva de descarga típica de Li-Ion/Li-Po **sin carga** (tabla en
 * bateria.c). Es provisoria: se puede reemplazar por la curva medida del equipo.
 *
 * @note Una sola lectura por llamada, corta, para no demorar el muestreo de los fotodiodos que
 * comparte el ADC1.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 05/11/2026 | Document creation                              |
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include "analog_io_mcu.h"

/*==================[macros]=================================================*/
#define BATERIA_CANAL_ADC           CH4      /*!< Canal del ADC donde está el punto medio del divisor (GPIO_4, ADC1_CH4). CH2 y CH3 son de los BPW34 */
#define BATERIA_R_ARRIBA_OHM        10000    /*!< Resistencia entre el positivo de la batería y el pin del ADC */
#define BATERIA_R_ABAJO_OHM         10000    /*!< Resistencia entre el pin del ADC y GND */
#define BATERIA_AJUSTE_PERMIL       1000     /*!< Ajuste fino en milésimas: 1000 = sin ajuste. Para corregir la tolerancia de las resistencias: con un tester, ajuste = 1000 x (tensión del tester) / (tensión que informa el equipo) */
#define BATERIA_MUESTRAS            16       /*!< Lecturas que se promedian */
#define BATERIA_PERIODO_MUESTRA_MS  100      /*!< Tiempo mínimo entre una lectura y la siguiente (16 lecturas = ~1.6 s de promedio) */
#define BATERIA_MV_MINIMO_VALIDO    2500     /*!< Por debajo de esta tensión no hay una batería de Li-ion conectada: la medición no vale */

/*==================[external functions declaration]=========================*/

/** @fn bool Bateria_Init(void)
 * @brief Configura el canal del ADC de la batería.
 * @return true si quedó listo
 */
bool Bateria_Init(void);

/** @fn void Bateria_Muestrear(bool omitir)
 * @brief Toma una lectura si pasaron BATERIA_PERIODO_MUESTRA_MS desde la anterior. Llamar seguido
 * (por ejemplo en cada vuelta del lazo principal): ella misma se espera.
 * @param omitir true para no medir en esta llamada (por ejemplo mientras suena el buzzer)
 */
void Bateria_Muestrear(bool omitir);

/** @fn bool Bateria_Valida(void)
 * @brief true si hay suficientes lecturas y la tensión es la de una batería conectada
 */
bool Bateria_Valida(void);

/** @fn uint16_t Bateria_PinMv(void)
 * @brief Tensión promedio en el pin del ADC (la mitad de la batería con 10k + 10k), en mV
 */
uint16_t Bateria_PinMv(void);

/** @fn uint16_t Bateria_Mv(void)
 * @brief Tensión de la batería (pin x divisor x ajuste), en mV
 */
uint16_t Bateria_Mv(void);

/** @fn uint8_t Bateria_Pct(void)
 * @brief Carga de la batería, de 0 a 100 %, según la tensión promedio
 */
uint8_t Bateria_Pct(void);

/** @fn uint8_t Bateria_PctDeMv(uint16_t mv)
 * @brief Carga que corresponde a una tensión de batería (curva de descarga sin carga)
 */
uint8_t Bateria_PctDeMv(uint16_t mv);

/** @} */
#endif /* BATERIA_H */
