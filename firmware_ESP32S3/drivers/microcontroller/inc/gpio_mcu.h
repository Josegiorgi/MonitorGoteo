#ifndef GPIO_MCU_H
#define GPIO_MCU_H
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Drivers_Microcontroller Drivers microcontroller
 ** @{ */
/** \addtogroup GIOP GPIO
 ** @{ */

/** \brief GPIO driver for a generic ESP32-S3 DevKit (Super Mini form factor).
 *
 * This driver provide functions to configure and handle the ESP32-S3 General
 * Purpose Input-Outputs.
 *
 * @note Ported from the ESP32-C3 driver. The ESP32-S3 has 49 physical GPIOs
 * (GPIO0-GPIO48), but GPIO22-GPIO25 don't exist on the chip at all, and
 * GPIO26-GPIO32 are always wired internally to the flash/PSRAM (embedded in
 * the same package on the Super Mini's ESP32-S3FH4R2), so neither range is
 * broken out on the board. The pin suggestions below (switch/LED/I2C/SPI/etc.)
 * are just a convenient default for wiring your own project, not a fixed
 * board layout - all of them were checked against the Super Mini's official
 * header pinout (GP1-GP13, GP14-GP18/GP21, GP33-GP42/GP45-GP48 broken out;
 * GP0=BOOT, GP19/GP20=native USB, GP43/GP44=UART0 console not header pins).
 * Low-cost clones can still vary, so re-check yours if something doesn't
 * match.
 *
 * @note GPIO_4 and GPIO_9 suggested for switches/pushbuttons.
 *
 * @note GPIO_8, GPIO_7 and GPIO_15 suggested for LEDs.
 *
 * @note GPIO_5 and GPIO_6 suggested for I2C (SDA/SCL) - see i2c_mcu.h. These
 * match the Super Mini's silkscreen-labeled I2C pins.
 *
 * @note GPIO_10, GPIO_11 and GPIO_12 suggested for SPI (CS/MOSI/SCLK) - see
 * spi_mcu.h. These match the chip's IO_MUX default FSPI pins, for best signal
 * integrity at high clock speeds.
 *
 * @note GPIO_0 and GPIO_3 are strapping pins (boot mode selection) - GPIO_0 is
 * usually wired to the onboard BOOT button. Avoid driving them externally
 * during reset.
 *
 * @note GPIO_19 and GPIO_20 are the chip's native USB pins (D-/D+, wired to
 * the board's USB-C connector) - avoid using them for anything else.
 *
 * @note GPIO_39-GPIO_42 are the default JTAG pins (MTCK/MTDO/MTDI/MTMS) -
 * only relevant if you wire an external JTAG probe (the Super Mini normally
 * debugs over its built-in USB-JTAG on GPIO_19/GPIO_20 instead).
 *
 * @note GPIO_43 and GPIO_44 are the default UART0 TX/RX console pins.
 *
 * @note GPIO_45 and GPIO_46 are strapping pins (VDD_SPI voltage / ROM message
 * printing) - avoid driving them externally during reset.
 *
 * @note GPIO_48 drives the Super Mini's onboard addressable WS2812 RGB LED
 * (confirmed on the board's official pinout) - it's also broken out on the
 * header, so it can be reused to chain external NeoPixels (see
 * neopixel_stripe.h).
 *
 * @author Albano Peñalva
 *
 * @section changelog
 *
 * |   Date	    | Description                                    						|
 * |:----------:|:----------------------------------------------------------------------|
 * | 23/10/2023 | Document creation		                         						|
 * | 21/08/2026 | Ported to ESP32-C3		                         						|
 * | 01/09/2026 | Ported to ESP32-S3 (Super Mini)		                         		|
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
 * @brief ESP32-S3 available GPIOs (GPIO22-GPIO25 don't exist on the chip,
 * GPIO26-GPIO32 are internally reserved for the flash/PSRAM)
 *
 */
typedef enum gpio_list{
	GPIO_0=0, 	/**< GPIO0 - strapping pin, usually the onboard BOOT button */
	GPIO_1, 	/**< GPIO1 - suggested for HC-SR04 Echo */
	GPIO_2, 	/**< GPIO2 - suggested for HC-SR04 Trigger */
	GPIO_3, 	/**< GPIO3 - strapping pin (JTAG source select) */
	GPIO_4, 	/**< GPIO4 - suggested for SWITCH_1 */
	GPIO_5, 	/**< GPIO5 - suggested for I2C SDA (Super Mini silkscreen) */
	GPIO_6, 	/**< GPIO6 - suggested for I2C SCL (Super Mini silkscreen) */
	GPIO_7, 	/**< GPIO7 - suggested for LED_2 */
	GPIO_8, 	/**< GPIO8 - suggested for LED_1 */
	GPIO_9, 	/**< GPIO9 - suggested for SWITCH_2 */
	GPIO_10, 	/**< GPIO10 - suggested for SPI CS (IO_MUX default FSPICS0) */
	GPIO_11, 	/**< GPIO11 - suggested for SPI MOSI (IO_MUX default FSPID) */
	GPIO_12, 	/**< GPIO12 - suggested for SPI SCLK (IO_MUX default FSPICLK) */
	GPIO_13, 	/**< GPIO13 - suggested for LCD_ITS_E0803 BCD1 */
	GPIO_14, 	/**< GPIO14 - suggested for LCD_ITS_E0803 BCD2 */
	GPIO_15, 	/**< GPIO15 - suggested for LED_3 */
	GPIO_16, 	/**< GPIO16 - suggested for LCD_ITS_E0803 BCD3 */
	GPIO_17, 	/**< GPIO17 - suggested for UART_CONNECTOR TX */
	GPIO_18, 	/**< GPIO18 - suggested for UART_CONNECTOR RX */
	GPIO_19, 	/**< GPIO19 - native USB D- */
	GPIO_20, 	/**< GPIO20 - native USB D+ */
	GPIO_21,	/**< GPIO21 - suggested for LCD_ITS_E0803 BCD4 */
	GPIO_22, 	/**< not available (pin doesn't exist on ESP32-S3) */
	GPIO_23, 	/**< not available (pin doesn't exist on ESP32-S3) */
	GPIO_24, 	/**< not available (pin doesn't exist on ESP32-S3) */
	GPIO_25, 	/**< not available (pin doesn't exist on ESP32-S3) */
	GPIO_26, 	/**< not available (internal flash/PSRAM) */
	GPIO_27, 	/**< not available (internal flash/PSRAM) */
	GPIO_28, 	/**< not available (internal flash/PSRAM) */
	GPIO_29, 	/**< not available (internal flash/PSRAM) */
	GPIO_30, 	/**< not available (internal flash/PSRAM) */
	GPIO_31, 	/**< not available (internal flash/PSRAM) */
	GPIO_32, 	/**< not available (internal flash/PSRAM) */
	GPIO_33, 	/**< GPIO33 - suggested for LCD_ITS_E0803 SEL1 */
	GPIO_34, 	/**< GPIO34 - suggested for LCD_ITS_E0803 SEL2 */
	GPIO_35, 	/**< GPIO35 - suggested for LCD_ITS_E0803 SEL3 */
	GPIO_36, 	/**< GPIO36 - suggested for L293 EN_1_2 */
	GPIO_37, 	/**< GPIO37 - suggested for L293 1A */
	GPIO_38, 	/**< GPIO38 - suggested for L293 2A */
	GPIO_39, 	/**< GPIO39 - suggested for L293 EN_3_4 (default JTAG MTCK) */
	GPIO_40, 	/**< GPIO40 - suggested for L293 3A (default JTAG MTDO) */
	GPIO_41, 	/**< GPIO41 - suggested for L293 4A (default JTAG MTDI) */
	GPIO_42,	/**< GPIO42 - free (default JTAG MTMS) */
	GPIO_43,	/**< GPIO43 - default UART0 TX (console) */
	GPIO_44,	/**< GPIO44 - default UART0 RX (console) */
	GPIO_45,	/**< GPIO45 - strapping pin (VDD_SPI voltage selection) */
	GPIO_46,	/**< GPIO46 - strapping pin (ROM message printing) */
	GPIO_47,	/**< GPIO47 - free */
	GPIO_48,	/**< GPIO48 - suggested for the onboard NeoPixel (see neopixel_stripe.h) */
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
