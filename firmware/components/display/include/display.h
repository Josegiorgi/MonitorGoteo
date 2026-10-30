#ifndef DISPLAY_H
#define DISPLAY_H
/** \addtogroup Display Display
 ** @{ */

/** \brief Wrapper fino sobre u8g2 para la OLED de 128x64 por I2C (SSD1306, SH1106 o SSD1315).
 *
 * Envía los bytes de la OLED por i2c_mcu, el mismo driver I2C que usa el ADXL345, así los dos
 * conviven en el mismo bus. No usa el HAL de u8g2 (u8g2_hal), que usa el driver I2C viejo y
 * no se puede combinar con i2c_mcu en la misma aplicación.
 *
 * El controlador y la dirección I2C se eligen por menuconfig (Display (OLED I2C 128x64)).
 * El bus lo inicia el programa con I2C_initialize() antes de Display_Init(). Para dibujar se
 * usa la API normal de u8g2 con el handle de Display_Handle().
 *
 * @note I2C: SCL = GPIO_7, SDA = GPIO_21 (los define i2c_mcu.h).
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 29/10/2026 | Document creation                              |
 *
 */

#include <stdbool.h>
#include "u8g2.h"

/*==================[external functions declaration]=========================*/

/** @fn bool Display_Init(void)
 * @brief Configura u8g2 con el controlador elegido, enciende la OLED y la deja en blanco. Requiere
 * I2C_initialize() previo.
 * @return true si la OLED respondió en el bus
 */
bool Display_Init(void);

/** @fn u8g2_t *Display_Handle(void)
 * @brief Handle de u8g2 para dibujar con la API de u8g2
 */
u8g2_t *Display_Handle(void);

/** @fn void Display_Clear(void)
 * @brief Borra el buffer (no se ve hasta Display_Flush)
 */
void Display_Clear(void);

/** @fn void Display_Flush(void)
 * @brief Envía el buffer a la OLED
 */
void Display_Flush(void);

/** @fn void Display_PowerSave(bool apagada)
 * @brief Apaga o enciende la OLED (modo de bajo consumo)
 */
void Display_PowerSave(bool apagada);

/** @fn void Display_Printf(int x, int y, const char *fmt, ...)
 * @brief Escribe texto con la fuente actual (por defecto 6x10) en el buffer. y es la línea de base.
 */
void Display_Printf(int x, int y, const char *fmt, ...);

/** @} */
#endif /* DISPLAY_H */
