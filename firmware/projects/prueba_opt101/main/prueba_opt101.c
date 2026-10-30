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
 *   tensión absoluta - queda centrada en 0, sin la continua. El corte del pasabanda (60-300Hz)
 *   sale de un análisis real con el OPT101 en 100k/3.9nF (ver ../analisis_opt101): el 88% de la
 *   energía de la gota está entre 85 y 340Hz, y entre 10 y 60Hz de pasaaltos con 300Hz de
 *   pasabajos la detectabilidad es la misma. Ver los comentarios de CUTOFF_HIGHPASS_HZ y
 *   CUTOFF_LOWPASS_HZ.
 * - MODE_DETECTOR: además del pasabanda, pasa la señal filtrada por el detector de correlación
 *   cruzada (componente signal_processing/xcorr_detector) con la plantilla de la gota
 *   (drop_template.h, el promedio de 35 gotas reales filtradas). Cuando la correlación pasa el umbral y
 *   se confirma el pico, se transmite DETECTION_VALUE (500) como segundo valor de la línea
 *   ("filtrada,deteccion"; 0 el resto del tiempo), para que el serial plotter lo grafique como
 *   una segunda serie, igual que en pruebabpw34.
 * - MODE_ANGLE: detecta las gotas igual que MODE_DETECTOR y transmite "deteccion,0,0,pitch": la
 *   detección (DETECTION_VALUE al detectar una gota, 0 el resto), dos columnas fijas en 0 y el pitch del acelerómetro
 *   ADXL345 (I2C) en grados con un decimal. Sirve para ver a qué ángulo se dejan de
 *   detectar las gotas al inclinar el dispositivo. Se imprime una línea cada PLOT_DECIMATION
 *   muestras (~10ms); si alguna de esas muestras detectó una gota, la línea sale con la detección.
 *   El detector corre con todas las muestras.
 *
 * @note En MODE_ANGLE, ADCTask no imprime: arma las líneas y se las pasa por una cola a PlotTask,
 * y una tercera tarea (AngleTask) lee el ADXL345 por I2C y actualiza el pitch cada 50ms. Así ni
 * el printf ni el I2C atrasan el muestreo del ADC.
 *
 * @note ADXL345 (solo en los modos de ángulo): SCL a GPIO_7, SDA a GPIO_21, GND a GND y
 * alimentación a 3V3. El pin CS va a 3V3 (modo I2C) y SDO a GND (dirección 0x53); SDO no puede
 * quedar sin conectar. Para ver los ejes y ángulos con el dispositivo colocado, ver ../prueba_adxl345.
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
 * | 08/10/2026 | MODE_DETECTOR: el aviso de gota pasa del LED a un segundo valor en la línea (500 al detectar, 0 si no), como en pruebabpw34. La señal filtrada se imprime con 1 decimal para que la línea entre en SAMPLE_PERIOD_US a 115200 baudios |
 * | 09/10/2026 | Con los registros nuevos del OPT101 (Rf 100k, C 3.9nF, antialias 4k7+100nF, LED con 1k2) el pasabanda pasa de 85-195Hz a 60-300Hz, ver la justificación en CUTOFF_HIGHPASS_HZ/CUTOFF_LOWPASS_HZ. OJO: drop_template.h y XCORR_THRESHOLD siguen siendo de los datos anteriores, hay que regenerarlos con un registro filtrado nuevo |
 * | 13/10/2026 | Se saca el notch en 100Hz (MODE_FILTERED_NOTCH, DETECTOR_USE_NOTCH e IirNotchInit): aplicado por software a las mismas gotas no mejora la detección (el margen de la correlación queda igual, 18.7 vs 18.6 con Q=8) y deforma la gota (con Q=8 pierde 17% de amplitud y la forma queda a 0.93; con Q=4, 30% y 0.86), porque el zumbido de 100Hz ya es chico con este montaje. MODE_DETECTOR vuelve a ser el modo 2. Se conserva FilterSample() |
 * | 13/10/2026 | Plantilla (drop_template.h) y XCORR_THRESHOLD regenerados con registros filtrados nuevos (pasabanda 60-300Hz, sin notch): plantilla de 63 muestras (promedio de 35 gotas de FiltradaconGota.csv), umbral 110 (antes 50.7). XCORR_PEAK_WINDOW_MS sube de 15 a 25ms porque la plantilla nueva es más larga. Validado con una toma anterior: 13 de 13 gotas |
 *
 * @author Josefina Giorgi (josefina.giorgi@ingenieriauner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "timer_mcu.h"
#include "analog_io_mcu.h"
#include "iir_filter.h"
#include "xcorr_detector.h"
#include "drop_template.h"
#include <math.h>
#include "i2c_mcu.h"
#include "adxl345.h"
/*==================[macros and definitions]=================================*/
#define MODE_RAW               0 /*!< Transmite el valor calibrado en mV, sin filtrar */
#define MODE_FILTERED          1 /*!< Pasa la lectura por el filtro pasabanda antes de transmitirla */
#define MODE_DETECTOR          2 /*!< Detecta la gota por correlación cruzada con la plantilla y lo avisa en un segundo valor de la línea */
#define MODE_ANGLE             3 /*!< Detecta la gota como MODE_DETECTOR y transmite "deteccion,0,0,pitch" (pitch del acelerómetro) */

#define TEST_MODE              MODE_ANGLE /*!< Modo activo. Cambiar acá para pasar de etapa. */

/* Parámetros de MODE_ANGLE */
#define ANGLE_UPDATE_MS        50    /*!< Cada cuánto se actualiza el pitch (el ángulo cambia mucho más lento que la señal, así que no hace falta más rápido) */
#define ANGLE_I2C_FREQ_HZ      100000 /*!< Velocidad del I2C del acelerómetro. 100 kHz en vez de los 400 kHz por defecto: tolera mejor cables largos o soldaduras flojas */
#define ANGLE_RETRY_MS         200   /*!< Espera tras una lectura fallida antes de reintentar */
#define ANGLE_FAIL_LIMIT       3     /*!< Lecturas fallidas seguidas tras las que el pitch pasa a NaN */
#define PLOT_DECIMATION        14   /*!< Se imprime 1 línea cada N muestras (14 x 0.7ms ~ 10ms, ~100 líneas/s), con la detección "pegada": si hubo gota en las N muestras, sale DETECTION_VALUE en esa línea. La línea "500,0,0,-12.3" ocupa ~15 caracteres, ~1.3ms a 115200 baudios, así que la UART queda con mucho margen */
#define PLOT_QUEUE_LEN         32    /*!< Líneas que puede acumular la cola entre ADCTask y la tarea que imprime. Si se llena, se descartan líneas del plotter (la detección no se ve afectada) */
#define ANGLE_AVG_SAMPLES      32    /*!< Lecturas del acelerómetro que se promedian por cada ángulo, para filtrar vibraciones */
#define RAD_TO_DEG             57.29578f
#define ANGLE_USE_ANGZ         1     /*!< 1: el ángulo que se registra es angZ (ángulo del eje Z respecto de la horizontal, el de prueba_adxl345) menos ANGLE_OFFSET_DEG; 0: el pitch de antes */
#define ANGLE_OFFSET_DEG       11.1f /*!< angZ (varía entre 10.6 y 11.7 en reposo, punto medio 11.1) con el equipo en la posición de referencia (cero) dentro del gabinete: leerlo en prueba_adxl345 y ponerlo acá. Se resta para que en esa posición el ángulo sea 0 */

#define ADC_CHANNEL          CH2   /*!< Canal ADC donde está conectado Vo del OPT101 (GPIO_2) */
#define SAMPLE_PERIOD_US     700  /*!< Período fijo entre muestras, en microsegundos. Tiene que
                                        quedar entre 0.5 y 1 ms para resolver bien el pico de la
                                        gota - queda en el techo de ese rango para tener más
                                        margen posible contra el watchdog */
#define CUTOFF_HIGHPASS_HZ   60.0f  /*!< Corte del pasaaltos. Con el OPT101 en 100k/3.9nF (ver
                                        ../analisis_opt101) solo ~10% de la energía de la gota
                                        está por debajo de 85Hz, y el ruido de ahí para abajo es
                                        deriva y zumbido de red. 60Hz saca la deriva y la continua
                                        del LED (~900mV) y atenúa los 50Hz de la red (ganancia
                                        ~0.4 con ORDER_4), sin recortar la gota: en la prueba con
                                        20 gotas reales, cualquier corte entre 10 y 60Hz da la
                                        misma detectabilidad (la peor gota queda ~25 veces sobre
                                        el máximo del ruido) */
#define CUTOFF_LOWPASS_HZ    300.0f /*!< Corte del pasabajos. La energía de la gota llega hasta
                                        ~300Hz (57% en 150-250Hz, 12% en 250-340Hz), bastante más
                                        arriba que con la ganancia anterior. Más arriba no hay
                                        nada que aportar: el antialias analógico (RC 4k7+100nF,
                                        fc ~339Hz) y el OPT101 (fc ~408Hz) ya atenúan. Bajarlo a
                                        200Hz o menos empeora la detección (peor gota / ruido de
                                        ~25 a ~17, y a 150Hz ~10) porque se recorta la gota */
#define FILTER_ORDER         ORDER_4 /*!< Orden del Butterworth (ORDER_2, 4, 6 u 8), para ambos
                                        filtros. Con ORDER_2 (12dB/octava) la transición entre
                                        "pasa" y "corta" es muy suave; un orden mayor cae más
                                        brusco más allá del corte */

/* Parámetros de MODE_DETECTOR (los valores salen del análisis de ../analisis_opt101) */
#define DETECTION_VALUE      500    /*!< Valor que se transmite en el segundo valor de la línea en la muestra en que se detecta una gota (0 el resto del tiempo) */
#define XCORR_THRESHOLD      110.0f /*!< Umbral de la correlación, en mV de la señal filtrada. Tiene que estar por encima del
                                        ruido más grande (47: perturbaciones reales de la toma de validación; sin goteo
                                        llega a 31) y por debajo de la gota más débil (254 en la correlación). Es el punto
                                        medio entre las dos, en veces: 2.3 veces el ruido máximo y 2.3 veces por debajo de
                                        la gota más débil. Cualquier valor entre 60 y 250 da los mismos resultados (35 de 35
                                        y 13 de 13 gotas, 0 falsos). OJO: la regla de 12 desvíos del ruido (44) queda corta:
                                        da un falso positivo en la toma de validación. Ver
                                        ../analisis_opt101/analizar_correlacion.py */
#define XCORR_PEAK_WINDOW_MS 25     /*!< Tiempo que se espera, tras pasar el umbral, quedándose con el máximo de la
                                        correlación. La plantilla es larga (45ms) y oscila (pre-pico, valle, pico, rebote):
                                        la correlación pasa el umbral ~16ms ANTES de su máximo, así que la ventana tiene que
                                        cubrir ese tramo para llegar al máximo. 25ms deja margen */
#define XCORR_REFRACTORY_MS  100    /*!< Tiempo que se ignora la señal tras detectar una gota (para no contar dos veces la misma) */

#define MS_TO_SAMPLES(ms)    (((ms) * 1000UL) / SAMPLE_PERIOD_US)
/*==================[internal data definition]===============================*/
static TaskHandle_t adc_task_handle = NULL;

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
#if TEST_MODE != MODE_RAW
static float FilterSample(float raw_mV);
#endif

/*==================[internal functions definition]===========================*/

/** @brief Función invocada en la interrupción del timer. Solo notifica a ADCTask. */
static void FuncTimerSample(void *param) {
    vTaskNotifyGiveFromISR(adc_task_handle, pdFALSE);
}

#if TEST_MODE != MODE_RAW
/** @brief Pasa una muestra por el pasabanda (pasabajos y después pasaaltos, que saca la
 *  continua). LowPassFilter/HiPassFilter guardan el
 *  estado entre llamadas, así que llamarlos con signal_lenght=1 (una muestra por vez) funciona
 *  como filtro en tiempo real. La salida queda centrada en 0 (con la gota como pico positivo o
 *  negativo), no es más una tensión absoluta. */
static float FilterSample(float raw_mV) {
    float lowpassed_mV, filtered_mV;
    LowPassFilter(&raw_mV, &lowpassed_mV, 1);
    HiPassFilter(&lowpassed_mV, &filtered_mV, 1);
    return filtered_mV;
}
#endif

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
        /* @note Se imprime con decimales (%.2f) a propósito: después de sacar la continua, lo
         * que queda puede ser de apenas unos pocos mV (o menos) de amplitud. Redondear a
         * entero ahí destruye la resolución justo donde más importa. */
        float filtered_mV = FilterSample((float)voltage_mV);

        printf("%.2f\r\n", filtered_mV);
#elif TEST_MODE == MODE_DETECTOR
        float filtered_mV = FilterSample((float)voltage_mV);

        bool drop_detected = XCorrProcess(filtered_mV, NULL);

        /* Se transmite "filtrada,deteccion": la señal filtrada y un segundo valor que es
         * DETECTION_VALUE en la muestra en que se detecta una gota y 0 el resto del tiempo (el
         * serial plotter lo grafica como una segunda serie). Se imprime con 1 decimal (no 2) para
         * que la línea, ahora con el segundo valor, siga entrando en SAMPLE_PERIOD_US a 115200
         * baudios (~87 us por caracter). No se imprime ningún texto extra: rompería el formato. */
        printf("%.1f,%d\r\n", filtered_mV, drop_detected ? DETECTION_VALUE : 0);
#elif TEST_MODE == MODE_ANGLE
        /* Igual que MODE_DETECTOR, pero no se imprime acá: ADCTask arma una línea cada
         * PLOT_DECIMATION muestras y se la pasa a PlotTask por una cola, así printf (lento a
         * 115200 baudios) nunca atrasa el muestreo ni el detector. La detección queda "pegada":
         * si alguna de las muestras salteadas detectó una gota, la línea sale con la detección. */
        static uint8_t decim_count = 0;
        static bool drop_pending = false;

        float filtered_mV = FilterSample((float)voltage_mV);
        if (XCorrProcess(filtered_mV, NULL)) {
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
/** @brief Inicializa el acelerómetro ADXL345 (busca el chip en 0x53 y 0x1D).
 *  @return true si el chip respondió */
static bool AngleSensorInit(void) {
    return ADXL345_Init();
}

/** @brief Lee la aceleración de los tres ejes en g, promediando ANGLE_AVG_SAMPLES lecturas para
 *  filtrar vibraciones. Con el equipo quieto, el eje que apunta hacia arriba da +1 g.
 *  @return false si alguna lectura falló */
static bool AngleSensorRead(float *ax, float *ay, float *az) {
    float sx = 0, sy = 0, sz = 0;
    for (int i = 0; i < ANGLE_AVG_SAMPLES; i++) {
        float x, y, z;
        if (!ADXL345_ReadAccel(&x, &y, &z)) {
            return false;
        }
        sx += x;
        sy += y;
        sz += z;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    *ax = sx / ANGLE_AVG_SAMPLES;
    *ay = sy / ANGLE_AVG_SAMPLES;
    *az = sz / ANGLE_AVG_SAMPLES;
    return true;
}

/** @brief Tarea que lee el acelerómetro cada ANGLE_UPDATE_MS y guarda el pitch (en grados, con
 *  signo) en pitch_deg. Corre con menos prioridad que ADCTask, así la lectura por I2C no atrasa
 *  el muestreo del ADC. */
static void AngleTask(void *pvParameter) {
    if (!I2C_initialize(ANGLE_I2C_FREQ_HZ)) {
        printf("Error: no se pudo inicializar el bus I2C (SDA=GPIO_21, SCL=GPIO_7)\r\n");
        vTaskDelete(NULL);
    }
    if (!AngleSensorInit()) {
        printf("Error: el acelerometro no responde. Revisar SDA/SCL, alimentacion y, en el ADXL345, CS a 3V3 y SDO a GND.\r\n");
        vTaskDelete(NULL);
    }

    uint8_t failures = 0;   /* lecturas fallidas seguidas */
    while (true) {
        float ax, ay, az;
        if (AngleSensorRead(&ax, &ay, &az)) {
            failures = 0;
#if ANGLE_USE_ANGZ
            float norm = sqrtf(ax * ax + ay * ay + az * az);
            float s = (norm > 0.2f) ? az / norm : 0.0f;   // con menos de 0.2 g no hay gravedad con la que medir
            s = (s > 1.0f) ? 1.0f : (s < -1.0f) ? -1.0f : s;
            pitch_deg = (norm > 0.2f) ? asinf(s) * RAD_TO_DEG - ANGLE_OFFSET_DEG : NAN;
#else
            pitch_deg = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
#endif
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
 *  detección en DETECTION_VALUE o 0 y el pitch en grados con un decimal. */
static void PlotTask(void *pvParameter) {
    plot_line_t line;
    while (true) {
        xQueueReceive(plot_queue, &line, portMAX_DELAY);
        printf("%d,0,0,%.1f\r\n", line.drop ? DETECTION_VALUE : 0, pitch_deg);
    }
}
#endif

/*==================[external functions definition]==========================*/
void app_main(void) {
    analog_input_config_t adc_config = {
        .input = ADC_CHANNEL,
        .mode = ADC_SINGLE,
        .func_p = NULL,
        .param_p = NULL,
    };
    AnalogInputInit(&adc_config);

#if TEST_MODE != MODE_RAW
    LowPassInit(1000000.0f / SAMPLE_PERIOD_US, CUTOFF_LOWPASS_HZ, FILTER_ORDER);
    HiPassInit(1000000.0f / SAMPLE_PERIOD_US, CUTOFF_HIGHPASS_HZ, FILTER_ORDER);
#endif

#if TEST_MODE == MODE_DETECTOR || TEST_MODE == MODE_ANGLE
    xcorr_config_t xcorr_config = {
        .template_signal = DROP_TEMPLATE,
        .template_len = DROP_TEMPLATE_LEN,
        .threshold = XCORR_THRESHOLD,
        .peak_window = MS_TO_SAMPLES(XCORR_PEAK_WINDOW_MS),
        .refractory = MS_TO_SAMPLES(XCORR_REFRACTORY_MS),
    };
    XCorrInit(&xcorr_config);
#endif

#if TEST_MODE == MODE_ANGLE
    plot_queue = xQueueCreate(PLOT_QUEUE_LEN, sizeof(plot_line_t));   /* tiene que existir antes de que ADCTask empiece a usarla */
    xTaskCreate(&AngleTask, "ACEL_ANGLE", 4096, NULL, 3, NULL);
    /* PlotTask con la prioridad de IDLE (0): printf espera activamente a la salida serie sin ceder
     * la CPU, y si el monitor del PC se atrasa esa espera puede durar segundos. Con prioridad mayor
     * IDLE no llegaba a correr y saltaba el watchdog de tareas; en 0 comparte el tiempo con IDLE. */
    xTaskCreate(&PlotTask, "PLOT", 4096, NULL, tskIDLE_PRIORITY, NULL);
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
