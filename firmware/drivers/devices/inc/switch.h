#ifndef SWITCH_H
#define SWITCH_H
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Drivers_Devices Drivers devices
 ** @{ */
/** \addtogroup Switch
 ** @{ */

/** \brief Switches driver, ported to a generic ESP32-C3 devkit.
 *
 * @note 2 switches supported: SWITCH_1 (default GPIO_4), SWITCH_2 (default GPIO_21).
 * Wire them to whichever pins suit your project - see gpio_mcu.h.
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
#include <stdbool.h>
#include <stdint.h>
/*==================[macros]=================================================*/

/*==================[typedef]================================================*/
typedef enum switches {
    SWITCH_1 = (1 << 0),  /**< Default: GPIO_4 */
    SWITCH_2 = (1 << 1),  /**< Default: GPIO_21 */
} switch_t;
/*==================[external data declaration]==============================*/

/*==================[external functions declaration]=========================*/
/**
 * @brief ESP-EDU switches initialization
 * 
 * @return int8_t 
 */
int8_t SwitchesInit(void);

/**
 * @brief Read all the switches state.
 * 
 * @return int8_t 0 if no key pressed, SWITCH_1, SWITCH_2 or (SWITCH_1 | SWITCH_2) in other case.
 */
int8_t SwitchesRead(void);

/**
 * @brief Enables the interruption of a particular key and assigns a callback function.
 * 
 * @param tec Selected switch
 * @param ptrIntFunc Pointer to callback function
 * @param args Pointer to callback function parameters
 */
void SwitchActivInt(switch_t tec, void *ptrIntFunc, void *args);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif

/*==================[end of file]============================================*/
