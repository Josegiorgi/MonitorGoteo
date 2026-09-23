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
 * Lee los dos canales con un período fijo y transmite ambos valores en una línea separados por
 * coma ("canal1,canal2"), formato que el serial plotter grafica como dos series. Tiene 3 modos
 * seleccionables por TEST_MODE:
 *
 * - MODE_RAW: transmite los valores calibrados en mV, sin filtrar.
 * - MODE_FILTERED: pasa cada canal por un pasabanda Butterworth de 4° orden (pasaaltos +
 *   pasabajos en cascada) antes de transmitirlo. La salida queda centrada en 0, sin la
 *   continua. Los cortes (~5-260 Hz) salen del espectro de energía de la gota promedio (ver
 *   ../analisis_bpw34): por debajo de ~6-9 Hz queda solo el 5% de la energía de la gota y por
 *   debajo de ~245-261 Hz, el 95%. El pasaaltos saca la continua y la deriva lenta (luz
 *   ambiente); el pasabajos saca el ruido blanco entre el corte y Nyquist (~417 Hz).
 * - MODE_DETECTOR: además del pasabanda, pasa la señal filtrada de cada canal por un detector de
 *   correlación cruzada (componente signal_processing/xcorr_detector) con la plantilla de la gota
 *   de ese canal (drop_template.h, el promedio de 7 gotas reales filtradas). Cuando cualquiera de
 *   los dos canales detecta una gota, se prende un LED (DETECTION_LED) durante LED_ON_TIME_MS.
 *   Sigue transmitiendo la señal filtrada de los dos canales, igual que MODE_FILTERED.
 *
 * @note Igual que en prueba_opt101, se usa timer_mcu en vez de vTaskDelay: la interrupción del
 * timer solo notifica a ADCTask, y la lectura del ADC, el filtrado y el printf se hacen en la
 * tarea.
 *
 * @note SAMPLE_PERIOD_US tiene que quedar por encima de lo que tarda imprimir una línea por la
 * UART de la consola (115200 baudios, ~87 us por caracter). Con dos valores de hasta 4 dígitos
 * (con signo en MODE_FILTERED) la línea ocupa hasta 11 caracteres (~960 us), o sea que el
 * período no puede bajar de ~1 ms sin que ADCTask se atrase y termine disparando el watchdog.
 *
 * @note El filtrado usa las instancias de filtro (iir_filter_t) del componente iir_filter de
 * middleware: cada canal tiene su propio pasabajos y su propio pasaaltos, con su propia memoria,
 * así que los dos canales se filtran por separado. (Las funciones LowPassFilter()/HiPassFilter()
 * de ese componente tienen un único filtro interno y no sirven para dos canales a la vez.) Por la
 * misma razón, el detector usa las instancias (xcorr_detector_t) del componente xcorr_detector:
 * un detector por canal, cada uno con su plantilla, su umbral y su propio estado.
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
 * | LED indicador de gota (solo MODE_DETECTOR, con R serie a GND) | GPIO_10 (LED_2) |
 *
 * @note Entre la salida de cada amplificador y su GPIO va un antialias RC (4.7 kΩ en serie y
 * 100 nF a GND, corte ≈ 339 Hz, por debajo de Nyquist). Las plantillas de drop_template.h se
 * tomaron con ese RC puesto.
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
 * | 23/09/2026 | Se agrega MODE_FILTERED: pasabanda Butterworth de 4° orden por canal, con cortes de 5 Hz y 260 Hz sacados del espectro de energía de la gota (../analisis_bpw34) |
 * | 23/09/2026 | Se agrega MODE_DETECTOR: detección de gotas por correlación cruzada con una plantilla por canal (drop_template.h) y aviso con un LED |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "timer_mcu.h"
#include "analog_io_mcu.h"
#include "iir_filter.h"
#include "xcorr_detector.h"
#include "led.h"
/*==================[macros and definitions]=================================*/
#define MODE_RAW               0 /*!< Transmite los valores calibrados en mV, sin filtrar */
#define MODE_FILTERED          1 /*!< Pasa cada canal por el pasabanda antes de transmitirlo */
#define MODE_DETECTOR          2 /*!< Detecta la gota por correlación cruzada con la plantilla y prende un LED */

#define TEST_MODE              MODE_DETECTOR /*!< Modo activo. Cambiar acá para pasar de etapa. */

#define ADC_CHANNEL_BPW34_1  CH2   /*!< Canal ADC de la salida del amplificador del BPW34 n°1 (GPIO_2) */
#define ADC_CHANNEL_BPW34_2  CH3   /*!< Canal ADC de la salida del amplificador del BPW34 n°2 (GPIO_3).
                                        Se evita CH0 (GPIO_0) porque el driver lo comparte con el DAC */
#define SAMPLE_PERIOD_US     1200  /*!< Período fijo entre muestras (de ambos canales), en microsegundos
                                        (~833 Hz). No puede bajar de ~1 ms por el tiempo de impresión
                                        de la línea con los dos valores (ver nota en la descripción) */

#define CUTOFF_HIGHPASS_HZ   5.0f   /*!< Corte del pasaaltos. En el espectro de energía de la gota
                                        promedio (../analisis_bpw34), el 5% de la energía queda por
                                        debajo de ~6 Hz (canal 1) y ~9 Hz (canal 2): a 5 Hz el filtro
                                        saca la continua y la deriva lenta sin tocar casi nada de la gota */
#define CUTOFF_LOWPASS_HZ    260.0f /*!< Corte del pasabajos. El 95% de la energía de la gota queda por
                                        debajo de ~245 Hz (canal 1) y ~261 Hz (canal 2); por encima
                                        solo queda ruido blanco hasta Nyquist (~417 Hz) */
#define FILTER_ORDER         ORDER_4 /*!< Orden del Butterworth (ORDER_2, 4, 6 u 8), para ambos filtros */
#define BASELINE_SAMPLES     100   /*!< Muestras que se promedian al arrancar para estimar el nivel base de
                                        cada canal (ver nota en BandPassFilter) */

/* Parámetros de MODE_DETECTOR (los valores salen del análisis de ../analisis_bpw34) */
#define DETECTION_LED        LED_2  /*!< LED que se prende al detectar una gota (LED_2 = GPIO_10; LED_3 = GPIO_5; evitar LED_1 = GPIO_20, que es el RX de la UART de la consola) */
#define XCORR_THRESHOLD_CH1  59.7f  /*!< Umbral de la correlación del canal 1, en mV de la señal filtrada: 12 veces el desvío de
                                        la correlación sobre la señal filtrada SIN goteo (el ruido llega a ~3.9 desvíos; la gota
                                        más chica da ~14 veces el umbral) */
#define XCORR_THRESHOLD_CH2  51.4f  /*!< Umbral de la correlación del canal 2 (mismo criterio que el canal 1) */
#define XCORR_PEAK_WINDOW_MS 15     /*!< Tiempo que se espera, tras pasar el umbral, quedándose con el máximo de la
                                        correlación. La plantilla oscila (caída, rebote, caída) y su correlación tiene
                                        lóbulos laterales a unos milisegundos del pico principal que también pueden pasar el umbral */
#define XCORR_REFRACTORY_MS  100    /*!< Tiempo que se ignora la señal tras detectar una gota (para no contar dos veces la misma) */
#define LED_ON_TIME_MS       200    /*!< Tiempo que queda prendido el LED después de cada gota */

#define MS_TO_SAMPLES(ms)    (((ms) * 1000UL) / SAMPLE_PERIOD_US)

#define N_CHANNELS           2     /*!< Cantidad de fotodiodos (y de canales del ADC) */
/*==================[internal data definition]===============================*/
static TaskHandle_t adc_task_handle = NULL;  /*!< Handle de ADCTask, para que la interrupción del timer sepa a qué tarea avisar */
static const adc_ch_t adc_channels[N_CHANNELS] = {ADC_CHANNEL_BPW34_1, ADC_CHANNEL_BPW34_2}; /*!< Canal del ADC de cada fotodiodo: posición 0 = BPW34 n°1, posición 1 = BPW34 n°2 */

#if TEST_MODE != MODE_RAW  /* estas variables solo existen en los modos que filtran */
static iir_filter_t lowpass[N_CHANNELS];  /*!< Un pasabajos por canal, cada uno con su propia memoria */
static iir_filter_t highpass[N_CHANNELS]; /*!< Un pasaaltos por canal, cada uno con su propia memoria */
static float baseline_mV[N_CHANNELS];     /*!< Nivel base de cada canal (en mV), medido al arrancar */
#endif

#if TEST_MODE == MODE_DETECTOR  /* estas variables solo existen en el modo detector */
#include "drop_template.h"          /* plantillas de la gota (solo hacen falta en este modo) */
static xcorr_detector_t detector[N_CHANNELS];  /*!< Un detector por canal, cada uno con su plantilla, su umbral y su propio estado */
static const float *const drop_templates[N_CHANNELS] = {DROP_TEMPLATE_CH1, DROP_TEMPLATE_CH2}; /*!< Plantilla de cada canal (drop_template.h) */
static const float xcorr_thresholds[N_CHANNELS] = {XCORR_THRESHOLD_CH1, XCORR_THRESHOLD_CH2};  /*!< Umbral de cada canal */
#endif

/*==================[internal functions declaration]=========================*/
static void FuncTimerSample(void *param);
static void ADCTask(void *pvParameter);

/*==================[internal functions definition]===========================*/

/** @brief Función invocada en la interrupción del timer. Solo notifica a ADCTask. */
static void FuncTimerSample(void *param) {
    vTaskNotifyGiveFromISR(adc_task_handle, pdFALSE);  // "despierta" a ADCTask para que tome una muestra
}

#if TEST_MODE != MODE_RAW  /* estas funciones solo se compilan en los modos que filtran */
/** @brief Inicializa el pasabajos y el pasaaltos de cada canal y estima el nivel base de cada
 *  canal promediando BASELINE_SAMPLES lecturas. */
static void BandPassInit(void) {
    float fs = 1000000.0f / SAMPLE_PERIOD_US;  // frecuencia de muestreo en Hz (1200 us -> ~833 Hz)
    for (int ch = 0; ch < N_CHANNELS; ch++) {  // se repite todo para cada canal
        IirLowPassInit(&lowpass[ch], fs, CUTOFF_LOWPASS_HZ, FILTER_ORDER);    // calcula los coeficientes del pasabajos de este canal y pone su memoria en 0
        IirHiPassInit(&highpass[ch], fs, CUTOFF_HIGHPASS_HZ, FILTER_ORDER);   // lo mismo para el pasaaltos

        float sum = 0;                                  // acumulador para promediar
        for (int i = 0; i < BASELINE_SAMPLES; i++) {    // lee BASELINE_SAMPLES veces seguidas (sin gota, con el haz libre)
            uint16_t value_mV;
            AnalogInputReadSingle(adc_channels[ch], &value_mV);  // una lectura del ADC, ya calibrada en mV
            sum += value_mV;
        }
        baseline_mV[ch] = sum / BASELINE_SAMPLES;       // el promedio es el nivel base de este canal
    }
}

/** @brief Filtra una muestra de un canal (pasabajos y después pasaaltos, igual que prueba_opt101).
 *
 * @note Antes de filtrar se resta el nivel base medido al arrancar. El pasaaltos lo sacaría igual,
 * pero los biquads de esp-dsp (que iir_filter usa por dentro) son de forma directa II: con una
 * continua de ~1200 mV a la entrada y un corte tan bajo (5 Hz contra fs ≈ 833 Hz), el estado
 * interno crece a cientos de miles y, en float, la resta entre esos números grandes pierde
 * resolución. Simulando en float de 32 bits con un registro real, el error baja de ~0.13 mV rms
 * (picos de ~3 mV) a ~0.001 mV al restar el nivel base. La deriva que quede después de eso la
 * saca el pasaaltos. */
static float BandPassFilter(int ch, float value_mV) {
    float x = value_mV - baseline_mV[ch];   // resta el nivel base: la señal queda cerca de 0
    IirFilter(&lowpass[ch], &x, &x, 1);     // pasabajos de este canal, una muestra (el resultado se guarda en la misma x)
    IirFilter(&highpass[ch], &x, &x, 1);    // pasaaltos de este canal, sobre la salida del pasabajos
    return x;                               // muestra filtrada, en mV
}
#endif

/** @brief Tarea encargada de leer los dos canales del ADC, filtrarlos (según TEST_MODE) y
 *  transmitirlos, una vez por notificación del timer. */
static void ADCTask(void *pvParameter) {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // espera (sin gastar CPU) a que el timer avise que toca muestrear

        uint16_t value_mV[N_CHANNELS];            // lectura cruda de cada canal, en mV
        for (int ch = 0; ch < N_CHANNELS; ch++) {
            AnalogInputReadSingle(adc_channels[ch], &value_mV[ch]);  // lee el canal ch del ADC, ya calibrado en mV
        }

#if TEST_MODE == MODE_RAW
        /* Modo sin filtrado: transmite las lecturas tal cual, "canal1,canal2" */
        printf("%u,%u\r\n", value_mV[0], value_mV[1]);
#elif TEST_MODE == MODE_FILTERED
        /* Modo filtrado: pasa cada canal por su pasabanda y transmite "canal1,canal2" filtrados.
         * Se imprime redondeado a mV entero (a diferencia del OPT101, que necesitaba decimales):
         * la gota mueve la señal cientos de mV y el ruido es de unos pocos mV, así que un decimal no
         * aporta nada y alargaría la línea más allá de lo que entra en SAMPLE_PERIOD_US. */
        long filtered_mV[N_CHANNELS];             // salida filtrada de cada canal, en mV enteros
        for (int ch = 0; ch < N_CHANNELS; ch++) {
            filtered_mV[ch] = lroundf(BandPassFilter(ch, (float)value_mV[ch]));  // filtra y redondea al mV más cercano
        }
        printf("%ld,%ld\r\n", filtered_mV[0], filtered_mV[1]);
#elif TEST_MODE == MODE_DETECTOR
        /* Modo detector: filtra cada canal, lo pasa por su detector y prende el LED si cualquiera de
         * los dos canales detecta una gota. Sigue transmitiendo "canal1,canal2" filtrados, igual que
         * MODE_FILTERED (sin texto extra, que rompería el formato del serial plotter). */
        static uint32_t led_samples_left = 0;     // muestras que le quedan prendido al LED (static: se conserva entre vueltas)

        long filtered_mV[N_CHANNELS];             // salida filtrada de cada canal, en mV enteros
        bool drop_detected = false;               // true si algún canal detectó una gota en esta muestra
        for (int ch = 0; ch < N_CHANNELS; ch++) {
            float filtered = BandPassFilter(ch, (float)value_mV[ch]);           // pasabanda de este canal
            if (XCorrDetectorProcess(&detector[ch], filtered, NULL)) {          // correlación con la plantilla de este canal
                drop_detected = true;                                           // este canal detectó una gota
            }
            filtered_mV[ch] = lroundf(filtered);                                // para imprimir, redondeado al mV
        }

        if (drop_detected) {
            LedOn(DETECTION_LED);                              // gota: prende el LED...
            led_samples_left = MS_TO_SAMPLES(LED_ON_TIME_MS);  // ...y (re)arranca la cuenta de cuánto tiempo queda prendido
        } else if (led_samples_left > 0 && --led_samples_left == 0) {
            LedOff(DETECTION_LED);                             // se cumplió LED_ON_TIME_MS sin otra gota: lo apaga
        }

        /* Va después del detector para que el LED no espere al printf */
        printf("%ld,%ld\r\n", filtered_mV[0], filtered_mV[1]);
#endif
    }
}

/*==================[external functions definition]==========================*/
void app_main(void) {
    for (int ch = 0; ch < N_CHANNELS; ch++) {   // configura los dos canales del ADC
        analog_input_config_t adc_config = {
            .input = adc_channels[ch],          // canal a configurar (CH2 o CH3)
            .mode = ADC_SINGLE,                 // lectura puntual, una muestra por pedido
            .func_p = NULL,                     // sin callback (solo se usa en modo continuo)
            .param_p = NULL,
        };
        AnalogInputInit(&adc_config);
    }

#if TEST_MODE != MODE_RAW
    BandPassInit();                             // arma los filtros y mide el nivel base (en los modos que filtran)
#endif

#if TEST_MODE == MODE_DETECTOR
    LedsInit();                                 // configura los GPIO de los LEDs de la placa
    for (int ch = 0; ch < N_CHANNELS; ch++) {   // un detector por canal
        xcorr_config_t xcorr_config = {
            .template_signal = drop_templates[ch],                   // plantilla de este canal
            .template_len = DROP_TEMPLATE_LEN,                       // 32 muestras (±20 ms)
            .threshold = xcorr_thresholds[ch],                       // umbral de este canal
            .peak_window = MS_TO_SAMPLES(XCORR_PEAK_WINDOW_MS),      // ventana para quedarse con el máximo
            .refractory = MS_TO_SAMPLES(XCORR_REFRACTORY_MS),        // tiempo muerto después de cada gota
        };
        XCorrDetectorInit(&detector[ch], &xcorr_config);            // normaliza la plantilla y pone el estado en 0
    }
#endif

    xTaskCreate(&ADCTask, "BPW34_ADC", 4096, NULL, 5, &adc_task_handle);  // crea la tarea que lee, filtra e imprime

    timer_config_t sample_timer = {
        .timer = TIMER_A,
        .period = SAMPLE_PERIOD_US,             // el timer interrumpe cada SAMPLE_PERIOD_US microsegundos
        .func_p = FuncTimerSample,              // función que se ejecuta en cada interrupción
        .param_p = NULL,
    };
    TimerInit(&sample_timer);
    TimerStart(TIMER_A);                        // a partir de acá arranca el muestreo
}
/*==================[end of file]============================================*/
