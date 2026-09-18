/*! @mainpage Prueba OPT101
 *
 * @section genDesc General Description
 *
 * Prueba del sensor de gota del monitor de goteo usando el fotodiodo con amplificador de
 * transimpedancia integrado OPT101, sobre ESP32-C3. El OPT101 y un LED enfrentado forman una
 * barrera de luz: mientras el haz llega completo al sensor la tensión de salida se mantiene
 * en un nivel base, y al caer una gota por el medio (interrumpiendo/refractando parcialmente
 * el haz) esa tensión cambia momentáneamente. Tiene 3 modos seleccionables por TEST_MODE:
 *
 * - MODE_RAW: transmite el valor calibrado en mV, sin filtrar. Sirve para verificar que el
 *   sensor/ADC responden bien (por ejemplo, si "MODE_FILTERED" da algo raro, comparar acá para
 *   saber si el problema está en el filtro o en otro lado).
 * - MODE_FILTERED: pasa la lectura por un pasabanda Butterworth (pasaaltos + pasabajos en
 *   cascada, componente signal_processing) antes de transmitirla. La salida ya no es una
 *   tensión absoluta - queda centrada en 0, sin la continua. El corte del pasabanda (~85-195Hz)
 *   sale de un análisis espectral real (ver ../analisis_opt101): comparando con FFT la misma
 *   grabación antes/después de empezar a gotear, el exceso de energía con goteo se concentra
 *   en ~90-200Hz, no en las frecuencias bajas donde se había estado filtrando antes - por eso
 *   nunca se veía el pico, se estaba cortando justo la banda que lleva la información.
 * - MODE_DETECTOR: además del pasabanda, pasa la señal filtrada por el detector de correlación
 *   cruzada (componente signal_processing/xcorr_detector) con la plantilla de la gota
 *   (drop_template.h, el promedio de 8 gotas reales). Cuando la correlación pasa el umbral y
 *   se confirma el pico, se prende un LED (DETECTION_LED) durante LED_ON_TIME_MS. Sigue
 *   transmitiendo la señal filtrada muestra a muestra, igual que MODE_FILTERED.
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
 * | 18/09/2026 | La salida volvió a imprimirse con decimales (%.2f): redondear a entero después de sacar la continua truncaba la resolución del pulso (que puede ser de menos de 1 mV) y la señal se veía como una onda cuadrada |
 * | 18/09/2026 | Se agrega MODE_RAW (sin filtro) para poder comparar contra MODE_FILTERED y aislar problemas |
 * | 18/09/2026 | CUTOFF_HIGHPASS_HZ sube de 1Hz (oscilaba solo, inestabilidad numérica) a 5Hz (aplanaba el pulso de la gota) a 2Hz, punto intermedio |
 * | 18/09/2026 | Se saca el pasaaltos Butterworth (a 2Hz seguía sin verse el pico y seguía habiendo ruido - el filtro, aunque ya no oscilaba visiblemente, seguía en zona numérica incómoda). Se reemplaza por un baseline adaptativo (media móvil exponencial lenta, restada a la señal), numéricamente robusto para cualquier constante de tiempo |
 * | 18/09/2026 | FILTER_ORDER sube de ORDER_2 a ORDER_4: con 2do orden, mover CUTOFF_LOWPASS_HZ para sacar más ruido siempre terminaba aplanando el pulso también (transición muy suave entre pasa y corta). Un orden mayor cae más brusco más allá del corte, sin tocar tanto lo que pasa cerca de él |
 * | 18/09/2026 | Análisis espectral (ver ../analisis_opt101) muestra que el exceso de energía con goteo está en ~90-200Hz, no en frecuencias bajas. Se saca el baseline adaptativo y se vuelve a un pasaaltos Butterworth, ahora con corte en 85Hz en vez de 1-5Hz - a esta relación corte/muestreo ya no hay problema de estabilidad numérica. CUTOFF_LOWPASS_HZ sube de 50Hz a 195Hz para dejar pasar toda la banda de interés |
 * | 19/09/2026 | Se agrega MODE_DETECTOR: detección de gotas por correlación cruzada con una plantilla (nuevo componente xcorr_detector en middleware, plantilla en drop_template.h) y aviso con un LED |
 * | 19/09/2026 | MODE_DETECTOR sigue transmitiendo la señal filtrada muestra a muestra (como MODE_FILTERED) mientras prende el LED al detectar; se saca el mensaje de texto por gota, que rompía el formato del plotter |
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
#include "xcorr_detector.h"
#include "led.h"
#include "drop_template.h"
/*==================[macros and definitions]=================================*/
#define MODE_RAW               0 /*!< Transmite el valor calibrado en mV, sin filtrar */
#define MODE_FILTERED          1 /*!< Pasa la lectura por el filtro pasabanda antes de transmitirla */
#define MODE_DETECTOR          2 /*!< Detecta la gota por correlación cruzada con la plantilla y prende un LED */

#define TEST_MODE              MODE_DETECTOR /*!< Modo activo. Cambiar acá para pasar de etapa. */

#define ADC_CHANNEL          CH2   /*!< Canal ADC donde está conectado Vo del OPT101 (GPIO_2) */
#define SAMPLE_PERIOD_US     700  /*!< Período fijo entre muestras, en microsegundos. Tiene que
                                        quedar entre 0.5 y 1 ms para resolver bien el pico de la
                                        gota - queda en el techo de ese rango para tener más
                                        margen posible contra el watchdog */
#define CUTOFF_HIGHPASS_HZ   85.0f  /*!< Corte del pasaaltos. El análisis espectral (FFT,
                                        misma grabación antes/después de empezar a gotear, ver
                                        ../analisis_opt101) mostró que el exceso de energía con
                                        goteo arranca recién cerca de los 90Hz - por debajo de
                                        eso es todo deriva/ruido de baja frecuencia sin
                                        información de la gota */
#define CUTOFF_LOWPASS_HZ    195.0f /*!< Corte del pasabajos. El mismo análisis mostró que el
                                        exceso de energía llega hasta ~190-200Hz (con máximos en
                                        100-120Hz, 150-160Hz y 180-190Hz) - por encima de eso ya
                                        no hay diferencia entre con/sin goteo, solo ruido */
#define FILTER_ORDER         ORDER_4 /*!< Orden del Butterworth (ORDER_2, 4, 6 u 8), para ambos
                                        filtros. Con ORDER_2 (12dB/octava) la transición entre
                                        "pasa" y "corta" es muy suave; un orden mayor cae más
                                        brusco más allá del corte */

/* Parámetros de MODE_DETECTOR (los valores salen del análisis de ../analisis_opt101) */
#define DETECTION_LED        LED_2  /*!< LED que se prende al detectar una gota (LED_2 = GPIO_10; LED_3 = GPIO_5; evitar LED_1 = GPIO_20, que es el RX de la UART de la consola) */
#define XCORR_THRESHOLD      13.7f  /*!< Umbral de la correlación, en mV de la señal filtrada. Es 12 veces el desvío de la
                                        correlación sobre la señal filtrada SIN goteo (donde el ruido llega a ~4.6 veces
                                        ese desvío); la gota más chica confirmada da ~15.5, el ruido, hasta ~5.2 */
#define XCORR_PEAK_WINDOW_MS 15     /*!< Tiempo que se espera, tras pasar el umbral, quedándose con el máximo de la
                                        correlación. La plantilla oscila (pico, valle, rebote) y su correlación tiene
                                        lóbulos laterales a ~6ms del pico principal que también pasan el umbral */
#define XCORR_REFRACTORY_MS  100    /*!< Tiempo que se ignora la señal tras detectar una gota (para no contar dos veces la misma) */
#define LED_ON_TIME_MS       200    /*!< Tiempo que queda prendido el LED después de cada gota */

#define MS_TO_SAMPLES(ms)    (((ms) * 1000UL) / SAMPLE_PERIOD_US)
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

/** @brief Tarea encargada de leer el ADC, filtrarlo (según TEST_MODE) y transmitirlo, una vez
 *  por notificación del timer. */
static void ADCTask(void *pvParameter) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        uint16_t voltage_mV;
        AnalogInputReadSingle(ADC_CHANNEL, &voltage_mV);

#if TEST_MODE == MODE_RAW
        printf("%u\r\n", voltage_mV);
#elif TEST_MODE == MODE_FILTERED
        /* LowPassFilter/HiPassFilter guardan el estado internamente entre llamadas, así que
         * llamarlos con signal_lenght=1 (una muestra por vez) funciona como filtro en tiempo
         * real. El pasaaltos va después del pasabajos y saca la continua - la salida queda
         * centrada en 0 (con la gota como pico positivo o negativo), no es más una tensión
         * absoluta.
         *
         * @note Se imprime con decimales (%.2f) a propósito: después de sacar la continua, lo
         * que queda puede ser de apenas unos pocos mV (o menos) de amplitud. Redondear a
         * entero ahí destruye la resolución justo donde más importa. */
        float raw_mV = (float)voltage_mV;
        float lowpassed_mV, filtered_mV;
        LowPassFilter(&raw_mV, &lowpassed_mV, 1);
        HiPassFilter(&lowpassed_mV, &filtered_mV, 1);

        printf("%.2f\r\n", filtered_mV);
#elif TEST_MODE == MODE_DETECTOR
        static uint32_t led_samples_left = 0;

        float raw_mV = (float)voltage_mV;
        float lowpassed_mV, filtered_mV;
        LowPassFilter(&raw_mV, &lowpassed_mV, 1);
        HiPassFilter(&lowpassed_mV, &filtered_mV, 1);

        if (XCorrProcess(filtered_mV, NULL)) {
            LedOn(DETECTION_LED);
            led_samples_left = MS_TO_SAMPLES(LED_ON_TIME_MS);
        } else if (led_samples_left > 0 && --led_samples_left == 0) {
            LedOff(DETECTION_LED);
        }

        /* Igual que MODE_FILTERED: se sigue transmitiendo la señal filtrada muestra a muestra (para
         * el serial plotter). Va después del detector para que el LED no espere al printf. No se
         * imprime ningún texto extra: rompería el formato de una columna numérica del plotter. */
        printf("%.2f\r\n", filtered_mV);
#endif
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

#if TEST_MODE == MODE_FILTERED || TEST_MODE == MODE_DETECTOR
    LowPassInit(1000000.0f / SAMPLE_PERIOD_US, CUTOFF_LOWPASS_HZ, FILTER_ORDER);
    HiPassInit(1000000.0f / SAMPLE_PERIOD_US, CUTOFF_HIGHPASS_HZ, FILTER_ORDER);
#endif

#if TEST_MODE == MODE_DETECTOR
    LedsInit();

    xcorr_config_t xcorr_config = {
        .template_signal = DROP_TEMPLATE,
        .template_len = DROP_TEMPLATE_LEN,
        .threshold = XCORR_THRESHOLD,
        .peak_window = MS_TO_SAMPLES(XCORR_PEAK_WINDOW_MS),
        .refractory = MS_TO_SAMPLES(XCORR_REFRACTORY_MS),
    };
    XCorrInit(&xcorr_config);
#endif

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
