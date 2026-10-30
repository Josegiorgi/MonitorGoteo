/*! @mainpage Test I2C scan
 *
 * @section genDesc General Description
 *
 * Prueba cada 2 s solo las direcciones I2C de los dispositivos del equipo (OLED 0x3C y ADXL345
 * en 0x53 o 0x1D) con i2c_master_probe (I2C_probe) e imprime para cada una si responde. Si
 * responde alguna dirección del ADXL345, lee el registro DEVID (0x00) y verifica que valga 0xE5.
 * Como repite la prueba todo el tiempo, se pueden mover los cables mientras corre para detectar
 * soldaduras flojas. Con SCAN_ALL_ADDRESSES en 1 también barre 0x08 a 0x77 y lista cualquier
 * otro dispositivo que responda (útil si un módulo tiene una dirección inesperada).
 *
 * Cada línea termina con [aciertos/pruebas] acumulados desde que arrancó, para comparar si un
 * cambio de cables o de soldadura mejora la conexión. Los errores del driver I2C se silencian.
 *
 * Salida esperada con el hardware bien (OLED y ADXL345 con SDO a GND):
 *
 *   0x3C  OLED                   OK
 *   0x53  ADXL345 (SDO a GND)    OK
 *         DEVID = 0xE5 (correcto)
 *   0x1D  ADXL345 (SDO a 3V3)    no responde
 *
 * @section hardConn Hardware Connection
 *
 * |   Dispositivo   |   ESP32-C3   |
 * |:---------------:|:------------:|
 * | OLED SCL        | GPIO_7       |
 * | OLED SDA        | GPIO_21      |
 * | ADXL345 SCL     | GPIO_7       |
 * | ADXL345 SDA     | GPIO_21      |
 * | GND / 3V3       | GND / 3V3    |
 *
 * @note Los pines (I2C_MASTER_SCL_IO e I2C_MASTER_SDA_IO) salen de i2c_mcu.h, que es donde
 * está definido el bus. I2C_initialize() activa los pull-ups internos.
 *
 * @note La consola va por USB Serial/JTAG (sdkconfig.defaults): GPIO20/21 son RX/TX de UART0
 * y GPIO21 es el SDA del I2C.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 29/10/2026 | Document creation		                         |
 * | 29/10/2026 | Prueba solo las direcciones de los dispositivos del equipo y verifica el DEVID del ADXL345 |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_mcu.h"
#include "adxl345.h"

/*==================[macros and definitions]=================================*/
#define I2C_FREQ_HZ       100000   /* 100 kHz: tolera mejor cables largos o soldaduras flojas */
#define PROBE_TIMEOUT_MS  50
#define TEST_PERIOD_MS    2000
#define OLED_ADDRESS      0x3C
#define ADDR_FIRST        0x08     /* las direcciones 0x00-0x07 y 0x78-0x7F están reservadas */
#define ADDR_LAST         0x77
#define SCAN_ALL_ADDRESSES 1       /* 1: además de los dispositivos del equipo, barre 0x08-0x77 y muestra cualquier otro que responda */

typedef struct {
    uint8_t address;
    const char *name;
    bool is_adxl345;    /*!< true: si responde, se lee y verifica el DEVID */
} device_t;

static const device_t devices[] = {
    {OLED_ADDRESS,             "OLED",                 false},
    {ADXL345_ADDRESS_SDO_LOW,  "ADXL345 (SDO a GND)",  true},
    {ADXL345_ADDRESS_SDO_HIGH, "ADXL345 (SDO a 3V3)",  true},
};

#define N_DEVICES   (sizeof(devices) / sizeof(devices[0]))

/*==================[external functions definition]==========================*/
void app_main(void)
{
    if (!I2C_initialize(I2C_FREQ_HZ)) {
        printf("Error: no se pudo inicializar el bus I2C (SDA=GPIO_%d, SCL=GPIO_%d)\n",
               I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
        return;
    }
    printf("Test I2C (SDA=GPIO_%d, SCL=GPIO_%d, %d kHz, pull-ups internos)\n",
           I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO, I2C_FREQ_HZ / 1000);

    /* El driver I2C imprime un error por cada transacción fallida; el test ya informa el resultado */
    esp_log_level_set("i2c.master", ESP_LOG_NONE);

    uint32_t tests[N_DEVICES] = {0}, acks[N_DEVICES] = {0};          /* pruebas de dirección y cuántas respondieron */
    uint32_t id_tests[N_DEVICES] = {0}, id_ok[N_DEVICES] = {0};      /* lecturas de DEVID y cuántas dieron 0xE5 */
    uint32_t wr_tests[N_DEVICES] = {0}, wr_ok[N_DEVICES] = {0};      /* escrituras de diagnóstico y cuántas anduvieron */

    while (1) {
        for (size_t i = 0; i < N_DEVICES; i++) {
            const device_t *d = &devices[i];
            bool ok = I2C_probe(d->address, PROBE_TIMEOUT_MS);
            tests[i]++;
            acks[i] += ok;
            printf("0x%02X  %-22s %-12s [%lu/%lu]\n", d->address, d->name, ok ? "OK" : "no responde",
                   (unsigned long)acks[i], (unsigned long)tests[i]);

            if (ok && d->is_adxl345) {
                uint8_t id = 0;
                bool read_ok = I2C_readBytes(d->address, ADXL345_REG_DEVID, 1, &id, PROBE_TIMEOUT_MS) == 1;
                id_tests[i]++;
                id_ok[i] += (read_ok && id == ADXL345_DEVID_VALUE);
                if (read_ok) {
                    printf("      DEVID = 0x%02X (%s) [%lu/%lu]\n", id,
                           id == ADXL345_DEVID_VALUE ? "correcto" : "inesperado, se esperaba 0xE5",
                           (unsigned long)id_ok[i], (unsigned long)id_tests[i]);
                } else {
                    printf("      DEVID: error de lectura [%lu/%lu]\n",
                           (unsigned long)id_ok[i], (unsigned long)id_tests[i]);
                }
                /* Diagnóstico: una escritura inofensiva (BW_RATE con su valor por defecto) prueba si el
                 * chip acepta la dirección del registro y el dato. Si la escritura anda y la lectura
                 * no, falla la lectura (el reinicio de la transacción); si fallan las dos, es el chip o el cableado. */
                bool w_ok = I2C_writeByte(d->address, ADXL345_REG_BW_RATE, ADXL345_BW_RATE_100HZ);
                wr_tests[i]++;
                wr_ok[i] += w_ok;
                printf("      escritura BW_RATE: %s [%lu/%lu]\n", w_ok ? "OK" : "falla",
                       (unsigned long)wr_ok[i], (unsigned long)wr_tests[i]);
            }
        }
#if SCAN_ALL_ADDRESSES
        /* Barrido completo: muestra también lo que responda en otras direcciones */
        int others = 0;
        for (uint8_t addr = ADDR_FIRST; addr <= ADDR_LAST; addr++) {
            bool known = false;
            for (size_t i = 0; i < sizeof(devices) / sizeof(devices[0]); i++) {
                known |= (devices[i].address == addr);
            }
            if (!known && I2C_probe(addr, PROBE_TIMEOUT_MS)) {
                printf("0x%02X  otro dispositivo       OK\n", addr);
                others++;
            }
        }
        if (others == 0) {
            printf("(ninguna otra direccion responde)\n");
        }
#endif
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(TEST_PERIOD_MS));
    }
}

/*==================[end of file]============================================*/
