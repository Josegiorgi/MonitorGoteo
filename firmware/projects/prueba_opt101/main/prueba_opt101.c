/*! @mainpage Prueba OPT101
 *
 * @section genDesc General Description
 *
 * Prueba del sensor de gota del monitor de goteo usando el fotodiodo con amplificador de
 * transimpedancia integrado OPT101, sobre ESP32-C3. El OPT101 y un LED enfrentado forman una
 * barrera de luz: mientras el haz llega completo al sensor la tensión de salida se mantiene
 * en un nivel base, y al caer una gota por el medio (interrumpiendo/refractando parcialmente
 * el haz) esa tensión cambia momentáneamente.
 *
 * Este proyecto lee el ADC de forma periódica, disparado por un timer de hardware (driver
 * timer_mcu), la pasa por un filtro pasabanda (pasabajos + pasaaltos Butterworth en cascada,
 * componente signal_processing) y transmite el resultado por consola en tiempo real, sin
 * cortes. La salida ya no es una tensión absoluta (queda centrada en 0, sin la continua).
 *
 * @note Se usa timer_mcu en vez de vTaskDelay porque el pulso de la gota dura pocos
 * milisegundos: con vTaskDelay, el período mínimo real queda atado al tick de FreeRTOS, y pedir
 * un período más corto que un tick redondea a vTaskDelay(0), que no le cede el CPU a la tarea
 * IDLE y dispara el watchdog. La interrupción del timer solo notifica a ADCTask (no lee el ADC
 * ni hace printf ahí mismo, que no es seguro dentro de una ISR); la lectura y la impresión
 * pasan en una tarea normal que espera esa notificación.
 *
 * @note SAMPLE_PERIOD_US tiene que quedar entre 0.5 y 1ms (para resolver bien el pico de la
 * gota) y por encima de lo que tarda leer+filtrar+imprimir una muestra, o ADCTask nunca llega a
 * bloquearse de verdad en ulTaskNotifyTake() y termina matando de hambre a la tarea IDLE
 * (dispara el watchdog).
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
 * | 15/09/2026 | Se agrega MODE_BASELINE_OFFSET para filtrar la deriva ambiental |
 * | 17/09/2026 | Se saca MODE_BASELINE_OFFSET (a rehacer luego del rediseño del soporte). Salida ahora en mV en vez de cuenta cruda del ADC, para poder chequear si el LED satura al sensor |
 * | 17/09/2026 | Se reemplaza vTaskDelay por timer_mcu (TIMER_A) para poder muestrear más rápido que 1 tick de FreeRTOS sin disparar el watchdog |
 * | 17/09/2026 | Se cambia a captura por tandas (buffer en RAM + volcado al final) para desacoplar el muestreo del cuello de botella de la UART |
 * | 17/09/2026 | Se agrega medición real de cuánto tarda AnalogInputReadSingle al arrancar |
 * | 17/09/2026 | Se vuelve a impresión en tiempo real (sin tandas/buffer) para que la salida se vea continua, ahora que se confirmó que el watchdog no era por el período de muestreo sino por el loop de impresión sin yield |
 * | 17/09/2026 | Se agrega filtro pasabajos Butterworth (IIR, componente signal_processing de la cátedra SAPS) para suavizar el ruido de muestra a muestra sin aplanar el pulso de la gota |
 * | 17/09/2026 | Se agrega pasaaltos en cascada (pasabanda) para sacar la continua/deriva de fondo. SAMPLE_PERIOD_US queda fijo entre 0.5 y 1ms, necesario para resolver bien el pico |
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
#include "iir_filter.h"
/*==================[macros and definitions]=================================*/
#define ADC_CHANNEL          CH2   /*!< Canal ADC donde está conectado Vo del OPT101 (GPIO_2) */
#define SAMPLE_PERIOD_US     1000  /*!< Período fijo entre muestras, en microsegundos. Tiene que
                                        quedar entre 0.5 y 1 ms para resolver bien el pico de la
                                        gota - queda en el techo de ese rango para tener más
                                        margen posible contra el watchdog */
#define CUTOFF_LOWPASS_HZ    60.0f /*!< Corte del pasabajos: 30Hz aplanaba el pulso de la gota
                                        junto con el ruido, 100Hz dejaba pasar demasiado ruido -
                                        punto intermedio, ajustar según lo que se vea */
#define CUTOFF_HIGHPASS_HZ   1.0f  /*!< Corte del pasaaltos: saca la continua/deriva de fondo,
                                        bastante por debajo del contenido en frecuencia del pulso
                                        de la gota para no tocarlo */
#define FILTER_ORDER         ORDER_2 /*!< Orden del Butterworth (ORDER_2, 4, 6 u 8), para ambos filtros */
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

/** @brief Tarea encargada de leer el ADC, filtrarlo (pasabanda: pasabajos + pasaaltos en
 *  cascada) y transmitirlo, una vez por notificación del timer. */
static void ADCTask(void *pvParameter) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint16_t voltage_mV;
        AnalogInputReadSingle(ADC_CHANNEL, &voltage_mV);

        /* LowPassFilter/HiPassFilter guardan el estado internamente entre llamadas, así que
         * llamarlos con signal_lenght=1 (una muestra por vez) funciona como filtro en tiempo
         * real. El pasaaltos va después del pasabajos y saca la continua - la salida queda
         * centrada en 0 (con la gota como pico positivo o negativo), no es más una tensión
         * absoluta, por eso ahora se imprime como entero con signo. */
        float raw_mV = (float)voltage_mV;
        float lowpassed_mV, filtered_mV;
        LowPassFilter(&raw_mV, &lowpassed_mV, 1);
        HiPassFilter(&lowpassed_mV, &filtered_mV, 1);

        int rounded_mV = (int)(filtered_mV >= 0 ? filtered_mV + 0.5f : filtered_mV - 0.5f);
        printf("%d\r\n", rounded_mV);
    }
}

/*==================[external functions definition]==========================*/
void app_main(void) {
    analog_input_config_t adc_config = {
        .input = ADC_CHANNEL,
        .mode = ADC_SINGLE,
        .func_p = NULL,
        .param_p = NULL,
    };
    AnalogInputInit(&adc_config);

    LowPassInit(1000000.0f / SAMPLE_PERIOD_US, CUTOFF_LOWPASS_HZ, FILTER_ORDER);
    HiPassInit(1000000.0f / SAMPLE_PERIOD_US, CUTOFF_HIGHPASS_HZ, FILTER_ORDER);

    xTaskCreate(&ADCTask, "OPT101_ADC", 4096, NULL, 5, &adc_task_handle);

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
