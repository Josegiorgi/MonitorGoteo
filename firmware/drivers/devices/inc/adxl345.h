#ifndef _ADXL345_H_
#define _ADXL345_H_
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Drivers_Devices Drivers devices
 ** @{ */
/** \addtogroup ADXL345 ADXL345
 ** @{ */

/** \brief Driver del acelerómetro de 3 ejes ADXL345 (Analog Devices) por I2C, sobre i2c_mcu.
 *
 * El chip arranca en standby: ADXL345_Init() lo pasa a modo medición con ±2 g y resolución
 * completa (FULL_RES), donde la sensibilidad es de ~3.9 mg/LSB (256 LSB/g) y la aceleración se
 * devuelve ya convertida a g. Con el equipo quieto el eje que apunta hacia arriba da +1 g.
 *
 * @note Dirección I2C: 0x53 con el pin SDO a GND y 0x1D con SDO a VDD. El pin SDO no puede quedar
 * sin conectar (la dirección quedaría indefinida), por eso ADXL345_Init() prueba las dos y se
 * queda con la que responde con DEVID = 0xE5. El pin CS tiene que estar a VDD I/O para modo I2C.
 *
 * @note Usa i2c_mcu (driver I2C nuevo, i2c_master.h): no se puede combinar en la misma
 * aplicación con otro código que use el driver I2C legacy, como el HAL de u8g2 de la OLED.
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 19/10/2026 | Document creation                              |
 *
 **/

/*==================[inclusions]=============================================*/
#include <stdint.h>
#include <stdbool.h>
#include "i2c_mcu.h"

/*==================[macros]=================================================*/
#define ADXL345_ADDRESS_SDO_LOW    0x53    /*!< Dirección I2C con el pin SDO a GND */
#define ADXL345_ADDRESS_SDO_HIGH   0x1D    /*!< Dirección I2C con el pin SDO a VDD */
#define ADXL345_DEVID_VALUE        0xE5    /*!< Valor fijo del registro DEVID (0x00) */

#define ADXL345_REG_DEVID          0x00    /*!< ID del dispositivo (solo lectura) */
#define ADXL345_REG_BW_RATE        0x2C    /*!< Velocidad de salida de datos */
#define ADXL345_REG_POWER_CTL      0x2D    /*!< Control de energía (bit MEASURE) */
#define ADXL345_REG_DATA_FORMAT    0x31    /*!< Formato de los datos (rango, FULL_RES) */
#define ADXL345_REG_DATAX0         0x32    /*!< Primero de los 6 registros de datos: X0, X1, Y0, Y1, Z0, Z1 (LSB primero) */

#define ADXL345_BW_RATE_100HZ      0x0A    /*!< Velocidad de salida de 100 Hz, modo normal */
#define ADXL345_POWER_MEASURE      0x08    /*!< POWER_CTL: bit MEASURE (modo medición) */
#define ADXL345_FORMAT_2G_FULLRES  0x08    /*!< DATA_FORMAT: FULL_RES = 1 y rango ±2 g */
#define ADXL345_LSB_PER_G          256.0f  /*!< Sensibilidad en ±2 g (3.9 mg/LSB) */

/*==================[external functions declaration]=========================*/

/** @fn bool ADXL345_Init(void)
 * @brief Busca el chip en 0x53 y 0x1D, verifica DEVID = 0xE5 y lo configura (±2 g, FULL_RES,
 * 100 Hz, modo medición). Requiere I2C_initialize() previo.
 * @return true si el chip respondió y quedó configurado
 */
bool ADXL345_Init(void);

/** @fn uint8_t ADXL345_GetAddress(void)
 * @brief Dirección I2C con la que respondió el chip en ADXL345_Init() (0 si no respondió)
 */
uint8_t ADXL345_GetAddress(void);

/** @fn bool ADXL345_ReadRaw(int16_t *x, int16_t *y, int16_t *z)
 * @brief Lee los tres ejes en cuentas (lectura en ráfaga de los 6 bytes, como recomienda la hoja de datos)
 * @return true si la lectura fue correcta
 */
bool ADXL345_ReadRaw(int16_t *x, int16_t *y, int16_t *z);

/** @fn bool ADXL345_ReadAccel(float *ax, float *ay, float *az)
 * @brief Lee los tres ejes convertidos a g
 * @return true si la lectura fue correcta
 */
bool ADXL345_ReadAccel(float *ax, float *ay, float *az);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif /* #ifndef _ADXL345_H_ */
