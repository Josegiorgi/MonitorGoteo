#ifndef INCLINACION_H
#define INCLINACION_H
/** \addtogroup Inclinacion Inclinación del equipo
 ** @{ */

/** \brief Ángulo del equipo respecto de la posición de referencia, medido con el ADXL345.
 *
 * Crea una tarea que lee el acelerómetro cada INCLINACION_PERIODO_MS, promedia unas lecturas para
 * filtrar vibraciones y guarda el ángulo. Es el mismo cálculo que usó MODE_ANGLE de
 * projects/pruebabpw34 para medir a qué ángulo se pierde la detección de gotas (ángulo del eje Z
 * respecto de la horizontal, menos el que da el equipo en su posición de referencia dentro del
 * gabinete), así que los ángulos de ese análisis valen acá.
 *
 * @note I2C: SCL = GPIO_7, SDA = GPIO_21, el mismo bus que la OLED. Requiere I2C_initialize()
 * antes de Inclinacion_Init(). Las lecturas son pocas y cortas para no dejar el bus ocupado.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 04/11/2026 | Document creation: sale de AngleTask de pruebabpw34 |
 *
 */

#include <stdbool.h>

/*==================[macros]=================================================*/
#define INCLINACION_OFFSET_DEG   11.1f  /*!< Ángulo del eje Z con el equipo en la posición de referencia (cero) dentro del gabinete: varía entre 10.6 y 11.7 en reposo, punto medio 11.1 (medido en prueba_adxl345). Se resta para que ahí el ángulo sea 0 */
#define INCLINACION_PERIODO_MS   250    /*!< Cada cuánto se actualiza el ángulo. Cambia mucho más lento que la señal de gotas */
#define INCLINACION_LECTURAS     6      /*!< Lecturas que se promedian por ángulo (el ADXL345 entrega una nueva cada 10 ms: ~60 ms, como el promedio de 32 lecturas cada 2 ms del análisis) */
#define INCLINACION_FALLAS_MAX   3      /*!< Lecturas fallidas seguidas tras las que el ángulo pasa a no válido */

/*==================[external functions declaration]=========================*/

/** @fn bool Inclinacion_Init(void)
 * @brief Busca el ADXL345 y crea la tarea de lectura. Se puede llamar de nuevo si falló (por
 * ejemplo en el autotest): si ya está andando devuelve true sin hacer nada.
 * @return true si el acelerómetro respondió y la tarea quedó creada
 */
bool Inclinacion_Init(void);

/** @fn bool Inclinacion_Valida(float *grados)
 * @brief Último ángulo medido, en grados con signo (positivo o negativo según hacia qué lado se inclina).
 * @param grados Si no es NULL, recibe el ángulo
 * @return false si el acelerómetro no inició o las últimas lecturas fallaron (el ángulo no es confiable)
 */
bool Inclinacion_Valida(float *grados);

/** @} */
#endif /* INCLINACION_H */
