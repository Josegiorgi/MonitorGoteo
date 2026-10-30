/*! @mainpage Test buzzer y alarmas
 *
 * @section genDesc General Description
 *
 * Recorre cada patrón de alarma de buzzer_alarm (oclusión, desviación de flujo, fin de sachet,
 * batería baja y colocación) durante PATTERN_TIME_MS, con una pausa de silencio entre uno y otro,
 * e imprime por consola cuál suena. Repite la secuencia indefinidamente. Sirve para comprobar
 * el buzzer y que los patrones se distingan de oído.
 *
 * @section hardConn Hardware Connection
 *
 * |   Dispositivo           |   ESP32-C3   |
 * |:-----------------------:|:------------:|
 * | Buzzer                  | GPIO_10      |
 * | GND                     | GND          |
 *
 * @note El buzzer es pasivo y suena con la señal PWM del pin.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 29/10/2026 | Document creation		                         |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "buzzer_alarm.h"

/*==================[macros and definitions]=================================*/
#define PATTERN_TIME_MS   7000   /* cuánto suena cada patrón (se repite varias veces en ese tiempo) */
#define PAUSE_TIME_MS     3000   /* silencio entre un patrón y el siguiente */

/*==================[external functions definition]==========================*/
void app_main(void)
{
    if (!BuzzerAlarm_Init(BUZZER_ALARM_GPIO)) {
        printf("Error: no se pudo iniciar buzzer_alarm\n");
        return;
    }
    printf("Test de alarmas: %d patrones, %d s cada uno\n", BUZZER_ALARM_CANTIDAD, PATTERN_TIME_MS / 1000);

    while (1) {
        for (int p = 0; p < BUZZER_ALARM_CANTIDAD; p++) {
            printf("Patron %d/%d: %s\n", p + 1, BUZZER_ALARM_CANTIDAD, BuzzerAlarm_Nombre((buzzer_alarm_t)p));
            BuzzerAlarm_Start((buzzer_alarm_t)p);
            vTaskDelay(pdMS_TO_TICKS(PATTERN_TIME_MS));
            BuzzerAlarm_Stop();
            printf("  (pausa)\n");
            vTaskDelay(pdMS_TO_TICKS(PAUSE_TIME_MS));
        }
        printf("--- fin de la secuencia, se repite ---\n");
    }
}

/*==================[end of file]============================================*/
