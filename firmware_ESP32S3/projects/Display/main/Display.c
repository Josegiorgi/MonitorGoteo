/*! @mainpage Display oled 0.96" SSD1315
 *
 * \section genDesc General Description
 *
 * Prueba de uso.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 12/09/2023 | Document creation		                         |
 *
 * @author Josefina giorgi (josefina.giorgi@ingenieriauner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "i2c_mcu.h"
#include "ssd1315.h"
#include "u8g2.h"
#include "u8g2_esp32_hal.h"
/*==================[macros and definitions]=================================*/
//#define CONFIG_BLINK_PERIOD 1000
/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/
// Opción Lopaka: pantalla generada con Lopaka (u8g2 - ESP-IDF)
// [BEGIN lopaka generated]
static const unsigned char image_Bluetooth_Idle_bits[] U8X8_PROGMEM = {0x30,0x00,0x30,0x00,0xf3,0x00,0xf3,0x00,0x3c,0x03,0x3c,0x03,0xf0,0x00,0xf0,0x00,0xf0,0x00,0xf0,0x00,0x3c,0x03,0x3c,0x03,0xf3,0x00,0xf3,0x00,0x30,0x00,0x30,0x00};
static const unsigned char image_Volup_bits[] U8X8_PROGMEM = {0xc0,0x30,0xc0,0x30,0xf0,0xc0,0xf0,0xc0,0xff,0xcc,0xff,0xcc,0xff,0xcc,0xff,0xcc,0xf0,0xc0,0xf0,0xc0,0xc0,0x30,0xc0,0x30};

void drawScreen_1(u8g2_t *u8g2) {
    u8g2_SetFontMode(u8g2, 1);
    u8g2_SetBitmapMode(u8g2, 1);
    // string 1
    u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
    u8g2_DrawStr(u8g2, 8, 15, "");
    // string 2
    u8g2_SetFont(u8g2, u8g2_font_timR10_tr);
    u8g2_DrawStr(u8g2, 39, 31, "120 ml/h");
    // line 3
    u8g2_DrawLine(u8g2, 1, 18, 127, 18);
    // string 4
    u8g2_DrawStr(u8g2, 40, 61, "01:30:08");
    // string 5
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, 27, 54, "");
    // Bluetooth_Idle
    u8g2_DrawXBMP(u8g2, 3, 0, 10, 16, image_Bluetooth_Idle_bits);
    // Volup
    u8g2_DrawXBMP(u8g2, 109, 3, 16, 12, image_Volup_bits);
    // string 8
    u8g2_SetFont(u8g2, u8g2_font_6x13_tr);
    u8g2_DrawStr(u8g2, 50, 14, "%Bat");
    // string 9
    u8g2_SetFont(u8g2, u8g2_font_timR10_tr);
    u8g2_DrawStr(u8g2, 34, 46, "Vol: 250ml");
}
// [END lopaka generated]
/*==================[external functions definition]==========================*/

void app_main(void){
    // Opción A: CLÁSICA (sin u8g2)
    //I2C_initialize(I2C_MASTER_FREQ_HZ);
    //SSD1315_Init(SSD1315_I2C_ADDRESS);
    //SSD1315_DrawString(0, 0, "HOLA", SSD1315_COLOR_WHITE);
    //SSD1315_UpdateScreen();

    // Opción u8g2
    u8g2_esp32_hal_t hal = U8G2_ESP32_HAL_DEFAULT;
    hal.bus.i2c.sda = 5;  // GPIO_5
    hal.bus.i2c.scl = 6;  // GPIO_6
    u8g2_esp32_hal_init(hal);

    u8g2_t u8g2;
    u8g2_Setup_ssd1315_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    u8x8_SetI2CAddress(&u8g2.u8x8, 0x78); // 0x3C corrido un bit a la izquierda

    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);

    // Opción Lopaka
    drawScreen_1(&u8g2);
    u8g2_SendBuffer(&u8g2);

}
/*==================[end of file]============================================*/
