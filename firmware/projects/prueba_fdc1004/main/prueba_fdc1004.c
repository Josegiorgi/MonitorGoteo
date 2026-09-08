/*! @mainpage Prueba FDC1004
 *
 * @section genDesc General Description
 *
 * Prueba y calibración del sensor de gota del monitor de goteo usando el módulo FDC1004
 * (conversor capacitancia-a-digital), sobre ESP32-C3. Tiene 3 modos seleccionables por
 * TEST_MODE (ver la guía de pruebas paso a paso en Google Docs):
 *
 * - MODE_I2C_CHECK: solo verifica la comunicación I2C con el chip (Device ID). No configura
 *   ninguna medición. Primer paso de todos, con cualquier cosa (o nada) conectada a los
 *   canales.
 * - MODE_RAW_CAPACITANCE: configura una medición single-ended en ACTIVE_CHANNEL y transmite
 *   la capacitancia (ya corregida por CAPDAC_OFFSET) sin ningún procesamiento. Sirve tanto
 *   para el capacitor de referencia como para ver el valor base real de las placas sin
 *   promediar, y así detectar si CAPDAC_OFFSET se queda corto (lectura saturada / no
 *   responde a cambios).
 * - MODE_BASELINE_OFFSET: modo final. Promedia BASELINE_SAMPLES muestras sin gota como
 *   referencia, y despues transmite continuamente la diferencia contra ese baseline, en pF.
 *   Sirve para las pruebas de perturbación estática y de gotas reales.
 *
 * @section hardConn Hardware Connection
 *
 * |   Pin del módulo FDC1004   |   ESP32-C3   |
 * |:---------------------------:|:--------------:|
 * | VCC                         | 3V3            |
 * | GND (lado I2C)               | GND            |
 * | SDA                          | GPIO_6         |
 * | SCL                          | GPIO_7         |
 * | Canal (ver ACTIVE_CHANNEL)   | placa activa (arco de cobre) |
 * | Shield (el asociado a ese canal en el módulo) | placa/guarda detrás de las placas |
 * | GND (lado sensor)            | placa de referencia (arco de cobre) |
 *
 * @note La placa de referencia va al pin GND del módulo, no a un segundo canal: el FDC1004
 * mide "canal contra GND", así que la capacitancia placa-placa que modula la gota queda
 * incluida directo en la lectura del canal (modo single-ended). El chip tiene 2 salidas de
 * shield (SHLD1/SHLD2), pero en single-ended quedan cortocircuitadas internamente - da lo
 * mismo cual de las dos exponga el módulo junto al canal usado, no requiere configuración
 * por firmware.
 *
 * @note Este módulo solo tiene soldados los pines de CIN2 y CIN3 - usar ACTIVE_CHANNEL para
 * elegir cuál, no se puede usar CIN1 ni CIN4.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 04/09/2026 | Document creation		                         |
 * | 07/09/2026 | Se agregan modos de prueba escalonados (I2C check, capacitancia cruda, offset contra baseline) |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_mcu.h"
#include "fdc1004.h"
/*==================[macros and definitions]=================================*/
#define MODE_I2C_CHECK       0 /*!< Paso 1: solo confirma que el chip responde por I2C */
#define MODE_RAW_CAPACITANCE 1 /*!< Paso 2/4: transmite la capacitancia cruda (corregida por CAPDAC), sin baseline */
#define MODE_BASELINE_OFFSET 2 /*!< Paso 5/6/7: promedia un baseline y transmite la diferencia en fF */

#define TEST_MODE            MODE_RAW_CAPACITANCE /*!< Modo activo. Cambiar acá para pasar de etapa. */

#define MEASUREMENT          1                   /*!< Registro de medición del FDC1004 a usar (1 a 4) */
#define ACTIVE_CHANNEL       FDC1004_CIN3         /*!< Este módulo solo tiene soldados CIN2 y CIN3 */
#define SAMPLE_RATE          FDC1004_RATE_100SPS  /*!< Tasa de muestreo: a mayor tasa, más ruido */
#define CAPDAC_OFFSET        0                    /*!< Offset en pasos de 3.125pF (0 a 31). Subir si la lectura cruda satura cerca de +15pF */
#define BASELINE_SAMPLES     30                   /*!< Muestras para calcular el valor base (sin gota), solo en MODE_BASELINE_OFFSET */
/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/
#if TEST_MODE == MODE_I2C_CHECK
static void RunI2CCheck(void);
#else
static float ReadNextCapacitance(void);
#if TEST_MODE == MODE_RAW_CAPACITANCE
static void RunRawCapacitance(void);
#elif TEST_MODE == MODE_BASELINE_OFFSET
static void RunBaselineOffset(void);
#endif
#endif

/*==================[internal functions definition]===========================*/

#if TEST_MODE == MODE_I2C_CHECK
static void RunI2CCheck(void) {
    while (1) {
        bool ok = FDC1004_Init();
        printf(ok ? "FDC1004 OK (Device ID correcto)\r\n" : "FDC1004 ERROR: no responde\r\n");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
#else

/* Cuantas veces reintentar (una vez por tick, ~10ms con CONFIG_FREERTOS_HZ=100) antes de
 * avisar por consola que el dato nunca llega, en vez de quedar esperando en silencio. */
#define DATA_READY_WARN_TICKS 300

/* Espera a que haya un dato listo y lo lee, corrigiendo por el offset de CAPDAC
 * (el driver devuelve el valor crudo relativo al CAPDAC configurado, ver fdc1004.h).
 *
 * @note vTaskDelay(1) espera 1 tick (no 1 ms): con "1 / portTICK_PERIOD_MS" y
 * CONFIG_FREERTOS_HZ=100 (portTICK_PERIOD_MS=10) la division entera da 0, es decir
 * vTaskDelay(0), que no cede el CPU de forma confiable y puede terminar disparando el
 * watchdog de la tarea IDLE si este while nunca corta (por ejemplo porque la medicion
 * nunca se configuro con exito). */
static float ReadNextCapacitance(void) {
    float capacitance_pF;
    uint16_t ticksWaited = 0;

    while (!FDC1004_DataReady(MEASUREMENT)) {
        vTaskDelay(1);
        if (++ticksWaited >= DATA_READY_WARN_TICKS) {
            printf("Sigo esperando datos del FDC1004: revisar si Configure/StartMeasurement "
                   "fallaron por I2C (NACK) o si hay un problema de cableado/alimentacion.\r\n");
            ticksWaited = 0;
        }
    }
    FDC1004_ReadCapacitance(MEASUREMENT, &capacitance_pF);

    return capacitance_pF + CAPDAC_OFFSET * 3.125f;
}

#if TEST_MODE == MODE_RAW_CAPACITANCE
static void RunRawCapacitance(void) {
    if (!FDC1004_Init()) {
        printf("No se pudo comunicar con el FDC1004. Revisar cableado.\r\n");
        return;
    }

    if (!FDC1004_ConfigureSingleEnded(MEASUREMENT, ACTIVE_CHANNEL, CAPDAC_OFFSET)) {
        printf("FDC1004_ConfigureSingleEnded fallo (NACK por I2C). Revisar cableado.\r\n");
        return;
    }
    if (!FDC1004_StartMeasurement(MEASUREMENT, SAMPLE_RATE, true)) {
        printf("FDC1004_StartMeasurement fallo (NACK por I2C). Revisar cableado.\r\n");
        return;
    }

    while (1) {
        printf("%.4f\r\n", ReadNextCapacitance());
    }
}
#elif TEST_MODE == MODE_BASELINE_OFFSET
static void RunBaselineOffset(void) {
    if (!FDC1004_Init()) {
        printf("No se pudo comunicar con el FDC1004. Revisar cableado.\r\n");
        return;
    }

    if (!FDC1004_ConfigureSingleEnded(MEASUREMENT, ACTIVE_CHANNEL, CAPDAC_OFFSET)) {
        printf("FDC1004_ConfigureSingleEnded fallo (NACK por I2C). Revisar cableado.\r\n");
        return;
    }
    if (!FDC1004_StartMeasurement(MEASUREMENT, SAMPLE_RATE, true)) {
        printf("FDC1004_StartMeasurement fallo (NACK por I2C). Revisar cableado.\r\n");
        return;
    }

    /* Calibracion: promedio de N muestras sin gota entre las placas */
    float sum = 0;
    for (uint8_t i = 0; i < BASELINE_SAMPLES; i++) {
        sum += ReadNextCapacitance();
    }
    float baseline_pF = sum / BASELINE_SAMPLES;
    printf("Baseline: %.4f pF\r\n", baseline_pF);

    while (1) {
        float capacitance_pF = ReadNextCapacitance();
        float offset_pF = capacitance_pF - baseline_pF;

        printf("%.4f\r\n", offset_pF);
    }
}
#endif /* TEST_MODE == MODE_RAW_CAPACITANCE / MODE_BASELINE_OFFSET */
#endif /* TEST_MODE == MODE_I2C_CHECK */

/*==================[external functions definition]==========================*/
void app_main(void) {
    I2C_initialize(I2C_MASTER_FREQ_HZ);

#if TEST_MODE == MODE_I2C_CHECK
    RunI2CCheck();
#elif TEST_MODE == MODE_RAW_CAPACITANCE
    RunRawCapacitance();
#elif TEST_MODE == MODE_BASELINE_OFFSET
    RunBaselineOffset();
#endif
}
/*==================[end of file]============================================*/
