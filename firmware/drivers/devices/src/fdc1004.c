/**
 * @file fdc1004.c
 * @brief Driver para el conversor capacitancia-a-digital FDC1004 via I2C
 *
 * @copyright Copyright (c) 2026
 *
 */

/*==================[inclusions]=============================================*/
#include "fdc1004.h"
#include "esp_log.h"
#include <stddef.h>

/*==================[macros and definitions]=================================*/
#define FDC1004_REG_MEAS_MSB(n)     (0x00 + 2 * ((n) - 1))
#define FDC1004_REG_MEAS_LSB(n)     (0x01 + 2 * ((n) - 1))
#define FDC1004_REG_CONF_MEAS(n)    (0x08 + ((n) - 1))
#define FDC1004_REG_FDC_CONF        0x0C
#define FDC1004_REG_DEVICE_ID       0xFF

#define FDC1004_DEVICE_ID_VALUE     0x1004
#define FDC1004_CHB_DISABLED        0x07    /*!< CHB = b111 -> medicion single-ended */

/*==================[internal functions definition]===========================*/
static const char *TAG = "FDC1004";

/*==================[external functions definition]==========================*/
bool FDC1004_Init(void) {
    uint16_t deviceId = 0;

    I2C_readWord(FDC1004_I2C_ADDRESS, FDC1004_REG_DEVICE_ID, &deviceId, 0);

    if (deviceId != FDC1004_DEVICE_ID_VALUE) {
        ESP_LOGE(TAG, "El FDC1004 no respondio en la direccion I2C 0x%02X (Device ID leido: 0x%04X, "
                      "esperado 0x%04X). Revisa cableado (SDA=GPIO_6, SCL=GPIO_7) y alimentacion.",
                 FDC1004_I2C_ADDRESS, deviceId, FDC1004_DEVICE_ID_VALUE);
        return false;
    }

    return true;
}

bool FDC1004_ConfigureSingleEnded(uint8_t measurement, fdc1004_channel_t channel, uint8_t capdac) {
    if (measurement < 1 || measurement > 4 || capdac > 31) {
        return false;
    }

    uint16_t conf = ((uint16_t)channel << 13) | ((uint16_t)FDC1004_CHB_DISABLED << 10) | ((uint16_t)capdac << 5);

    return I2C_writeWord(FDC1004_I2C_ADDRESS, FDC1004_REG_CONF_MEAS(measurement), conf);
}

bool FDC1004_StartMeasurement(uint8_t measurement, fdc1004_rate_t rate, bool repeat) {
    if (measurement < 1 || measurement > 4) {
        return false;
    }

    uint16_t fdcConf = ((uint16_t)rate << 10) | (repeat ? (1 << 8) : 0) | (1 << (8 - measurement));

    return I2C_writeWord(FDC1004_I2C_ADDRESS, FDC1004_REG_FDC_CONF, fdcConf);
}

bool FDC1004_DataReady(uint8_t measurement) {
    if (measurement < 1 || measurement > 4) {
        return false;
    }

    uint16_t fdcConf = 0;
    I2C_readWord(FDC1004_I2C_ADDRESS, FDC1004_REG_FDC_CONF, &fdcConf, 0);

    return (fdcConf & (1 << (4 - measurement))) != 0;
}

bool FDC1004_ReadCapacitance(uint8_t measurement, float *capacitance_pF) {
    if (measurement < 1 || measurement > 4 || capacitance_pF == NULL) {
        return false;
    }

    uint16_t msb = 0, lsb = 0;
    I2C_readWord(FDC1004_I2C_ADDRESS, FDC1004_REG_MEAS_MSB(measurement), &msb, 0);
    I2C_readWord(FDC1004_I2C_ADDRESS, FDC1004_REG_MEAS_LSB(measurement), &lsb, 0);

    /* Los 24 bits de dato (complemento a 2) ocupan la parte alta de estos 32 bits;
     * el shift aritmetico conserva el signo. */
    int32_t raw = (((int32_t)msb << 16) | lsb) >> 8;
    *capacitance_pF = (float)raw / 524288.0f; // 2^19

    return true;
}

/*==================[end of file]============================================*/
