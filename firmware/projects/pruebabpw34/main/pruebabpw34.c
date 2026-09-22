/*! @mainpage Prueba BPW34
 *
 * @section genDesc General Description
 *
 * Prueba del sensor de gota del monitor de goteo usando dos fotodiodos BPW34, sobre ESP32-C3.
 * Cada BPW34 va seguido de su propio amplificador (acondicionamiento de la corriente del
 * fotodiodo a tensión), y la salida de cada amplificador entra a un canal del ADC. Un LED
 * emisor alimentado desde 5V ilumina los fotodiodos a través de la cámara de goteo: al caer una
 * gota, la tensión de salida cambia momentáneamente.
 *
 * Esta primera versión solo levanta los datos: lee los dos canales con un período fijo y
 * transmite ambos valores calibrados en mV, sin filtrar, en una línea separados por coma
 * ("canal1,canal2"), formato que el serial plotter grafica como dos series.
 *
 * @note Igual que en prueba_opt101, se usa timer_mcu en vez de vTaskDelay: la interrupción del
 * timer solo notifica a ADCTask, y la lectura del ADC y el printf se hacen en la tarea.
 *
 * @note SAMPLE_PERIOD_US tiene que quedar por encima de lo que tarda imprimir una línea por la
 * UART de la consola (115200 baudios, ~87 us por caracter). Con dos valores de hasta 4 dígitos
 * la línea ocupa hasta 11 caracteres (~960 us), o sea que el período no puede bajar de ~1 ms
 * sin que ADCTask se atrase y termine disparando el watchdog.
 *
 * @section hardConn Hardware Connection
 *
 * |   Señal                              |   ESP32-C3          |
 * |:------------------------------------:|:-------------------:|
 * | Salida amplificador BPW34 n°1        | GPIO_2 (ADC1_CH2)   |
 * | Salida amplificador BPW34 n°2        | GPIO_3 (ADC1_CH3)   |
 * | Alimentación de los amplificadores   | 3V3                 |
 * | Alimentación del LED emisor (con R serie) | 5V             |
 * | GND de amplificadores y LED          | GND                 |
 *
 * @note Los amplificadores se alimentan con 3.3V, así que su salida nunca supera la tensión
 * máxima de entrada del ADC. Con la atenuación de 12dB del driver, el rango donde la
 * calibración del ESP32-C3 es precisa llega hasta ~2.5V: conviene ajustar la ganancia para que
 * el nivel base quede por debajo de eso. La línea de 5V del LED no debe llegar a ningún GPIO.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 22/09/2026 | Document creation		                         |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "timer_mcu.h"
#include "analog_io_mcu.h"
/*==================[macros and definitions]=================================*/
#define ADC_CHANNEL_BPW34_1  CH2   /*!< Canal ADC de la salida del amplificador del BPW34 n°1 (GPIO_2) */
#define ADC_CHANNEL_BPW34_2  CH3   /*!< Canal ADC de la salida del amplificador del BPW34 n°2 (GPIO_3).
                                        Se evita CH0 (GPIO_0) porque el driver lo comparte con el DAC */
#define SAMPLE_PERIOD_US     1200  /*!< Período fijo entre muestras (de ambos canales), en microsegundos
                                        (~833 Hz). No puede bajar de ~1 ms por el tiempo de impresión
                                        de la línea con los dos valores (ver nota en la descripción) */
/*==================[internal data definition]===============================*/
static TaskHandle_t adc_task_handle = NULL;

/*==================[internal functions declaration]=========================*/
static void FuncTimerSample(void *param);
static void ADCTask(void *pvParameter);

/*==================[internal functions definition]===========================*/

/** @brief Función invocada en la interrupción del timer. Solo notifica a ADCTask. */
static void FuncTimerSample(void *param) {
    vTaskNotifyGiveFromISR(adc_task_handle, pdFALSE);
}

/** @brief Tarea encargada de leer los dos canales del ADC y transmitirlos, una vez por
 *  notificación del timer. */
static void ADCTask(void *pvParameter) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint16_t bpw34_1_mV, bpw34_2_mV;
        AnalogInputReadSingle(ADC_CHANNEL_BPW34_1, &bpw34_1_mV);
        AnalogInputReadSingle(ADC_CHANNEL_BPW34_2, &bpw34_2_mV);

        printf("%u,%u\r\n", bpw34_1_mV, bpw34_2_mV);
    }
}

/*==================[external functions definition]==========================*/
void app_main(void) {
    analog_input_config_t adc_config_1 = {
        .input = ADC_CHANNEL_BPW34_1,
        .mode = ADC_SINGLE,
        .func_p = NULL,
        .param_p = NULL,
    };
    AnalogInputInit(&adc_config_1);

    analog_input_config_t adc_config_2 = {
        .input = ADC_CHANNEL_BPW34_2,
        .mode = ADC_SINGLE,
        .func_p = NULL,
        .param_p = NULL,
    };
    AnalogInputInit(&adc_config_2);

    xTaskCreate(&ADCTask, "BPW34_ADC", 4096, NULL, 5, &adc_task_handle);

    timer_config_t sample_timer = {
        .timer = TIMER_A,
        .period = SAMPLE_PERIOD_US,
        .func_p = FuncTimerSample,
        .param_p = NULL,
    };
    TimerInit(&sample_timer);
    TimerStart(TIMER_A);
}
/*==================[end of file]============================================*/
