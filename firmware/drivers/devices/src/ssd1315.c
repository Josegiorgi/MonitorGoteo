/**
 * @file ssd1315.c
 * @brief Driver para display OLED 128x64 SSD1315 (compatible SSD1306) via I2C
 *
 * @copyright Copyright (c) 2026
 *
 */

/*==================[inclusions]=============================================*/
#include "ssd1315.h"
#include <string.h>

/*==================[macros and definitions]=================================*/
#define SSD1315_PAGES           (SSD1315_HEIGHT / 8)
#define SSD1315_BUFFER_SIZE     (SSD1315_WIDTH * SSD1315_PAGES)

#define SSD1315_CMD_MODE        0x00
#define SSD1315_DATA_MODE       0x40

#define FONT_WIDTH               5
#define FONT_HEIGHT               7

/*==================[internal data definition]===============================*/
static uint8_t devAddr = SSD1315_I2C_ADDRESS;
static uint8_t frameBuffer[SSD1315_BUFFER_SIZE];

/** Fuente 5x7, formato columna (bit0 = fila superior). Cubre el subconjunto de
 *  caracteres definido en font5x7_chars. Cualquier caracter fuera de ese
 *  conjunto se dibuja como espacio en blanco. */
static const char font5x7_chars[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.,:-+/%!?()";
static const uint8_t font5x7_data[][FONT_WIDTH] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
    {0x22, 0x41, 0x49, 0x49, 0x36}, // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
    {0x7C, 0x12, 0x11, 0x12, 0x7C}, // 'A'
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 'E'
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 'F'
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 'G'
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 'H'
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 'I'
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 'J'
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 'K'
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 'L'
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // 'M'
    {0x7F, 0x02, 0x04, 0x08, 0x7F}, // 'N'
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 'O'
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 'P'
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 'Q'
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 'R'
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 'S'
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 'T'
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 'U'
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 'V'
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // 'W'
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 'X'
    {0x03, 0x04, 0x78, 0x04, 0x03}, // 'Y'
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 'Z'
    {0x00, 0x00, 0x60, 0x00, 0x00}, // '.'
    {0x00, 0x40, 0x30, 0x00, 0x00}, // ','
    {0x00, 0x00, 0x36, 0x00, 0x00}, // ':'
    {0x08, 0x08, 0x08, 0x08, 0x08}, // '-'
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // '+'
    {0x40, 0x30, 0x08, 0x06, 0x01}, // '/'
    {0x63, 0x10, 0x08, 0x04, 0x63}, // '%'
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // '!'
    {0x02, 0x01, 0x51, 0x09, 0x06}, // '?'
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // '('
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // ')'
};

/*==================[internal functions declaration]=========================*/
static bool SSD1315_WriteCommand(uint8_t cmd);
static const uint8_t *SSD1315_GetGlyph(char c);

/*==================[internal functions definition]===========================*/
static const char *TAG = "SSD1315";

static bool SSD1315_WriteCommand(uint8_t cmd) {
    return I2C_writeByte(devAddr, SSD1315_CMD_MODE, cmd);
}

static const uint8_t *SSD1315_GetGlyph(char c) {
    for (uint8_t i = 0; i < sizeof(font5x7_chars) - 1; i++) {
        if (font5x7_chars[i] == c) {
            return font5x7_data[i];
        }
    }
    return font5x7_data[0]; // caracter no soportado -> espacio en blanco
}

/*==================[external functions definition]==========================*/
bool SSD1315_Init(uint8_t i2cAddress) {
    devAddr = i2cAddress;

    if (!SSD1315_WriteCommand(0xAE)) { // Display OFF (primer comando: sirve de test de comunicacion)
        ESP_LOGE(TAG, "El display no respondio en la direccion I2C 0x%02X. "
                      "Revisa cableado (SDA=GPIO_6, SCL=GPIO_7), alimentacion y direccion I2C.", devAddr);
        return false;
    }

    SSD1315_WriteCommand(0xD5); // Set display clock divide ratio/oscillator frequency
    SSD1315_WriteCommand(0x80);
    SSD1315_WriteCommand(0xA8); // Set multiplex ratio
    SSD1315_WriteCommand(SSD1315_HEIGHT - 1);
    SSD1315_WriteCommand(0xD3); // Set display offset
    SSD1315_WriteCommand(0x00);
    SSD1315_WriteCommand(0x40); // Set display start line = 0
    SSD1315_WriteCommand(0x8D); // Charge pump
    SSD1315_WriteCommand(0x14);
    SSD1315_WriteCommand(0x20); // Memory addressing mode
    SSD1315_WriteCommand(0x00); // Horizontal addressing mode
    SSD1315_WriteCommand(0xA1); // Segment remap (columna 127 -> SEG0)
    SSD1315_WriteCommand(0xC8); // COM output scan direction remapeado
    SSD1315_WriteCommand(0xDA); // COM pins hardware configuration
    SSD1315_WriteCommand(0x12);
    SSD1315_WriteCommand(0x81); // Contraste
    SSD1315_WriteCommand(0xCF);
    SSD1315_WriteCommand(0xD9); // Pre-charge period
    SSD1315_WriteCommand(0xF1);
    SSD1315_WriteCommand(0xDB); // VCOMH deselect level
    SSD1315_WriteCommand(0x40);
    SSD1315_WriteCommand(0xA4); // Resume to RAM content display
    SSD1315_WriteCommand(0xA6); // Display normal (no invertido)
    SSD1315_WriteCommand(0xAF); // Display ON

    SSD1315_Clear();
    SSD1315_UpdateScreen();

    return true;
}

void SSD1315_Clear(void) {
    memset(frameBuffer, 0x00, SSD1315_BUFFER_SIZE);
}

void SSD1315_UpdateScreen(void) {
    for (uint8_t page = 0; page < SSD1315_PAGES; page++) {
        SSD1315_WriteCommand(0xB0 + page); // Set page start address
        SSD1315_WriteCommand(0x00);        // Set lower column start address = 0
        SSD1315_WriteCommand(0x10);        // Set higher column start address = 0
        I2C_writeBytes(devAddr, SSD1315_DATA_MODE, SSD1315_WIDTH, &frameBuffer[page * SSD1315_WIDTH]);
    }
}

void SSD1315_SetPixel(uint8_t x, uint8_t y, ssd1315_color_t color) {
    if (x >= SSD1315_WIDTH || y >= SSD1315_HEIGHT) {
        return;
    }

    uint16_t index = x + (y / 8) * SSD1315_WIDTH;
    uint8_t bit = y % 8;

    if (color == SSD1315_COLOR_WHITE) {
        frameBuffer[index] |= (1 << bit);
    } else {
        frameBuffer[index] &= ~(1 << bit);
    }
}

void SSD1315_DrawChar(uint8_t x, uint8_t y, char c, ssd1315_color_t color) {
    const uint8_t *glyph = SSD1315_GetGlyph(c);

    for (uint8_t col = 0; col < FONT_WIDTH; col++) {
        uint8_t line = glyph[col];
        for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
            ssd1315_color_t pixelColor = (line & (1 << row)) ? color :
                (color == SSD1315_COLOR_WHITE ? SSD1315_COLOR_BLACK : SSD1315_COLOR_WHITE);
            SSD1315_SetPixel(x + col, y + row, pixelColor);
        }
    }
}

void SSD1315_DrawString(uint8_t x, uint8_t y, const char *str, ssd1315_color_t color) {
    uint8_t cursor = x;

    while (*str != '\0') {
        SSD1315_DrawChar(cursor, y, *str, color);
        cursor += FONT_WIDTH + 1; // 1 columna de espacio entre caracteres
        str++;
        if (cursor + FONT_WIDTH > SSD1315_WIDTH) {
            break;
        }
    }
}

void SSD1315_SetContrast(uint8_t contrast) {
    SSD1315_WriteCommand(0x81);
    SSD1315_WriteCommand(contrast);
}

void SSD1315_DisplayOn(bool on) {
    SSD1315_WriteCommand(on ? 0xAF : 0xAE);
}

/*==================[end of file]============================================*/
