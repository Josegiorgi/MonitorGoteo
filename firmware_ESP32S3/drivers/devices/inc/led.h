#ifndef LED_H
#define LED_H
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Drivers_Devices Drivers devices
 ** @{ */
/** \addtogroup LED Led
 ** @{ */

/** \brief LED driver, ported to a generic ESP32-S3 devkit.
 *
 * @note 3 LEDs supported: LED_1 (default GPIO_8), LED_2 (default GPIO_7), LED_3
 * (default GPIO_15). Wire them to whichever pins suit your project - see gpio_mcu.h.
 *
 * @author Albano Peñalva
 *
 * @section changelog
 *
 * |   Date	    | Description                                    						|
 * |:----------:|:----------------------------------------------------------------------|
 * | 23/10/2023 | Document creation		                         						|
 * | 21/08/2026 | Ported to ESP32-C3		                         						|
 * | 01/09/2026 | Ported to ESP32-S3		                         						|
 *
 **/

/*==================[inclusions]=============================================*/
#include <stdbool.h>
#include <stdint.h>
/*==================[macros]=================================================*/
/**
 * @brief List of available LEDs in ESP-EDU board.
 */
typedef enum LEDs {
    LED_3 = (1 << 0), /**< Default: GPIO_15 */
    LED_2 = (1 << 1), /**< Default: GPIO_7 */
    LED_1 = (1 << 2), /**< Default: GPIO_8 */
} led_t;
/*==================[typedef]================================================*/

/*==================[external data declaration]==============================*/

/*==================[external functions declaration]=========================*/
/**
 * @brief ESP-EDU LEDs initialization
 * 
 * @return uint8_t 
 */
uint8_t LedsInit(void);

/**
 * @brief Turn on a specific LED
 * 
 * @param led LED number
 * @return uint8_t false: if invalid LED number
 */
uint8_t LedOn(led_t led);

/**
 * @brief Turn off a specific LED
 * 
 * @param led LED number
 * @return uint8_t false: if invalid LED number 
 */
uint8_t LedOff(led_t led);

/**
 * @brief Invert a specific LED state
 * 
 * @param led LED number
 * @return uint8_t false: if invalid LED number 
 */
uint8_t LedToggle(led_t led);

/**
 * @brief Turn off all LEDs
 * 
 * @return uint8_t 
 */
uint8_t LedsOffAll(void);

/**
 * @brief Turn on or off leds from a mask.
 * 
 * @param mask (b0: LED_3, b1: LED_2, b2: LED_1)
 * @return uint8_t 
 */
uint8_t LedsMask(uint8_t mask);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */

#endif /* #ifndef LED_H */

/*==================[end of file]============================================*/

