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
 * coma ("canal1,canal2"), formato que el serial plotter grafica como dos series. Tiene 4 modos
 * seleccionables por TEST_MODE:
 *
 * - MODE_RAW: transmite los valores calibrados en mV, sin filtrar.
 * - MODE_FILTERED: pasa cada canal por un pasabanda Butterworth de 4° orden (pasaaltos +
 *   pasabajos en cascada) antes de transmitirlo. La salida queda centrada en 0, sin la
 *   continua. Los cortes (20-260 Hz) salen del espectro de energía de la gota promedio y de
 *   probar pasabandas con registros reales (ver ../analisis_bpw34): por debajo de ~17 Hz queda
 *   solo el 5% de la energía de la gota y por debajo de ~255 Hz, el 95%. El pasaaltos saca la
 *   continua, la deriva lenta (luz ambiente) y los saltos del nivel base al mover el dispositivo;
 *   el pasabajos saca el ruido blanco entre el corte y Nyquist (~417 Hz). Ver los comentarios de
 *   CUTOFF_HIGHPASS_HZ y CUTOFF_LOWPASS_HZ.
 * - MODE_DETECTOR: además del pasabanda, pasa la señal filtrada de cada canal por un detector de
 *   correlación cruzada (componente signal_processing/xcorr_detector) con la plantilla de la gota
 *   de ese canal (drop_template.h, el promedio de 101 gotas del canal 1 y 126 del canal 2,
 *   reales y filtradas). Transmite la señal filtrada de los dos canales y, como tercer valor de la línea ("canal1,canal2,deteccion"), un
 *   0 normalmente y DETECTION_VALUE (1000) en la muestra en que se detecta una gota, para que el
 *   serial plotter la muestre como una tercera serie. Una gota se avisa **una sola vez**: si la
 *   detecta solo el canal 1, solo el canal 2 o los dos (que disparan con unos 5 ms de diferencia),
 *   se transmite una única detección; el aviso del otro canal se ignora durante
 *   XCORR_REFRACTORY_MS.
 * - MODE_ANGLE: detecta las gotas igual que MODE_DETECTOR y transmite "deteccion,0,0,pitch": la
 *   detección (ANGLE_DETECTION_VALUE al detectar una gota, 0 el resto), dos columnas fijas en 0
 *   y el pitch del acelerómetro MPU-6000 / GY-521 (I2C) en grados con un decimal. Mismo formato
 *   que MODE_ANGLE de prueba_opt101, así el análisis (../analisis_opt101/analizar_angulo.py) sirve
 *   para los dos. Sirve para ver a qué ángulo se dejan de detectar las gotas al inclinar el
 *   dispositivo. Se imprime una línea cada PLOT_DECIMATION muestras (~10 ms); si alguna de esas
 *   muestras detectó una gota, la línea sale con la detección. El detector corre con todas las
 *   muestras. ADCTask no imprime: arma las líneas y se las pasa por una cola a PlotTask, y una
 *   tercera tarea (AngleTask) lee el MPU por I2C y actualiza el pitch cada 50 ms.
 *
 * @note MPU / GY-521 (solo en MODE_ANGLE): VCC a 3V3, GND a GND, SCL a GPIO_7 y SDA a GPIO_21.
 * Ver ../prueba_mpu6000/README.md.
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
 * | 07/10/2026 | MODE_DETECTOR: el aviso de gota pasa del LED a un tercer valor en la línea (1000 al detectar, 0 si no) y los umbrales suben a 300 mV para reducir el ruido |
 * | 14/10/2026 | CUTOFF_HIGHPASS_HZ sube de 5 a 20 Hz según el análisis de los registros crudos nuevos (../analisis_bpw34/analizar_crudo_bpw34.py) |
 * | 14/10/2026 | Plantillas (drop_template.h, ahora de 37 muestras) y umbrales regenerados con registros filtrados nuevos (pasabanda 20-260Hz): umbrales 160 (canal 1) y 140 (canal 2), antes 300. Emulado sobre los registros: 68/68, 70/71 y 33/33 gotas, 0 falsas |
 * | 14/10/2026 | MODE_DETECTOR: una gota se avisa una sola vez aunque la detecten los dos canales (que disparaban con ~5 ms de diferencia y daban dos picos de 1000 en la línea): tras avisar, se ignora la detección de cualquiera de los dos canales durante XCORR_REFRACTORY_MS |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "timer_mcu.h"
#include "analog_io_mcu.h"
#include "iir_filter.h"
#include "xcorr_detector.h"
#include "i2c_mcu.h"
#include "mpu6050.h"
/*==================[macros and definitions]=================================*/
#define MODE_RAW               0 /*!< Transmite los valores calibrados en mV, sin filtrar */
#define MODE_FILTERED          1 /*!< Pasa cada canal por el pasabanda antes de transmitirlo */
#define MODE_DETECTOR          2 /*!< Detecta la gota por correlación cruzada con la plantilla y la avisa en un tercer valor de la línea */
#define MODE_ANGLE             3 /*!< Detecta la gota como MODE_DETECTOR y transmite solo "deteccion,0,0,pitch" (pitch del MPU) */

#define TEST_MODE              MODE_ANGLE /*!< Modo activo. Cambiar acá para pasar de etapa. */

#define ADC_CHANNEL_BPW34_1  CH2   /*!< Canal ADC de la salida del amplificador del BPW34 n°1 (GPIO_2) */
#define ADC_CHANNEL_BPW34_2  CH3   /*!< Canal ADC de la salida del amplificador del BPW34 n°2 (GPIO_3).
                                        Se evita CH0 (GPIO_0) porque el driver lo comparte con el DAC */
#define SAMPLE_PERIOD_US     1200  /*!< Período fijo entre muestras (de ambos canales), en microsegundos
                                        (~833 Hz). No puede bajar de ~1 ms por el tiempo de impresión
                                        de la línea con los dos valores (ver nota en la descripción) */

#define CUTOFF_HIGHPASS_HZ   20.0f  /*!< Corte del pasaaltos. Con los registros crudos del 14/10/2026 (../analisis_bpw34,
                                        analizar_crudo_bpw34.py) el 5% de la energía de la gota queda por debajo de ~17 Hz
                                        (los dos canales) y solo ~6% por debajo de 20 Hz, así que a 20 Hz casi no se le saca
                                        nada a la gota. Saca la continua, la deriva lenta y los saltos del nivel base (se
                                        ven al mover el dispositivo): probando pasabandas, 10-20 Hz da más margen de
                                        detección que 5 Hz (canal 1: 6.2-6.6 contra 5.2; canal 2: 5.8-6.8 contra 4.0), y
                                        con 40 Hz ya empeora. Antes 5 Hz (sacado de los registros del 23/09) */
#define CUTOFF_LOWPASS_HZ    260.0f /*!< Corte del pasabajos. El 95% de la energía de la gota queda por
                                        debajo de ~245 Hz (canal 1) y ~261 Hz (canal 2); por encima
                                        solo queda ruido blanco hasta Nyquist (~417 Hz) */
#define FILTER_ORDER         ORDER_4 /*!< Orden del Butterworth (ORDER_2, 4, 6 u 8), para ambos filtros */
#define BASELINE_SAMPLES     100   /*!< Muestras que se promedian al arrancar para estimar el nivel base de
                                        cada canal (ver nota en BandPassFilter) */

/* Parámetros de MODE_DETECTOR (los valores salen del análisis de ../analisis_bpw34) */
#define DETECTION_VALUE      1000   /*!< Valor que se transmite en el tercer canal en la muestra en que se detecta una gota (0 el resto del tiempo) */
#define XCORR_THRESHOLD_CH1  160.0f /*!< Umbral de la correlación del canal 1, en mV de la señal filtrada. Es 3 veces el ruido
                                        máximo de la correlación en este canal (54, medido en dos registros sin goteo y en los
                                        tramos sin gota de tres registros con goteo): se prioriza no tener falsas detecciones
                                        (otra PC en el mismo enchufe llegó a disparar el detector). El firmware avisa si
                                        CUALQUIERA de los dos canales detecta, así que una gota débil en un canal no se pierde
                                        si el otro la ve. Con este umbral: 68 de 68 gotas con el dispositivo fijo, 70 de 71
                                        moviéndolo y 33 de 33 en la validación, 0 falsas. Ver
                                        ../analisis_bpw34/analizar_correlacion_bpw34.py (antes 300) */
#define XCORR_THRESHOLD_CH2  140.0f /*!< Umbral de la correlación del canal 2 (mismo criterio que el canal 1: 3 veces su ruido
                                        máximo, 47) */
#define XCORR_PEAK_WINDOW_MS 15     /*!< Tiempo que se espera, tras pasar el umbral, quedándose con el máximo de la
                                        correlación. La plantilla oscila (caída, rebote, caída): la correlación pasa el umbral
                                        unos 6 ms antes de su máximo (13 ms como mucho), así que 15 ms alcanza para llegar al máximo */
#define XCORR_REFRACTORY_MS  100    /*!< Tiempo que se ignora la señal tras detectar una gota (para no contar dos veces la misma) */

#define MS_TO_SAMPLES(ms)    (((ms) * 1000UL) / SAMPLE_PERIOD_US)

/* Parámetros de MODE_ANGLE (mismo formato de salida que MODE_ANGLE de prueba_opt101, para usar el mismo análisis) */
#define ANGLE_DETECTION_VALUE  500   /*!< Valor de la columna 1 en la línea en que se detectó una gota (0 el resto) */
#define ANGLE_UPDATE_MS        50    /*!< Cada cuánto se actualiza el pitch (el ángulo cambia mucho más lento que la señal, así que no hace falta más rápido) */
#define ANGLE_I2C_FREQ_HZ      100000 /*!< Velocidad del I2C del MPU. 100 kHz en vez de los 400 kHz por defecto: tolera mejor cables largos o soldaduras flojas */
#define ANGLE_RETRY_MS         200   /*!< Espera tras una lectura fallida antes de reintentar */
#define ANGLE_FAIL_LIMIT       3     /*!< Lecturas fallidas seguidas tras las que el pitch pasa a NaN */
#define ANGLE_AVG_SAMPLES      8     /*!< Lecturas del acelerómetro que se promedian por cada ángulo, para filtrar vibraciones */
#define ACCEL_LSB_PER_G        16384.0f /*!< Sensibilidad del acelerómetro en +-2g (MPU6050_ACCEL_FS_2) */
#define RAD_TO_DEG             57.29578f
#define PLOT_DECIMATION        8     /*!< Se imprime 1 línea cada N muestras (8 x 1.2ms ~ 10ms, ~100 líneas/s), con la detección "pegada": si hubo gota en las N muestras, sale ANGLE_DETECTION_VALUE en esa línea. La línea "500,0,0,-12.3" ocupa ~15 caracteres, ~1.3ms a 115200 baudios, así que la UART queda con mucho margen */
#define PLOT_QUEUE_LEN         32    /*!< Líneas que puede acumular la cola entre ADCTask y la tarea que imprime. Si se llena, se descartan líneas del plotter (la detección no se ve afectada) */

#define N_CHANNELS           2     /*!< Cantidad de fotodiodos (y de canales del ADC) */
/*==================[internal data definition]===============================*/
static TaskHandle_t adc_task_handle = NULL;  /*!< Handle de ADCTask, para que la interrupción del timer sepa a qué tarea avisar */
static const adc_ch_t adc_channels[N_CHANNELS] = {ADC_CHANNEL_BPW34_1, ADC_CHANNEL_BPW34_2}; /*!< Canal del ADC de cada fotodiodo: posición 0 = BPW34 n°1, posición 1 = BPW34 n°2 */

#if TEST_MODE != MODE_RAW  /* estas variables solo existen en los modos que filtran */
static iir_filter_t lowpass[N_CHANNELS];  /*!< Un pasabajos por canal, cada uno con su propia memoria */
static iir_filter_t highpass[N_CHANNELS]; /*!< Un pasaaltos por canal, cada uno con su propia memoria */
static float baseline_mV[N_CHANNELS];     /*!< Nivel base de cada canal (en mV), medido al arrancar */
#endif

#if TEST_MODE == MODE_DETECTOR || TEST_MODE == MODE_ANGLE  /* estas variables solo existen en los modos con detector */
#include "drop_template.h"          /* plantillas de la gota (solo hacen falta en este modo) */
static xcorr_detector_t detector[N_CHANNELS];  /*!< Un detector por canal, cada uno con su plantilla, su umbral y su propio estado */
static uint32_t merge_refractory_left = 0;     /*!< Muestras que faltan para volver a avisar una gota: evita avisar dos veces la misma si la detectan los dos canales */
static const float *const drop_templates[N_CHANNELS] = {DROP_TEMPLATE_CH1, DROP_TEMPLATE_CH2}; /*!< Plantilla de cada canal (drop_template.h) */
static const float xcorr_thresholds[N_CHANNELS] = {XCORR_THRESHOLD_CH1, XCORR_THRESHOLD_CH2};  /*!< Umbral de cada canal */
#endif

#if TEST_MODE == MODE_ANGLE
/** Una línea del plotter, armada por ADCTask y impresa por PlotTask. */
typedef struct {
    bool drop;          /*!< true si hubo detección en alguna de las muestras que esta línea representa */
} plot_line_t;

static QueueHandle_t plot_queue = NULL;       /*!< Cola de líneas ADCTask -> PlotTask. Así printf (lento) no atrasa el muestreo */
static volatile float pitch_deg = 0.0f;       /*!< Último pitch calculado por AngleTask (float de 32 bits: la lectura es atómica en el ESP32-C3) */
#endif

/*==================[internal functions declaration]=========================*/
static void FuncTimerSample(void *param);
static void ADCTask(void *pvParameter);
#if TEST_MODE == MODE_ANGLE
static void AngleTask(void *pvParameter);
static void PlotTask(void *pvParameter);
#endif

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
 * continua de ~1200 mV a la entrada y un corte tan bajo (20 Hz contra fs ≈ 833 Hz), el estado
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
        /* Modo detector: filtra cada canal, lo pasa por su detector y transmite "canal1,canal2,deteccion":
         * las dos señales filtradas y un tercer valor que es DETECTION_VALUE en la muestra en que se
         * detecta una gota y 0 el resto del tiempo.
         *
         * Cada canal detecta por su cuenta. Una gota se avisa UNA sola vez, sin importar qué canal la
         * vea: solo el canal 1, solo el canal 2, o los dos (que disparan con ~5 ms de diferencia, cada
         * uno en su muestra). Por eso, tras avisar una gota, se ignora cualquier detección de
         * cualquiera de los dos canales durante XCORR_REFRACTORY_MS. */
        long filtered_mV[N_CHANNELS];             // salida filtrada de cada canal, en mV enteros
        bool drop_detected = false;               // true en la muestra en que se avisa una gota
        for (int ch = 0; ch < N_CHANNELS; ch++) {
            float filtered = BandPassFilter(ch, (float)value_mV[ch]);           // pasabanda de este canal
            bool channel_detected = XCorrDetectorProcess(&detector[ch], filtered, NULL);  // correlación con la plantilla de este canal
            if (channel_detected && merge_refractory_left == 0) {
                drop_detected = true;                                           // primera detección de esta gota, de cualquiera de los dos canales
                merge_refractory_left = MS_TO_SAMPLES(XCORR_REFRACTORY_MS);     // a partir de ahora ignora el aviso del otro canal
            }
            filtered_mV[ch] = lroundf(filtered);                                // para imprimir, redondeado al mV
        }
        if (merge_refractory_left > 0 && !drop_detected) {
            merge_refractory_left--;                                            // cuenta el tiempo muerto (no en la muestra en que se avisó)
        }

        printf("%ld,%ld,%d\r\n", filtered_mV[0], filtered_mV[1], drop_detected ? DETECTION_VALUE : 0);
#elif TEST_MODE == MODE_ANGLE
        /* Igual que MODE_DETECTOR (cada canal detecta por su cuenta y una gota se avisa una sola vez), pero
         * no se imprime acá: ADCTask arma una línea cada PLOT_DECIMATION muestras y se la pasa a PlotTask
         * por una cola, así printf (lento a 115200 baudios) nunca atrasa el muestreo ni el detector. La
         * detección queda "pegada": si alguna de las muestras salteadas detectó una gota, la línea sale
         * con la detección. */
        static uint8_t decim_count = 0;
        static bool drop_pending = false;

        bool drop_detected = false;               // true en la muestra en que se avisa una gota
        for (int ch = 0; ch < N_CHANNELS; ch++) {
            float filtered = BandPassFilter(ch, (float)value_mV[ch]);
            bool channel_detected = XCorrDetectorProcess(&detector[ch], filtered, NULL);
            if (channel_detected && merge_refractory_left == 0) {
                drop_detected = true;
                merge_refractory_left = MS_TO_SAMPLES(XCORR_REFRACTORY_MS);
            }
        }
        if (merge_refractory_left > 0 && !drop_detected) {
            merge_refractory_left--;
        }
        if (drop_detected) {
            drop_pending = true;
        }

        if (++decim_count >= PLOT_DECIMATION) {
            decim_count = 0;
            plot_line_t line = {.drop = drop_pending};
            drop_pending = false;
            xQueueSend(plot_queue, &line, 0);   /* sin esperar: si la cola está llena se pierde la línea del plotter, no la detección */
        }
#endif
    }
}

#if TEST_MODE == MODE_ANGLE
/** @brief Tarea que lee el acelerómetro cada ANGLE_UPDATE_MS y guarda el pitch (en grados, con
 *  signo) en pitch_deg. Corre con menos prioridad que ADCTask, así la lectura por I2C no atrasa
 *  el muestreo del ADC. */
static void AngleTask(void *pvParameter) {
    if (!I2C_initialize(ANGLE_I2C_FREQ_HZ)) {
        printf("Error: no se pudo inicializar el bus I2C (SDA=GPIO_21, SCL=GPIO_7)\r\n");
        vTaskDelete(NULL);
    }
    MPU6050_initialize();
    if (!MPU6050_testConnection()) {
        printf("Error: el MPU no responde. Revisar SDA/SCL, VCC y GND.\r\n");
        vTaskDelete(NULL);
    }

    uint8_t failures = 0;   /* lecturas fallidas seguidas */
    while (true) {
        int32_t sx = 0, sy = 0, sz = 0;
        bool read_ok = true;
        for (int i = 0; i < ANGLE_AVG_SAMPLES && read_ok; i++) {
            uint8_t raw[6];
            /* Se lee directo con I2C_readBytes (en vez de MPU6050_getAcceleration) para poder ver
             * si la lectura falló: devuelve la cantidad de bytes leídos, o 0 si hubo error. */
            if (I2C_readBytes(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_ACCEL_XOUT_H, 6, raw, I2C_MASTER_TIMEOUT_MS) != 6) {
                read_ok = false;
                break;
            }
            sx += (int16_t)((raw[0] << 8) | raw[1]);
            sy += (int16_t)((raw[2] << 8) | raw[3]);
            sz += (int16_t)((raw[4] << 8) | raw[5]);
            vTaskDelay(pdMS_TO_TICKS(2));
        }

        if (read_ok) {
            failures = 0;
            float ax = sx / (float)ANGLE_AVG_SAMPLES / ACCEL_LSB_PER_G;
            float ay = sy / (float)ANGLE_AVG_SAMPLES / ACCEL_LSB_PER_G;
            float az = sz / (float)ANGLE_AVG_SAMPLES / ACCEL_LSB_PER_G;
            pitch_deg = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
            vTaskDelay(pdMS_TO_TICKS(ANGLE_UPDATE_MS));
        } else {
            /* Falló la lectura (NACK, cable flojo, etc.): no se reintenta enseguida, se espera para no
             * martillar un bus que puede estar en mal estado. Tras varias seguidas el pitch pasa a NaN
             * para que no se confunda un valor viejo con uno actual. */
            if (failures < 255) failures++;
            if (failures >= ANGLE_FAIL_LIMIT) pitch_deg = NAN;
            vTaskDelay(pdMS_TO_TICKS(ANGLE_RETRY_MS));
        }
    }
}

/** @brief Tarea que imprime las líneas que le pasa ADCTask, agregando el pitch más reciente.
 *  Formato (el serial plotter grafica una serie por columna): "deteccion,0,0,pitch", con la
 *  detección en ANGLE_DETECTION_VALUE o 0 y el pitch en grados con un decimal. */
static void PlotTask(void *pvParameter) {
    plot_line_t line;
    while (true) {
        xQueueReceive(plot_queue, &line, portMAX_DELAY);
        printf("%d,0,0,%.1f\r\n", line.drop ? ANGLE_DETECTION_VALUE : 0, pitch_deg);
    }
}
#endif

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

#if TEST_MODE == MODE_DETECTOR || TEST_MODE == MODE_ANGLE
    for (int ch = 0; ch < N_CHANNELS; ch++) {   // un detector por canal
        xcorr_config_t xcorr_config = {
            .template_signal = drop_templates[ch],                   // plantilla de este canal
            .template_len = DROP_TEMPLATE_LEN,                       // 37 muestras (-15 a +30 ms)
            .threshold = xcorr_thresholds[ch],                       // umbral de este canal
            .peak_window = MS_TO_SAMPLES(XCORR_PEAK_WINDOW_MS),      // ventana para quedarse con el máximo
            .refractory = MS_TO_SAMPLES(XCORR_REFRACTORY_MS),        // tiempo muerto después de cada gota
        };
        XCorrDetectorInit(&detector[ch], &xcorr_config);            // normaliza la plantilla y pone el estado en 0
    }
#endif

#if TEST_MODE == MODE_ANGLE
    plot_queue = xQueueCreate(PLOT_QUEUE_LEN, sizeof(plot_line_t));   /* tiene que existir antes de que ADCTask empiece a usarla */
    xTaskCreate(&AngleTask, "MPU_ANGLE", 4096, NULL, 3, NULL);
    /* PlotTask con la prioridad de IDLE (0): printf espera activamente a la salida serie sin ceder
     * la CPU, y si el monitor del PC se atrasa esa espera puede durar segundos. Con prioridad mayor
     * IDLE no llegaba a correr y saltaba el watchdog de tareas; en 0 comparte el tiempo con IDLE. */
    xTaskCreate(&PlotTask, "PLOT", 4096, NULL, tskIDLE_PRIORITY, NULL);
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
