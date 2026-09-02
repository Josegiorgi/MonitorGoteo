#ifndef _SSD1315_H_
#define _SSD1315_H_
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Drivers_Devices Drivers devices
 ** @{ */
/** \addtogroup SSD1315 SSD1315
 ** @{ */

/** \brief Driver para display OLED monocromo 128x64 basado en el controlador SSD1315
 *  (compatible a nivel de comandos con el SSD1306, p. ej. Grove OLED 0.96"). Se comunica
 *  con la placa EDU-ESP mediante I2C.
 *
 * @note Direccion I2C tipica: 0x3C (algunos modulos usan 0x3D).
 *
 * @section changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:------------------------------------------------|
 * | 25/08/2026 | Document creation		                         |
 *
 **/

/*==================[inclusions]=============================================*/
#include <stdint.h>
#include <stdbool.h>
#include "i2c_mcu.h"

/*==================[macros]=================================================*/
#define SSD1315_I2C_ADDRESS    0x3C    /*!< Direccion I2C por defecto (ADDR a GND) */
#define SSD1315_WIDTH          128     /*!< Ancho del display en pixeles */
#define SSD1315_HEIGHT         64      /*!< Alto del display en pixeles */

/*==================[typedef]================================================*/
/**
 * @brief Color de un pixel/trazo
 */
typedef enum {
    SSD1315_COLOR_BLACK = 0,   /*!< Pixel apagado */
    SSD1315_COLOR_WHITE = 1,   /*!< Pixel encendido */
} ssd1315_color_t;

/*==================[external functions declaration]=========================*/

/** @fn bool SSD1315_Init(uint8_t i2cAddress)
 * @brief Inicializa el display OLED (secuencia de encendido) y limpia el buffer interno.
 * @param i2cAddress Direccion I2C del modulo (usar SSD1315_I2C_ADDRESS por defecto)
 * @return true si el display respondio correctamente
 */
bool SSD1315_Init(uint8_t i2cAddress);

/** @fn void SSD1315_Clear(void)
 * @brief Limpia el buffer interno (no actualiza el display hasta llamar a SSD1315_UpdateScreen)
 */
void SSD1315_Clear(void);

/** @fn void SSD1315_UpdateScreen(void)
 * @brief Envia el contenido del buffer interno al display por I2C
 */
void SSD1315_UpdateScreen(void);

/** @fn void SSD1315_SetPixel(uint8_t x, uint8_t y, ssd1315_color_t color)
 * @brief Escribe un pixel en el buffer interno
 * @param x Coordenada horizontal (0 a SSD1315_WIDTH-1)
 * @param y Coordenada vertical (0 a SSD1315_HEIGHT-1)
 * @param color SSD1315_COLOR_BLACK o SSD1315_COLOR_WHITE
 */
void SSD1315_SetPixel(uint8_t x, uint8_t y, ssd1315_color_t color);

/** @fn void SSD1315_DrawChar(uint8_t x, uint8_t y, char c, ssd1315_color_t color)
 * @brief Dibuja un caracter (fuente 5x7) en el buffer interno
 * @param x Columna donde comienza el caracter
 * @param y Fila donde comienza el caracter
 * @param c Caracter ASCII imprimible (0x20 a 0x7E)
 * @param color Color del trazo
 */
void SSD1315_DrawChar(uint8_t x, uint8_t y, char c, ssd1315_color_t color);

/** @fn void SSD1315_DrawString(uint8_t x, uint8_t y, const char *str, ssd1315_color_t color)
 * @brief Dibuja una cadena de caracteres (fuente 5x7, avanza 6 px por caracter) en el buffer interno
 * @param x Columna donde comienza el texto
 * @param y Fila donde comienza el texto
 * @param str Cadena terminada en NULL
 * @param color Color del trazo
 */
void SSD1315_DrawString(uint8_t x, uint8_t y, const char *str, ssd1315_color_t color);

/** @fn void SSD1315_SetContrast(uint8_t contrast)
 * @brief Ajusta el contraste/brillo del display
 * @param contrast Valor de 0 a 255
 */
void SSD1315_SetContrast(uint8_t contrast);

/** @fn void SSD1315_DisplayOn(bool on)
 * @brief Enciende o apaga la salida del panel (el contenido del buffer se conserva)
 * @param on true = encendido, false = apagado
 */
void SSD1315_DisplayOn(bool on);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif /* #ifndef _SSD1315_H_ */

/*==================[end of file]============================================*/
