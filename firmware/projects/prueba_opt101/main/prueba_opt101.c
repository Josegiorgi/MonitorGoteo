/*! @mainpage Prueba OPT101
 *
 * @section genDesc General Description
 *
 * Prueba del sensor de gota del monitor de goteo usando el fotodiodo con amplificador de
 * transimpedancia integrado OPT101, sobre ESP32-C3. El OPT101 y un LED enfrentado forman una
 * barrera de luz: mientras el haz llega completo al sensor la tensión de salida se mantiene
 * en un nivel base, y al caer una gota por el medio (interrumpiendo parcialmente el haz) esa
 * tensión cae momentáneamente. Este proyecto solo lee el pin ADC en loop y transmite el valor
 * crudo por consola para calibrar el circuito y ver la forma del pulso antes de escribir la
 * lógica de detección final.
 *
 * @section hardConn Hardware Connection
 *
 * |   Pin del OPT101   |   ESP32-C3   |
 * |:-------------------:|:--------------:|
 * | +V (pin 1)           | 3V3            |
 * | GND (pin 3, 4 y 5 - unidos entre sí) | GND |
 * | Vo (pin 8, salida)   | GPIO_2 (ADC1_CH2) |
 *
 * @note El OPT101 debe quedar enfrentado al LED emisor, con el hueco por donde cae la gota
 * entre ambos. La salida Vo ya viene amplificada por el transimpedancia interno del chip, no
 * requiere circuito externo adicional para esta prueba.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 09/09/2026 | Document creation		                         |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "analog_io_mcu.h"
/*==================[macros and definitions]=================================*/
#define ADC_CHANNEL     CH2 /*!< Canal ADC donde está conectado Vo del OPT101 (GPIO_2) */
#define SAMPLE_PERIOD_MS 20 /*!< Período entre muestras. Bajarlo si el pulso de la gota se ve recortado en el plotter */
/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/

/*==================[external functions definition]==========================*/
void app_main(void) {
    analog_input_config_t adc_config = {
        .input = ADC_CHANNEL,
        .mode = ADC_SINGLE,
        .func_p = NULL,
        .param_p = NULL,
    };
    AnalogInputInit(&adc_config);

    while (1) {
        uint16_t adc_value;
        AnalogInputReadSingle(ADC_CHANNEL, &adc_value);
        printf("%u\r\n", adc_value);

        vTaskDelay(SAMPLE_PERIOD_MS / portTICK_PERIOD_MS);
    }
}
/*==================[end of file]============================================*/
