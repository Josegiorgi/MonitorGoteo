#ifndef TOUCH_MCU_H
#define TOUCH_MCU_H
/** \addtogroup Drivers_Programable Drivers Programable
 ** @{ */
/** \addtogroup Drivers_Microcontroller Drivers microcontroller
 ** @{ */
/** \addtogroup Touch Touch
 ** @{ */

/** \brief Capacitive touch sensor driver for a generic ESP32-S3 DevKit (Super Mini form
 * factor).
 *
 * This driver provides functions to configure and read the microcontroller's built-in
 * capacitive touch sensor peripheral.
 *
 * @note The ESP32-S3 has 15 touch channels (T0-T14). T0 is an internal denoise-only
 * channel with no external GPIO; T1-T14 map 1:1 to GPIO1-GPIO14 (touch channel N is
 * always GPIO N, for N = 1..14). Those same GPIOs are also suggested defaults for other
 * devices in this driver set (SWITCH_1, HC-SR04, I2C, SPI, LCD_ITS_E0803) - don't use a
 * pin for touch sensing and another peripheral at the same time.
 *
 * @note Wire a touch pad as a copper/metal pad (or plain wire) connected directly to the
 * GPIO - no pull-up resistor or other external components needed.
 *
 * @note GPIO14 (TOUCH_CH_14) is hardwired by the chip as the waterproof "shield" channel -
 * see TouchShieldEnable(). If you use the shield, GPIO14 can't be used as a sensing
 * electrode or for anything else.
 *
 * @author Albano Peñalva
 *
 * @section changelog
 *
 * |   Date	    | Description                                    						|
 * |:----------:|:----------------------------------------------------------------------|
 * | 02/09/2026 | Document creation (ESP32-S3 Super Mini)		                       	|
 *
 **/

/*==================[inclusions]=============================================*/
#include <stdbool.h>
#include <stdint.h>
/*==================[macros]=================================================*/

/*==================[typedef]================================================*/
/**
 * @brief ESP32-S3 touch channels (T0 is internal only, not exposed here)
 */
typedef enum touch_ch{
	TOUCH_CH_1 = 1,		/**< Touch channel 1 - GPIO1 */
	TOUCH_CH_2,			/**< Touch channel 2 - GPIO2 */
	TOUCH_CH_3,			/**< Touch channel 3 - GPIO3 */
	TOUCH_CH_4,			/**< Touch channel 4 - GPIO4 */
	TOUCH_CH_5,			/**< Touch channel 5 - GPIO5 */
	TOUCH_CH_6,			/**< Touch channel 6 - GPIO6 */
	TOUCH_CH_7,			/**< Touch channel 7 - GPIO7 */
	TOUCH_CH_8,			/**< Touch channel 8 - GPIO8 */
	TOUCH_CH_9,			/**< Touch channel 9 - GPIO9 */
	TOUCH_CH_10,		/**< Touch channel 10 - GPIO10 */
	TOUCH_CH_11,		/**< Touch channel 11 - GPIO11 */
	TOUCH_CH_12,		/**< Touch channel 12 - GPIO12 */
	TOUCH_CH_13,		/**< Touch channel 13 - GPIO13 */
	TOUCH_CH_14,		/**< Touch channel 14 - GPIO14 */
} touch_ch_t;

/*==================[external data declaration]==============================*/

/*==================[external functions declaration]=========================*/
/**
 * @brief Touch sensor initialization
 *
 * @note Initializes the touch peripheral (only once, on the first call) and configures
 * and enables the given channel with the given detection threshold.
 *
 * @param channel Touch channel to configure
 * @param threshold Raw counter delta from the benchmark that triggers a "touched" state -
 * lower values make the pad more sensitive. Tune it by reading TouchReadRaw() with the
 * pad untouched and touched, and picking a value in between.
 * @return true on success
 */
bool TouchInit(touch_ch_t channel, uint32_t threshold);

/**
 * @brief Read the raw value of a touch channel
 *
 * @param channel Touch channel to read
 * @return uint32_t raw touch sensor counter value
 */
uint32_t TouchReadRaw(touch_ch_t channel);

/**
 * @brief Check if a touch channel is currently touched
 *
 * @param channel Touch channel to check
 * @return true if touched (raw value crossed the configured threshold)
 */
bool TouchDetect(touch_ch_t channel);

/**
 * @brief Configure a callback invoked when any enabled touch channel changes state
 *
 * @note The touch peripheral has a single, global interrupt (not one per channel): the
 * callback fires on a state change on any channel initialized with TouchInit(). Call
 * TouchDetect() from within the callback (or right after) to find out which channel(s)
 * changed.
 *
 * @param ptr_int_func Pointer to callback function
 * @param args Pointer to callback function parameters
 */
void TouchActivInt(void *ptr_int_func, void *args);

/**
 * @brief Enable the touch sensor waterproof shield
 *
 * @note Reduces false readings caused by a film of water/humidity sitting on top of the
 * sensing electrode, by driving a guard ring around it at the same potential. Requires at
 * least one channel already configured with TouchInit() (so the peripheral is
 * initialized).
 *
 * @note GPIO14 (Touch14) is used as the shield channel by the chip itself and can't be
 * changed - wire a guard ring/plane around your sensing electrode to GPIO14, and don't use
 * that pin for anything else while the shield is enabled.
 *
 * @param guard_channel Touch channel used as the guard pad, to additionally detect
 * large-area water coverage over the sensing electrode. Pass the same channel you're
 * sensing with if you don't need a separate guard electrode.
 * @param shield_level Shield channel drive capability (0 to 7, higher compensates more
 * parasitic capacitance - i.e. a bigger/farther shield plane - at the cost of more power).
 * Start low (0-2) and raise it only if readings look unstable with the shield wired.
 * @return true on success
 */
bool TouchShieldEnable(touch_ch_t guard_channel, uint8_t shield_level);

/**
 * @brief Disable the touch sensor waterproof shield
 *
 */
void TouchShieldDisable(void);

/**
 * @brief Touch sensor de-initialization
 *
 */
void TouchDeinit(void);

/** @} doxygen end group definition */
/** @} doxygen end group definition */
/** @} doxygen end group definition */
#endif /* TOUCH_MCU_H */

/*==================[end of file]============================================*/
