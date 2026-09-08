#ifndef _FDC1004_H_
#define _FDC1004_H_
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Drivers_Devices Drivers devices
 ** @{ */
/** \addtogroup FDC1004 FDC1004
 ** @{ */

/** \brief Driver para el conversor capacitancia-a-digital FDC1004 (Texas Instruments),
 *  usado como sensor de gota del monitor de goteo. Se comunica con la placa por I2C.
 *
 * @note Direccion I2C fija: 0x50 (no configurable, sin pin ADDR).
 *
 * @note Este driver solo cubre mediciones single-ended (CINn vs GND), que es el modo
 * correcto para las dos placas semicirculares que rodean la camara de goteo: la placa
 * activa va al pin de canal del modulo (CIN1) y la placa de referencia va al pin GND del
 * modulo (no a otro canal CINn) - ver documentacion/hardware.md para el porque de esa
 * eleccion frente al modo diferencial.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:------------------------------------------------|
 * | 04/09/2026 | Document creation		                         |
 *
 **/

/*==================[inclusions]=============================================*/
#include <stdint.h>
#include <stdbool.h>
#include "i2c_mcu.h"

/*==================[macros]=================================================*/
#define FDC1004_I2C_ADDRESS    0x50    /*!< Direccion I2C fija del FDC1004 */

/*==================[typedef]================================================*/
/**
 * @brief Canal de entrada capacitiva (CIN1 a CIN4)
 */
typedef enum {
    FDC1004_CIN1 = 0,
    FDC1004_CIN2 = 1,
    FDC1004_CIN3 = 2,
    FDC1004_CIN4 = 3,
} fdc1004_channel_t;

/**
 * @brief Tasa de muestreo (a mayor tasa, menor resolucion efectiva)
 */
typedef enum {
    FDC1004_RATE_100SPS = 0x01,
    FDC1004_RATE_200SPS = 0x02,
    FDC1004_RATE_400SPS = 0x03,
} fdc1004_rate_t;

/*==================[external functions declaration]=========================*/

/** @fn bool FDC1004_Init(void)
 * @brief Verifica comunicacion con el FDC1004 leyendo su registro de Device ID.
 * @note Requiere haber llamado antes a I2C_initialize().
 * @return true si el dispositivo respondio con el Device ID esperado (0x1004)
 */
bool FDC1004_Init(void);

/** @fn bool FDC1004_ConfigureSingleEnded(uint8_t measurement, fdc1004_channel_t channel, uint8_t capdac)
 * @brief Configura una medicion single-ended (canal vs GND).
 * @param measurement Numero de medicion a configurar (1 a 4, cada una es independiente)
 * @param channel Canal de entrada conectado a la placa activa
 * @param capdac Offset de capacitancia a descontar, en pasos de 3.125 pF (0 a 31, 0 = sin
 * offset). Dejar en 0 salvo que la capacitancia base placa-placa supere el rango de +-15 pF.
 * @return true si la escritura por I2C fue exitosa
 */
bool FDC1004_ConfigureSingleEnded(uint8_t measurement, fdc1004_channel_t channel, uint8_t capdac);

/** @fn bool FDC1004_StartMeasurement(uint8_t measurement, fdc1004_rate_t rate, bool repeat)
 * @brief Dispara una medicion ya configurada con FDC1004_ConfigureSingleEnded().
 * @param measurement Numero de medicion a disparar (1 a 4)
 * @param rate Tasa de muestreo
 * @param repeat true = medicion repetida en forma continua, false = una sola conversion
 * @return true si la escritura por I2C fue exitosa
 */
bool FDC1004_StartMeasurement(uint8_t measurement, fdc1004_rate_t rate, bool repeat);

/** @fn bool FDC1004_DataReady(uint8_t measurement)
 * @brief Consulta si una medicion disparada ya tiene resultado listo para leer.
 * @param measurement Numero de medicion a consultar (1 a 4)
 * @return true si el resultado esta listo
 */
bool FDC1004_DataReady(uint8_t measurement);

/** @fn bool FDC1004_ReadCapacitance(uint8_t measurement, float *capacitance_pF)
 * @brief Lee el resultado de una medicion. La lectura limpia automaticamente el flag de
 * "dato listo" (comportamiento del propio chip, ver datasheet).
 * @param measurement Numero de medicion a leer (1 a 4)
 * @param capacitance_pF Capacitancia medida, en pF, relativa al offset de CAPDAC configurado
 * @return true si la lectura por I2C fue exitosa
 */
bool FDC1004_ReadCapacitance(uint8_t measurement, float *capacitance_pF);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif /* #ifndef _FDC1004_H_ */

/*==================[end of file]============================================*/
