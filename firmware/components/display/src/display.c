/*==================[inclusions]=============================================*/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "i2c_mcu.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "sdkconfig.h"

/*==================[macros and definitions]=================================*/
#define TX_BUFFER_LEN   160     /*!< u8g2 manda a lo sumo ~40 bytes por transferencia */
#define TEXT_MAX        32

/*==================[internal data definition]===============================*/
static u8g2_t u8g2;
static uint8_t tx_buffer[TX_BUFFER_LEN];
static uint16_t tx_len = 0;
static bool tx_overflow = false;

/*==================[internal functions definition]==========================*/
/** Callback de bytes de u8g2: junta los bytes de una transferencia y los manda por i2c_mcu.
 * El primer byte es el de control del SSD13xx (0x00 comando, 0x40 datos): hace de "registro". */
static uint8_t ByteCb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC:
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        tx_len = 0;
        tx_overflow = false;
        break;
    case U8X8_MSG_BYTE_SEND:
        if (tx_len + arg_int <= TX_BUFFER_LEN) {
            memcpy(&tx_buffer[tx_len], arg_ptr, arg_int);
            tx_len += arg_int;
        } else {
            tx_overflow = true;
        }
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        if (tx_overflow || tx_len < 2) {
            return 0;
        }
        /* u8g2 guarda la dirección ya corrida un bit a la izquierda (0x78): i2c_mcu usa 7 bits */
        return I2C_writeBytes(u8x8_GetI2CAddress(u8x8) >> 1, tx_buffer[0], tx_len - 1, &tx_buffer[1]) ? 1 : 0;
    default:
        return 0;
    }
    return 1;
}

/** Callback de pines y demoras de u8g2: la OLED por I2C no usa pines extra, solo demoras. */
static uint8_t DelayCb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        break;
    case U8X8_MSG_DELAY_MILLI: {
        TickType_t ticks = pdMS_TO_TICKS(arg_int);
        vTaskDelay(ticks > 0 ? ticks : 1);
        break;
    }
    case U8X8_MSG_DELAY_10MICRO:
        esp_rom_delay_us(10u * arg_int);
        break;
    case U8X8_MSG_DELAY_100NANO:
    case U8X8_MSG_DELAY_NANO:
        break;
    case U8X8_MSG_DELAY_I2C:
        esp_rom_delay_us(5);
        break;
    default:
        u8x8_SetGPIOResult(u8x8, 1);
        break;
    }
    return 1;
}

/*==================[external functions definition]==========================*/
bool Display_Init(void)
{
#if defined(CONFIG_DISPLAY_SSD1306)
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, ByteCb, DelayCb);
#elif defined(CONFIG_DISPLAY_SH1106)
    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2, U8G2_R0, ByteCb, DelayCb);
#else
    u8g2_Setup_ssd1315_i2c_128x64_noname_f(&u8g2, U8G2_R0, ByteCb, DelayCb);
#endif
    u8x8_SetI2CAddress(&u8g2.u8x8, CONFIG_DISPLAY_I2C_ADDRESS << 1);

    if (!I2C_probe(CONFIG_DISPLAY_I2C_ADDRESS, 50)) {
        return false;
    }
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);
    return true;
}

u8g2_t *Display_Handle(void)
{
    return &u8g2;
}

void Display_Clear(void)
{
    u8g2_ClearBuffer(&u8g2);
}

void Display_Flush(void)
{
    u8g2_SendBuffer(&u8g2);
}

void Display_PowerSave(bool apagada)
{
    u8g2_SetPowerSave(&u8g2, apagada ? 1 : 0);
}

void Display_Printf(int x, int y, const char *fmt, ...)
{
    char text[TEXT_MAX];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    u8g2_DrawStr(&u8g2, x, y, text);
}
