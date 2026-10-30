#ifndef OPTICAL_SENSOR_H
#define OPTICAL_SENSOR_H
/** \addtogroup Optical_Sensor Sensor óptico
 ** @{ */

/** \brief Muestreo y filtrado del sensor óptico de gotas (fotodiodos BPW34).
 *
 * Lee los canales del ADC con un período fijo (timer), le resta a cada uno su nivel base (medido
 * al arrancar, con el haz libre) y lo pasa por un pasabanda Butterworth de 4° orden (20-260 Hz).
 * Después de cada muestra llama a la función que registró OpticalSensor_Init(), con la lectura
 * cruda en mV y la señal filtrada de cada canal. Es el mismo procesamiento de MODE_FILTERED de
 * projects/pruebabpw34, que no se modificó.
 *
 * El sensor se elige por menuconfig ("Sensor optico"). Por ahora solo está migrado el BPW34.
 *
 * @note BPW34: salida del amplificador n°1 en GPIO_2 (ADC1_CH2) y n°2 en GPIO_3 (ADC1_CH3). El
 * LED infrarrojo va siempre encendido por hardware.
 *
 * @note La función registrada se ejecuta en la tarea de muestreo, una vez por muestra: tiene que
 * ser corta y no puede bloquear.
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

/*==================[macros]=================================================*/
#define OPTICAL_SENSOR_CHANNELS         2        /*!< Cantidad de fotodiodos (canales del ADC) */
#define OPTICAL_SENSOR_SAMPLE_PERIOD_US 1200     /*!< Período entre muestras (~833 Hz) */

/*==================[typedef]================================================*/
/**
 * @brief Función que recibe cada muestra
 * @param raw_mV Lectura cruda de cada canal, en mV
 * @param filtered_mV Señal filtrada de cada canal (centrada en 0), en mV
 */
typedef void (*optical_sensor_cb_t)(const uint16_t raw_mV[OPTICAL_SENSOR_CHANNELS],
                                    const float filtered_mV[OPTICAL_SENSOR_CHANNELS]);

/*==================[external functions declaration]=========================*/

/** @fn bool OpticalSensor_Init(optical_sensor_cb_t cb)
 * @brief Configura el ADC, arma los filtros, mide el nivel base y empieza a muestrear.
 * Con el haz libre (sin gota) al llamarla.
 * @return true si quedó muestreando
 */
bool OpticalSensor_Init(optical_sensor_cb_t cb);

/** @fn float OpticalSensor_BaseMv(int canal)
 * @brief Nivel base medido al arrancar en un canal, en mV (0 si el canal no existe)
 */
float OpticalSensor_BaseMv(int canal);

/** @} */
#endif /* OPTICAL_SENSOR_H */
