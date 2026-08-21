#ifndef LCD_ITSE0803_H
#define LCD_ITSE0803_H

/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Drivers_Devices Drivers devices
 ** @{ */
/** \addtogroup LCD_ITSE0803 LCD ITSE0803
 ** @{
 */

/** \brief Driver for using the 3 digits numeric display, ported from ESP-EDU to a
 * generic ESP32-C3 devkit.
 *
 * @note Needs 7 dedicated GPIOs. GPIO_22/GPIO_23 (used on the original ESP32-C6 board)
 * don't exist on the ESP32-C3, so BCD3/BCD4 were remapped to GPIO_2/GPIO_3 below - which
 * overlap with the suggested ADC use of those pins in gpio_mcu.h. Remap as needed for
 * your own wiring; this display isn't required for I2C-based projects.
 *
 * |   Display      |	Pin (C3)	|
 * |:--------------:|:-------------:|
 * | 	Vcc 	    |	5V      	|
 * | 	BCD1		| 	GPIO_20		|
 * | 	BCD2	 	| 	GPIO_21		|
 * | 	BCD3	 	| 	GPIO_2		|
 * | 	BCD4	 	| 	GPIO_3		|
 * | 	SEL1	 	| 	GPIO_19		|
 * | 	SEL2	 	| 	GPIO_18		|
 * | 	SEL3	 	| 	GPIO_9		|
 * | 	Gnd 	    | 	GND     	|
 *
 * @author Albano Peñalva
 *
 * @section changelog
 *
 * |   Date	    | Description                                    						|
 * |:----------:|:----------------------------------------------------------------------|
 * | 23/10/2023 | Document creation		                         						|
 * | 21/08/2026 | Ported to ESP32-C3		                         						|
 *
 **/

/*==================[inclusions]=============================================*/
#include <stdint.h>
#include <stdbool.h>
/*==================[macros]=================================================*/

/*==================[typedef]================================================*/

/*==================[external data declaration]==============================*/

/*==================[external functions declaration]=========================*/
/**
 * @brief ESP-EDU LCD Module initialization.
 * 
 * @return true 
 */
bool LcdItsE0803Init(void);

/** @brief Function to display value a LCD Module for EDU-CIAA.
 *
 * @param[in] value to show 0..999
 *
 * @return false if an error occurs (out of scale), in other case returns true
 */

/**
 * @brief Display a value in LCD Module.
 * 
 * @param value Number to display (o to 999)
 * @return true if value < 999
 * @return false if value > 999
 */
bool LcdItsE0803Write(uint16_t value);

/**
 * @brief Read value displayed in LCD.
 * 
 * @return uint16_t value displayed.
 */
uint16_t LcdItsE0803Read(void);

/**
 * @brief Turn off display.
 * 
 */
void LcdItsE0803Off(void);

/**
 * @brief ESP-EDU LCD Module initialization.
 * 
 * @return true 
 */
bool LcdItsE0803DeInit(void);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif /* #ifndef DISPLAYITS_E0803_H */

/*==================[end of file]============================================*/
