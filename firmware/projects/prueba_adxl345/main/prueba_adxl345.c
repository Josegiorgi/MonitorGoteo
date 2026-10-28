/*! @mainpage Prueba ADXL345
 *
 * @section genDesc General Description
 *
 * Lectura del acelerómetro ADXL345 por I2C para ver, según cómo quede colocado en el dispositivo,
 * qué eje y qué ángulo son los que siguen la inclinación que interesa medir.
 *
 * Imprime por UART, a TX_PERIOD_MS, una línea como:
 *
 *   ax=+0.012 ay=-0.034 az=+0.998 g | arriba=+Z | angX=-0.7 angY=-1.9 angZ=+88.0 | pitch=+0.7 roll=-2.0 tilt=2.3 deg
 *
 * - ax, ay, az: aceleración en g sobre cada eje del chip. Con el equipo quieto es la gravedad: el
 *   eje que apunta hacia arriba marca cerca de +1 g (hacia abajo, -1 g) y los otros cerca de 0.
 * - arriba: el eje que más se acerca a la vertical y hacia dónde apunta (+Z: la flecha de Z mira
 *   hacia arriba). Dice de qué manera quedó colocado el dispositivo.
 * - angX, angY, angZ: ángulo de cada eje respecto de la horizontal, con signo. Un eje horizontal
 *   da 0°, uno vertical hacia arriba +90° y hacia abajo -90°. Al inclinar el dispositivo, el eje
 *   que cambia es el que sigue esa inclinación; el signo dice hacia qué lado.
 * - pitch, roll: los ángulos que usa MODE_ANGLE de prueba_opt101 y pruebabpw34. pitch =
 *   atan2(-ax, sqrt(ay^2 + az^2)) (es -angX: gira alrededor del eje Y) y roll = atan2(ay, az) (gira
 *   alrededor del eje X). Son los buenos cuando el eje Z queda hacia arriba.
 * - tilt: cuánto se aparta el eje que apunta más hacia arriba de la vertical, sin signo.
 *
 * Los ángulos solo valen con el dispositivo quieto (el movimiento suma aceleración a la gravedad).
 *
 * Ejes de este módulo (según su serigrafía, con el chip horizontal): Y apunta hacia el lado de los
 * pines, X a lo largo del lado más largo (sentido a confirmar: en un sistema derecho con Z saliendo
 * del chip, sería hacia la derecha) y Z sale del chip hacia quien mira. Con el módulo plano y el
 * chip hacia arriba, bajar el lado hacia donde apunta X da pitch positivo y subir el lado de los
 * pines (Y) da roll positivo.
 *
 * @section hardConn Hardware Connection
 *
 * |   ADXL345   |   ESP32-C3   |
 * |:-----------:|:------------:|
 * | SCL         | GPIO_7       |
 * | SDA         | GPIO_21      |
 * | GND         | GND          |
 * | alimentación | 3V3         |
 * | CS          | 3V3 (modo I2C) |
 * | SDO         | GND (dirección 0x53) |
 *
 * @note SDO no puede quedar sin conectar. Si el chip no responde, el programa lo avisa cada
 * segundo y sigue intentando: se puede corregir el cableado sin volver a cargar el programa.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 19/10/2026 | Document creation		                         |
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
#include "adxl345.h"

/*==================[macros and definitions]=================================*/
#define TX_PERIOD_MS        100
#define AVG_SAMPLES         8           /* lecturas que se promedian por línea, para filtrar vibraciones */
#define I2C_FREQ_HZ         100000      /* 100 kHz: tolera mejor cables largos o soldaduras flojas */
#define RAD_TO_DEG          57.29578f

/*==================[internal functions declaration]=========================*/
static bool ReadAccelAveraged(float *ax, float *ay, float *az);

/*==================[external functions definition]==========================*/
void app_main(void)
{
    if (!I2C_initialize(I2C_FREQ_HZ)) {
        printf("Error: no se pudo inicializar el bus I2C (SDA=GPIO_21, SCL=GPIO_7)\n");
        return;
    }

    while (!ADXL345_Init()) {
        printf("Error: el ADXL345 no responde (se probo 0x53 y 0x1D). Revisar SDA/SCL, alimentacion, CS a 3V3 y SDO a GND.\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    printf("ADXL345 OK (direccion 0x%02X)\n", ADXL345_GetAddress());

    while (1) {
        float ax, ay, az;
        if (!ReadAccelAveraged(&ax, &ay, &az)) {
            printf("Error de lectura del ADXL345\n");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        float norm = sqrtf(ax * ax + ay * ay + az * az);
        if (norm < 0.2f) {   /* en caida libre o chip mal leido: no hay gravedad con la que calcular ángulos */
            printf("ax=%+.3f ay=%+.3f az=%+.3f g | sin gravedad medible\n", ax, ay, az);
            vTaskDelay(pdMS_TO_TICKS(TX_PERIOD_MS));
            continue;
        }

        /* Eje que más se acerca a la vertical */
        char arriba_eje = 'X';
        float arriba = ax;
        if (fabsf(ay) > fabsf(arriba)) { arriba = ay; arriba_eje = 'Y'; }
        if (fabsf(az) > fabsf(arriba)) { arriba = az; arriba_eje = 'Z'; }

        float ang_x = asinf(ax / norm) * RAD_TO_DEG;
        float ang_y = asinf(ay / norm) * RAD_TO_DEG;
        float ang_z = asinf(az / norm) * RAD_TO_DEG;
        float pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
        float roll = atan2f(ay, az) * RAD_TO_DEG;
        float tilt = acosf(fabsf(arriba) / norm) * RAD_TO_DEG;

        printf("ax=%+.3f ay=%+.3f az=%+.3f g | arriba=%c%c | angX=%+.1f angY=%+.1f angZ=%+.1f | pitch=%+.1f roll=%+.1f tilt=%.1f deg\n",
               ax, ay, az, arriba >= 0 ? '+' : '-', arriba_eje, ang_x, ang_y, ang_z, pitch, roll, tilt);
        vTaskDelay(pdMS_TO_TICKS(TX_PERIOD_MS));
    }
}

/*==================[internal functions definition]==========================*/
static bool ReadAccelAveraged(float *ax, float *ay, float *az)
{
    float sx = 0, sy = 0, sz = 0;
    for (int i = 0; i < AVG_SAMPLES; i++) {
        float x, y, z;
        if (!ADXL345_ReadAccel(&x, &y, &z)) {
            return false;
        }
        sx += x;
        sy += y;
        sz += z;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    *ax = sx / AVG_SAMPLES;
    *ay = sy / AVG_SAMPLES;
    *az = sz / AVG_SAMPLES;
    return true;
}

/*==================[end of file]============================================*/
