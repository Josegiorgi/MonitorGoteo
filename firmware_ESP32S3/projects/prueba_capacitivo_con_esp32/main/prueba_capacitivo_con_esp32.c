/*! @mainpage Prueba capacitivo con ESP32
 *
 * \section genDesc General Description
 *
 * Prueba del sensor de deteccion capacitiva (touch) del ESP32-S3, pensada para el sensor
 * de gota del monitor de goteo. Imprime por consola el valor crudo del canal touch en
 * loop, para poder calibrar el umbral de deteccion comparando el valor con aire entre los
 * electrodos contra el valor con una gota real pasando.
 *
 * @section hardConn Hardware Connection
 *
 * |   Electrodo         |   GPIO                      |
 * |:-------------------:|:----------------------------|
 * | Sensor (activo)      | GPIO4 (TOUCH_CH_4)          |
 * | Referencia           | GND                         |
 *
 * @note Sin shield: en esta placa GPIO14 (el canal de shield, fijo por hardware) no esta
 * disponible como pin de conexion - solo existe como via interna de la PCB, no como pin de
 * borde soldable. Se prueba solo con sensor + referencia.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 03/09/2026 | Document creation		                         |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_mcu.h"
/*==================[macros and definitions]=================================*/
#define TOUCH_CHANNEL       TOUCH_CH_4  /*!< Electrodo sensor: GPIO4 */
#define TOUCH_THRESHOLD     100000      /*!< Valor de partida, ajustar segun calibracion real */
#define READ_PERIOD_MS      20
#define BASELINE_SAMPLES    30          /*!< Muestras para calcular el valor base (sin gota) */
/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/

/*==================[external functions definition]==========================*/
void app_main(void){
	TouchInit(TOUCH_CHANNEL, TOUCH_THRESHOLD);
	TouchSetSensitivity(0);  /* bajar a 1 o 2 si hace falta mas sensibilidad */

	/* Calibracion: promedio de 30 muestras sin gota entre los electrodos */
	uint64_t sum = 0;
	for(uint8_t i = 0; i < BASELINE_SAMPLES; i++){
		sum += TouchReadRaw(TOUCH_CHANNEL);
		vTaskDelay(READ_PERIOD_MS / portTICK_PERIOD_MS);
	}
	uint32_t baseline = (uint32_t)(sum / BASELINE_SAMPLES);
	printf("Baseline: %lu\r\n", baseline);

	while(1){
		uint32_t raw = TouchReadRaw(TOUCH_CHANNEL);
		int32_t offset = (int32_t)raw - (int32_t)baseline;

		printf("%ld\r\n", offset);

		vTaskDelay(READ_PERIOD_MS / portTICK_PERIOD_MS);
	}
}
/*==================[end of file]============================================*/
