#ifndef GPIO_MCU_H
#define GPIO_MCU_H
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Drivers_Microcontroller Drivers microcontroller
 ** @{ */
/** \addtogroup GIOP GPIO
 ** @{ */

/** \brief GPIO driver for a generic ESP32-C3 DevKit.
 *
 * This driver provide functions to configure and handle the ESP32-C3 General
 * Purpose Input-Outputs.
 *
 * @note Ported from the original ESP-EDU (ESP32-C6) driver. The ESP32-C3 only has 22
 * physical GPIOs (GPIO0-GPIO21), and GPIO11-GPIO17 are internally reserved for the SPI
 * flash on every module/devkit, so they are not usable (same as GPIO_14 already was on
 * the original board). The pin suggestions below (switch/LED/I2C) are just a convenient
 * default for wiring your own project on a generic devkit, not a fixed board layout.
 *
 * @note GPIO_4 and GPIO_21 suggested for switches/pushbuttons.
 *
 * @note GPIO_5, GPIO_10 and GPIO_20 suggested for LEDs.
 *
 * @note GPIO_6 and GPIO_7 suggested for I2C (SDA/SCL) - see i2c_mcu.h.
 *
 * @note GPIO_2, GPIO_8 and GPIO_9 are strapping pins (boot mode selection): avoid driving
 * them externally during reset.
 *
 * @note GPIO_18 and GPIO_19 are shared between UART_CONNECTOR and the SPI bus (see
 * uart_mcu.h / spi_mcu.h), and are also the devkit's native USB pins (USB-Serial-JTAG).
 * GPIO_20 and GPIO_21 may be the default UART0 console pins on some devkit variants -
 * check your board before wiring something permanent there.
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
/**
 * @brief GPIO direction (input or output).
 * 
 */
typedef enum {
	GPIO_INPUT = 0, 	/**< Input with pull-up resistor */
	GPIO_OUTPUT			/**< Output */
	} io_t;

/**
 * @brief ESP32-C3 available GPIOs (GPIO11-GPIO17 don't exist on any devkit, reserved for
 * the internal SPI flash)
 *
 */
typedef enum gpio_list{
	GPIO_0=0, 	/**< GPIO0 - shared with ADC CH0 and DAC */
	GPIO_1, 	/**< GPIO1 - shared with ADC CH1 */
	GPIO_2, 	/**< GPIO2 - shared with ADC CH2 (strapping pin) */
	GPIO_3, 	/**< GPIO3 - shared with ADC CH3 */
	GPIO_4, 	/**< GPIO4 - suggested for SWITCH_1 */
	GPIO_5, 	/**< GPIO5 - suggested for LED_3 */
	GPIO_6, 	/**< GPIO6 - shared with I2C SDA */
	GPIO_7, 	/**< GPIO7 - shared with I2C SCL */
	GPIO_8, 	/**< GPIO8 - shared with NeoPixel (strapping pin) */
	GPIO_9, 	/**< GPIO9 - shared with SPI CS0 (strapping pin) */
	GPIO_10, 	/**< GPIO10 - suggested for LED_2 */
	GPIO_11, 	/**< not available (internal flash) */
	GPIO_12, 	/**< not available (internal flash) */
	GPIO_13, 	/**< not available (internal flash) */
	GPIO_14, 	/**< not available (internal flash) */
	GPIO_15, 	/**< not available (internal flash) */
	GPIO_16, 	/**< not available (internal flash) */
	GPIO_17, 	/**< not available (internal flash) */
	GPIO_18, 	/**< GPIO18 - shared with UART_CONNECTOR TX and SPI SCLK (also USB D-) */
	GPIO_19, 	/**< GPIO19 - shared with UART_CONNECTOR RX and SPI MOSI (also USB D+) */
	GPIO_20, 	/**< GPIO20 - suggested for LED_1 (also default UART0 RX on some devkits) */
	GPIO_21,	/**< GPIO21 - suggested for SWITCH_2 (also default UART0 TX on some devkits) */
} gpio_t;

/*==================[internal data declaration]==============================*/

/*==================[internal functions declaration]=========================*/
/**
 * @brief GPIO initialization
 * 
 * @param pin GPIO number
 * @param io GPIO direction
 */
void GPIOInit(gpio_t pin, io_t io);

/**
 * @brief Change GPIO state to high
 * 
 * @param pin GPIO number
 */
void GPIOOn(gpio_t pin);

/**
 * @brief Change GPIO state to low
 * 
 * @param pin GPIO number
 */
void GPIOOff(gpio_t pin);

/**
 * @brief Change GPIO state
 * 
 * @param pin GPIO number
 * @param state GPIO state (true: high - false: low)
 */
void GPIOState(gpio_t pin, bool state);

/**
 * @brief Invert GPIO state
 * 
 * @param pin GPIO number
 */
void GPIOToggle(gpio_t pin);

/**
 * @brief Reads GPIO state
 * 
 * @param pin 
 * @return true GPIO input high
 * @return false GPIO input low
 */
bool GPIORead(gpio_t pin);

/**
 * @brief Configure GPIO input interruption
 * 
 * @param pin GPIO number
 * @param ptr_int_func Pointer to callback function
 * @param edge true: positive edge - false: negative edge
 * @param args 
 */
void GPIOActivInt(gpio_t pin, void *ptr_int_func, bool edge, void *args);

/**
 * @brief Configure an input glitch filter to a GPIO
 * 
 * @note You can add filters to up to 8 GPIO
 * 
 * @param pin GPIO number
 * @param filter filter number
 */
void GPIOInputFilter(gpio_t pin);

/**
 * @brief GPIO de-initialization
 * 
 */
void GPIODeinit(void);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif /* INC_GPIO_H_ */

/*==================[end of file]============================================*/
