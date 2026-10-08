/*! @mainpage Prueba MPU-6000
 *
 * @section genDesc General Description
 *
 * Lectura del acelerómetro MPU-6000 por I2C y cálculo del ángulo de inclinación del
 * monitor de goteo, para registrar a qué ángulo se dejan de detectar gotas.
 *
 * Imprime por UART, a TX_PERIOD_MS: aceleración en g (ax, ay, az), pitch, roll y la
 * inclinación total respecto de la vertical (tilt). Los ángulos salen de la dirección del
 * vector gravedad, así que solo valen con el dispositivo quieto (sin aceleraciones lineales).
 *
 * El eje que apunta hacia abajo con el dispositivo en su posición normal queda como
 * referencia de 0° de tilt: ajustar TILT_AXIS según cómo se suelde el chip en la placa.
 *
 * @section hardConn Hardware Connection
 *
 * |   Módulo GY-521   |   ESP32-C3   |
 * |:-----------------:|:------------:|
 * | VCC               | 3V3          |
 * | GND               | GND          |
 * | SCL               | GPIO_7       |
 * | SDA               | GPIO_21      |
 *
 * @note AD0, INT, XDA y XCL quedan sin conectar (dirección 0x68). Ver el README.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 08/10/2026 | Document creation		                         |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_mcu.h"
#include "mpu6050.h"

/*==================[macros and definitions]=================================*/
#define TX_PERIOD_MS        100
#define ACCEL_LSB_PER_G     16384.0f    /* rango +-2 g (MPU6050_ACCEL_FS_2) */
#define RAD_TO_DEG          57.29578f
#define AVG_SAMPLES         8           /* promedio para filtrar vibraciones */

/** Eje del chip que apunta hacia arriba en la posición normal (0° de tilt): 'x', 'y' o 'z'. */
#define TILT_AXIS           'z'

/*==================[internal functions declaration]=========================*/
static void ReadAccelG(float *ax, float *ay, float *az);

/*==================[external functions definition]==========================*/
void app_main(void)
{
    if (!I2C_initialize(I2C_MASTER_FREQ_HZ)) {
        printf("Error: no se pudo inicializar el bus I2C (SDA=GPIO_21, SCL=GPIO_7)\n");
        return;
    }

    MPU6050_initialize();
    if (!MPU6050_testConnection()) {
        printf("Error: el MPU-6000 no responde. Revisar SDA/SCL, VDD, AD0 y pull-ups.\n");
        return;
    }
    printf("MPU-6000 OK\n");

    while (1) {
        float ax, ay, az;
        ReadAccelG(&ax, &ay, &az);

        float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
        float roll = atan2f(ay, az) * RAD_TO_DEG;

        float up = (TILT_AXIS == 'x') ? ax : (TILT_AXIS == 'y') ? ay : az;
        float norm = sqrtf(ax * ax + ay * ay + az * az);
        float tilt = (norm > 0.01f) ? acosf(fabsf(up) / norm) * RAD_TO_DEG : 0.0f;

        printf("ax=%.3f ay=%.3f az=%.3f g | pitch=%.1f roll=%.1f tilt=%.1f deg\n",
               ax, ay, az, pitch, roll, tilt);
        vTaskDelay(pdMS_TO_TICKS(TX_PERIOD_MS));
    }
}

/*==================[internal functions definition]==========================*/
static void ReadAccelG(float *ax, float *ay, float *az)
{
    int32_t sx = 0, sy = 0, sz = 0;
    for (int i = 0; i < AVG_SAMPLES; i++) {
        int16_t x, y, z;
        MPU6050_getAcceleration(&x, &y, &z);
        sx += x;
        sy += y;
        sz += z;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    *ax = sx / (float)AVG_SAMPLES / ACCEL_LSB_PER_G;
    *ay = sy / (float)AVG_SAMPLES / ACCEL_LSB_PER_G;
    *az = sz / (float)AVG_SAMPLES / ACCEL_LSB_PER_G;
}

/*==================[end of file]============================================*/
