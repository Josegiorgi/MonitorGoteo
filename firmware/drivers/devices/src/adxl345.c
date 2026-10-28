/**
 * @file adxl345.c
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 * @brief Driver del acelerómetro ADXL345 por I2C (ver adxl345.h)
 * @version 0.1
 * @date 2026-10-19
 */

/*==================[inclusions]=============================================*/
#include "adxl345.h"

/*==================[internal data definition]===============================*/
static uint8_t s_address = 0;   /*!< Dirección con la que respondió el chip (0 = todavía no se encontró) */

/*==================[internal functions declaration]=========================*/
/** Lee DEVID en la dirección dada. true si el chip contestó con 0xE5. */
static bool ProbeAddress(uint8_t address);

/*==================[internal functions definition]===========================*/
static bool ProbeAddress(uint8_t address)
{
    uint8_t id = 0;
    return I2C_readBytes(address, ADXL345_REG_DEVID, 1, &id, I2C_MASTER_TIMEOUT_MS) == 1
           && id == ADXL345_DEVID_VALUE;
}

/*==================[external functions definition]==========================*/
bool ADXL345_Init(void)
{
    s_address = 0;
    if (ProbeAddress(ADXL345_ADDRESS_SDO_LOW)) {
        s_address = ADXL345_ADDRESS_SDO_LOW;
    } else if (ProbeAddress(ADXL345_ADDRESS_SDO_HIGH)) {
        s_address = ADXL345_ADDRESS_SDO_HIGH;
    } else {
        return false;
    }

    /* El chip arranca en standby: primero formato y velocidad, y al final MEASURE. */
    return I2C_writeByte(s_address, ADXL345_REG_DATA_FORMAT, ADXL345_FORMAT_2G_FULLRES)
           && I2C_writeByte(s_address, ADXL345_REG_BW_RATE, ADXL345_BW_RATE_100HZ)
           && I2C_writeByte(s_address, ADXL345_REG_POWER_CTL, ADXL345_POWER_MEASURE);
}

uint8_t ADXL345_GetAddress(void)
{
    return s_address;
}

bool ADXL345_ReadRaw(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t raw[6];
    if (s_address == 0 || I2C_readBytes(s_address, ADXL345_REG_DATAX0, 6, raw, I2C_MASTER_TIMEOUT_MS) != 6) {
        return false;
    }
    /* Complemento a dos con el byte bajo primero */
    *x = (int16_t)((raw[1] << 8) | raw[0]);
    *y = (int16_t)((raw[3] << 8) | raw[2]);
    *z = (int16_t)((raw[5] << 8) | raw[4]);
    return true;
}

bool ADXL345_ReadAccel(float *ax, float *ay, float *az)
{
    int16_t x, y, z;
    if (!ADXL345_ReadRaw(&x, &y, &z)) {
        return false;
    }
    *ax = x / ADXL345_LSB_PER_G;
    *ay = y / ADXL345_LSB_PER_G;
    *az = z / ADXL345_LSB_PER_G;
    return true;
}

/*==================[end of file]============================================*/
